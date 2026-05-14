#include "flow_field_builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <gdal_alg.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include "cartesian3.h"
#include "stb_image_write.h"

namespace {

constexpr int kMaxSeedTextureDimension = 65536;
constexpr double kPi = 3.14159265358979323846;

enum class ProjectionTargetType
{
    Mapbox,
    Cesium,
    Epsg
};

struct ProjectionTarget
{
    ProjectionTargetType type = ProjectionTargetType::Epsg;
    int epsg = 4326;
    int components = 2;
};

struct GdalDatasetDeleter
{
    void operator()(GDALDataset* dataset) const
    {
        if (dataset != nullptr)
        {
            GDALClose(dataset);
        }
    }
};

using GdalDatasetPtr = std::unique_ptr<GDALDataset, GdalDatasetDeleter>;

struct CoordinateTransformationDeleter
{
    void operator()(OGRCoordinateTransformation* transformation) const
    {
        if (transformation != nullptr)
        {
            OGRCoordinateTransformation::DestroyCT(transformation);
        }
    }
};

using CoordinateTransformationPtr = std::unique_ptr<OGRCoordinateTransformation, CoordinateTransformationDeleter>;

void validateOutputName(const std::string& outputName)
{
    const std::filesystem::path namePath(outputName);
    if (outputName.empty()
        || outputName.find('\\') != std::string::npos
        || namePath.is_absolute()
        || namePath.has_parent_path()
        || outputName == "."
        || outputName == "..")
    {
        throw std::invalid_argument("output name must be a non-empty file stem");
    }
}

void packFloat(float value, unsigned char* target)
{
    static_assert(sizeof(float) == 4);
    std::memcpy(target, &value, sizeof(float));
}

void packSeedCoordinate(std::size_t x, std::size_t y, unsigned char* target)
{
    target[0] = static_cast<unsigned char>((x >> 8U) & 0xffU);
    target[1] = static_cast<unsigned char>(x & 0xffU);
    target[2] = static_cast<unsigned char>((y >> 8U) & 0xffU);
    target[3] = static_cast<unsigned char>(y & 0xffU);
}

std::array<double, 4> computeExtent(const std::vector<double>& xs, const std::vector<double>& ys)
{
    const auto [minX, maxX] = std::minmax_element(xs.begin(), xs.end());
    const auto [minY, maxY] = std::minmax_element(ys.begin(), ys.end());
    return {*minX, *minY, *maxX, *maxY};
}

ProjectionTarget parseProjectionTarget(const std::string& target)
{
    if (target == "mapbox")
    {
        return {ProjectionTargetType::Mapbox, 4326, 2};
    }
    if (target == "cesium")
    {
        return {ProjectionTargetType::Cesium, 4326, 3};
    }
    if (target.empty())
    {
        throw std::invalid_argument("projection target must not be empty");
    }

    std::size_t parsedCharacters = 0;
    const int epsg = std::stoi(target, &parsedCharacters);
    if (parsedCharacters != target.size() || epsg <= 0)
    {
        throw std::invalid_argument("projection target must be 'mapbox', 'cesium', or an EPSG code");
    }
    return {ProjectionTargetType::Epsg, epsg, 2};
}

void importEpsg(OGRSpatialReference& reference, int epsg, const char* label)
{
    if (epsg <= 0 || reference.importFromEPSG(epsg) != OGRERR_NONE)
    {
        throw std::invalid_argument(std::string("invalid ") + label + " EPSG code");
    }
    reference.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
}

CoordinateTransformationPtr createTransformation(int sourceEpsg, const ProjectionTarget& target)
{
    OGRSpatialReference sourceReference;
    OGRSpatialReference targetReference;
    importEpsg(sourceReference, sourceEpsg, "source");
    importEpsg(targetReference, target.epsg, "target");

    auto* transformation = OGRCreateCoordinateTransformation(&sourceReference, &targetReference);
    if (transformation == nullptr)
    {
        throw std::runtime_error("failed to create projection transformation");
    }
    return CoordinateTransformationPtr(transformation);
}

void transformCoordinate(
        OGRCoordinateTransformation& transformation,
        ProjectionTargetType targetType,
        double& x,
        double& y,
        double& z)
{
    if (!transformation.Transform(1, &x, &y))
    {
        throw std::runtime_error("failed to transform projection coordinate");
    }

    if (targetType == ProjectionTargetType::Mapbox)
    {
        x = (180.0 + x) / 360.0;
        y = (180.0 - (180.0 / kPi * std::log(std::tan(kPi / 4.0 + y * kPi / 360.0)))) / 360.0;
        z = 0.0;
    }
    else if (targetType == ProjectionTargetType::Cesium)
    {
        Cartesian3 cartesian;
        Cartesian3::fromDegrees(x, y, 0.0, &cartesian);
        x = cartesian.x;
        y = cartesian.y;
        z = cartesian.z;
    }
    else
    {
        z = 0.0;
    }

    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
    {
        throw std::runtime_error("projection coordinate is not finite");
    }
}

void packProjectedComponents(double x, double y, double z, int components, unsigned char* target)
{
    packFloat(static_cast<float>(x), target);
    packFloat(static_cast<float>(y), target + 4U);
    if (components == 3)
    {
        packFloat(static_cast<float>(z), target + 8U);
    }
}

} // namespace

FlowFieldBuilder::FlowFieldBuilder()
        : m_context()
{
}

FlowFieldBuilder::FlowFieldBuilder(std::filesystem::path outputDirectory)
        : m_outputDirectory(std::move(outputDirectory)), m_context()
{
}

void FlowFieldBuilder::setOutputDirectory(std::filesystem::path outputDirectory)
{
    m_outputDirectory = std::move(outputDirectory);
}

void FlowFieldBuilder::setTextureSize(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        throw std::invalid_argument("texture size must be positive");
    }
    if (width > kMaxSeedTextureDimension || height > kMaxSeedTextureDimension)
    {
        throw std::invalid_argument("texture size exceeds seed texture coordinate range");
    }
    if (width > std::numeric_limits<int>::max() / 2)
    {
        throw std::invalid_argument("flow texture width is too large");
    }

    m_textureWidth = width;
    m_textureHeight = height;
    if (m_domainMode != DomainMode::None)
    {
        rebuildDomain();
    }
}

void FlowFieldBuilder::setProjectionTextureSize(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        throw std::invalid_argument("projection texture size must be positive");
    }
    if (width > std::numeric_limits<int>::max() / 3)
    {
        throw std::invalid_argument("projection texture width is too large");
    }

    m_projectionTextureWidth = width;
    m_projectionTextureHeight = height;
}

void FlowFieldBuilder::setSourceEpsg(int epsg)
{
    OGRSpatialReference reference;
    importEpsg(reference, epsg, "source");
    m_sourceEpsg = epsg;
}

void FlowFieldBuilder::setQuikgridOptions(const XpandOptions& options)
{
    m_options = options;
    if (m_domainMode != DomainMode::None && m_textureWidth > 0 && m_textureHeight > 0)
    {
        rebuildDomain();
    }
}

void FlowFieldBuilder::setDomainFromArrays(const double* xs, const double* ys, std::size_t count)
{
    setDomainArrays(xs, ys, count);
    m_vectorPath.clear();
    m_domainMode = DomainMode::Arrays;
    m_domainMask.clear();
    if (m_textureWidth > 0 && m_textureHeight > 0)
    {
        rebuildDomain();
    }
}

void FlowFieldBuilder::setDomainFromArrays(const double* xs, const double* ys, std::size_t count, int sourceEpsg)
{
    setDomainFromArrays(xs, ys, count);
    setSourceEpsg(sourceEpsg);
}

void FlowFieldBuilder::setDomainFromVector(
        const double* xs,
        const double* ys,
        std::size_t count,
        const std::filesystem::path& vectorPath)
{
    if (vectorPath.empty())
    {
        throw std::invalid_argument("vector path must not be empty");
    }

    setDomainArrays(xs, ys, count);
    m_vectorPath = vectorPath;
    m_domainMode = DomainMode::Vector;
    m_domainMask.clear();
    if (m_textureWidth > 0 && m_textureHeight > 0)
    {
        rebuildDomain();
    }
}

void FlowFieldBuilder::setDomainFromVector(
        const double* xs,
        const double* ys,
        std::size_t count,
        const std::filesystem::path& vectorPath,
        int sourceEpsg)
{
    setDomainFromVector(xs, ys, count, vectorPath);
    setSourceEpsg(sourceEpsg);
}

void FlowFieldBuilder::buildUvTexture(
        const float* us,
        const float* vs,
        std::size_t count,
        const std::string& outputName)
{
    validateReadyToBuild(count, outputName);
    if (us == nullptr || vs == nullptr)
    {
        throw std::invalid_argument("us and vs must not be null");
    }

    const std::size_t gridCellCount = static_cast<std::size_t>(m_textureWidth) * static_cast<std::size_t>(m_textureHeight);
    std::vector<float> uGrid(gridCellCount, m_options.undefinedValue);
    std::vector<float> vGrid(gridCellCount, m_options.undefinedValue);
    m_context.interpolate(us, count, uGrid.data(), uGrid.size());
    m_context.interpolate(vs, count, vGrid.data(), vGrid.size());

    writeTextures(uGrid, vGrid, outputName);
}

void FlowFieldBuilder::buildProjectionTexture(const std::string& target, const std::string& outputName)
{
    validateReadyToBuildProjection(target, outputName);

    const auto projectionTarget = parseProjectionTarget(target);
    auto transformation = createTransformation(m_sourceEpsg, projectionTarget);

    std::array<std::array<double, 2>, 4> corners {{
            {m_domainExtent[0], m_domainExtent[1]},
            {m_domainExtent[0], m_domainExtent[3]},
            {m_domainExtent[2], m_domainExtent[1]},
            {m_domainExtent[2], m_domainExtent[3]},
    }};
    m_projectionExtent = {
            std::numeric_limits<double>::max(),
            std::numeric_limits<double>::max(),
            std::numeric_limits<double>::lowest(),
            std::numeric_limits<double>::lowest(),
    };
    for (auto& corner : corners)
    {
        double z = 0.0;
        transformCoordinate(*transformation, projectionTarget.type, corner[0], corner[1], z);
        m_projectionExtent[0] = std::min(m_projectionExtent[0], corner[0]);
        m_projectionExtent[1] = std::min(m_projectionExtent[1], corner[1]);
        m_projectionExtent[2] = std::max(m_projectionExtent[2], corner[0]);
        m_projectionExtent[3] = std::max(m_projectionExtent[3], corner[1]);
    }

    transformation = createTransformation(m_sourceEpsg, projectionTarget);

    const std::size_t logicalWidth = static_cast<std::size_t>(m_projectionTextureWidth);
    const std::size_t logicalHeight = static_cast<std::size_t>(m_projectionTextureHeight);
    const std::size_t components = static_cast<std::size_t>(projectionTarget.components);
    std::vector<unsigned char> texture(logicalWidth * logicalHeight * components * 4U, 0U);

    const double dx = (m_domainExtent[2] - m_domainExtent[0]) / static_cast<double>(m_projectionTextureWidth);
    const double dy = (m_domainExtent[3] - m_domainExtent[1]) / static_cast<double>(m_projectionTextureHeight);
    for (int yIndex = 0; yIndex < m_projectionTextureHeight; ++yIndex)
    {
        for (int xIndex = 0; xIndex < m_projectionTextureWidth; ++xIndex)
        {
            double x = m_domainExtent[0] + 0.5 * dx + static_cast<double>(xIndex) * dx;
            double y = m_domainExtent[1] + 0.5 * dy + static_cast<double>(yIndex) * dy;
            double z = 0.0;
            transformCoordinate(*transformation, projectionTarget.type, x, y, z);

            const std::size_t pngRow = static_cast<std::size_t>(m_projectionTextureHeight - yIndex - 1);
            const std::size_t base = pngRow * logicalWidth * components * 4U
                    + static_cast<std::size_t>(xIndex) * components * 4U;
            packProjectedComponents(x, y, z, projectionTarget.components, texture.data() + base);
        }
    }

    std::filesystem::create_directories(m_outputDirectory);
    const auto outputPath = projectionTexturePath(outputName).string();
    if (!stbi_write_png(
                outputPath.c_str(),
                m_projectionTextureWidth * projectionTarget.components,
                m_projectionTextureHeight,
                4,
                texture.data(),
                0))
    {
        throw std::runtime_error("failed to write projection texture");
    }
}

std::filesystem::path FlowFieldBuilder::uvTexturePath(const std::string& outputName) const
{
    validateOutputName(outputName);
    return m_outputDirectory / ("uv_" + outputName + ".png");
}

std::filesystem::path FlowFieldBuilder::seedTexturePath(const std::string& outputName) const
{
    validateOutputName(outputName);
    return m_outputDirectory / ("seed_" + outputName + ".png");
}

std::filesystem::path FlowFieldBuilder::projectionTexturePath(const std::string& outputName) const
{
    validateOutputName(outputName);
    return m_outputDirectory / ("projection_" + outputName + ".png");
}

void FlowFieldBuilder::setDomainArrays(const double* xs, const double* ys, std::size_t count)
{
    if (xs == nullptr || ys == nullptr)
    {
        throw std::invalid_argument("xs and ys must not be null");
    }
    if (count < 3)
    {
        throw std::invalid_argument("domain requires at least three points");
    }
    if (count > static_cast<std::size_t>(std::numeric_limits<long>::max()))
    {
        throw std::invalid_argument("point count exceeds quikgrid index range");
    }

    m_xs.assign(xs, xs + count);
    m_ys.assign(ys, ys + count);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (!std::isfinite(m_xs[index]) || !std::isfinite(m_ys[index]))
        {
            throw std::invalid_argument("domain coordinates must be finite");
        }
    }

    m_domainExtent = computeExtent(m_xs, m_ys);
    if (!(m_domainExtent[2] > m_domainExtent[0]) || !(m_domainExtent[3] > m_domainExtent[1]))
    {
        throw std::invalid_argument("domain extent must have positive area");
    }
}

void FlowFieldBuilder::rebuildDomain()
{
    if (m_textureWidth <= 0 || m_textureHeight <= 0)
    {
        throw std::logic_error("texture size must be set before domain");
    }
    if (m_xs.empty() || m_ys.empty())
    {
        throw std::logic_error("domain coordinates are not set");
    }

    rebuildGridCoordinates();
    m_context.reset(
            m_xs.data(),
            m_ys.data(),
            m_xs.size(),
            m_gridXs.data(),
            m_gridXs.size(),
            m_gridYs.data(),
            m_gridYs.size(),
            m_options);

    const std::size_t cellCount = static_cast<std::size_t>(m_textureWidth) * static_cast<std::size_t>(m_textureHeight);
    m_domainMask.assign(cellCount, 1U);
    if (m_domainMode == DomainMode::Vector)
    {
        rebuildVectorMask();
    }
}

void FlowFieldBuilder::rebuildGridCoordinates()
{
    m_gridXs.resize(static_cast<std::size_t>(m_textureWidth));
    m_gridYs.resize(static_cast<std::size_t>(m_textureHeight));

    const double xMin = m_domainExtent[0];
    const double yMin = m_domainExtent[1];
    const double xMax = m_domainExtent[2];
    const double yMax = m_domainExtent[3];
    for (int index = 0; index < m_textureWidth; ++index)
    {
        const double ratio = m_textureWidth == 1 ? 0.5 : static_cast<double>(index) / static_cast<double>(m_textureWidth - 1);
        m_gridXs[static_cast<std::size_t>(index)] = static_cast<float>(xMin + (xMax - xMin) * ratio);
    }
    for (int index = 0; index < m_textureHeight; ++index)
    {
        const double ratio = m_textureHeight == 1 ? 0.5 : static_cast<double>(index) / static_cast<double>(m_textureHeight - 1);
        m_gridYs[static_cast<std::size_t>(index)] = static_cast<float>(yMin + (yMax - yMin) * ratio);
    }
}

void FlowFieldBuilder::rebuildVectorMask()
{
    GDALAllRegister();

    GdalDatasetPtr vectorDataset(static_cast<GDALDataset*>(
            GDALOpenEx(m_vectorPath.string().c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr)));
    if (vectorDataset == nullptr)
    {
        throw std::runtime_error("failed to open vector domain");
    }

    OGRLayer* layer = vectorDataset->GetLayer(0);
    if (layer == nullptr)
    {
        throw std::runtime_error("vector domain does not contain a layer");
    }

    GDALDriver* memoryDriver = GetGDALDriverManager()->GetDriverByName("MEM");
    if (memoryDriver == nullptr)
    {
        throw std::runtime_error("GDAL MEM driver is not available");
    }

    GdalDatasetPtr rasterDataset(memoryDriver->Create("", m_textureWidth, m_textureHeight, 1, GDT_Byte, nullptr));
    if (rasterDataset == nullptr)
    {
        throw std::runtime_error("failed to create in-memory domain mask");
    }
    if (rasterDataset->GetRasterBand(1)->Fill(0.0) != CE_None)
    {
        throw std::runtime_error("failed to initialize in-memory domain mask");
    }

    const double geoTransform[6] {
            m_domainExtent[0],
            (m_domainExtent[2] - m_domainExtent[0]) / static_cast<double>(m_textureWidth),
            0.0,
            m_domainExtent[3],
            0.0,
            -(m_domainExtent[3] - m_domainExtent[1]) / static_cast<double>(m_textureHeight)};
    if (rasterDataset->SetGeoTransform(const_cast<double*>(geoTransform)) != CE_None)
    {
        throw std::runtime_error("failed to set domain mask transform");
    }

    if (const OGRSpatialReference* spatialReference = layer->GetSpatialRef())
    {
        char* wkt = nullptr;
        if (spatialReference->exportToWkt(&wkt) == OGRERR_NONE)
        {
            rasterDataset->SetProjection(wkt);
            CPLFree(wkt);
        }
    }

    int bandList = 1;
    OGRLayerH layerHandle = OGRLayer::ToHandle(layer);
    double burnValue = 1.0;
    if (GDALRasterizeLayers(
                GDALDataset::ToHandle(rasterDataset.get()),
                1,
                &bandList,
                1,
                &layerHandle,
                nullptr,
                nullptr,
                &burnValue,
                nullptr,
                nullptr,
                nullptr) != CE_None)
    {
        throw std::runtime_error("failed to rasterize vector domain");
    }

    std::vector<unsigned char> topDownMask(static_cast<std::size_t>(m_textureWidth) * static_cast<std::size_t>(m_textureHeight), 0U);
    if (rasterDataset->GetRasterBand(1)->RasterIO(
                GF_Read,
                0,
                0,
                m_textureWidth,
                m_textureHeight,
                topDownMask.data(),
                m_textureWidth,
                m_textureHeight,
                GDT_Byte,
                0,
                0,
                nullptr) != CE_None)
    {
        throw std::runtime_error("failed to read vector domain mask");
    }

    for (int row = 0; row < m_textureHeight; ++row)
    {
        for (int column = 0; column < m_textureWidth; ++column)
        {
            const std::size_t source = static_cast<std::size_t>(row) * static_cast<std::size_t>(m_textureWidth)
                    + static_cast<std::size_t>(column);
            const std::size_t target = static_cast<std::size_t>(m_textureHeight - row - 1) * static_cast<std::size_t>(m_textureWidth)
                    + static_cast<std::size_t>(column);
            m_domainMask[target] = topDownMask[source] == 0U ? 0U : 1U;
        }
    }
}

void FlowFieldBuilder::validateReadyToBuild(std::size_t count, const std::string& outputName) const
{
    validateOutputName(outputName);
    if (m_outputDirectory.empty())
    {
        throw std::logic_error("output directory is not set");
    }
    if (m_textureWidth <= 0 || m_textureHeight <= 0)
    {
        throw std::logic_error("texture size is not set");
    }
    if (m_domainMode == DomainMode::None || m_xs.empty() || m_domainMask.empty())
    {
        throw std::logic_error("domain is not set");
    }
    if (count != m_xs.size())
    {
        throw std::invalid_argument("velocity count must match domain point count");
    }
}

void FlowFieldBuilder::validateReadyToBuildProjection(const std::string& target, const std::string& outputName) const
{
    validateOutputName(outputName);
    if (target.empty())
    {
        throw std::invalid_argument("projection target must not be empty");
    }
    if (m_outputDirectory.empty())
    {
        throw std::logic_error("output directory is not set");
    }
    if (m_sourceEpsg <= 0)
    {
        throw std::logic_error("source EPSG is not set");
    }
    if (m_projectionTextureWidth <= 0 || m_projectionTextureHeight <= 0)
    {
        throw std::logic_error("projection texture size is not set");
    }
    if (m_domainMode == DomainMode::None || m_xs.empty())
    {
        throw std::logic_error("domain is not set");
    }
}

bool FlowFieldBuilder::isDefined(float value) const
{
    return std::isfinite(value) && value != m_options.undefinedValue;
}

bool FlowFieldBuilder::isValidCell(float u, float v, std::size_t index) const
{
    return m_domainMask[index] != 0U && isDefined(u) && isDefined(v);
}

void FlowFieldBuilder::writeTextures(
        const std::vector<float>& uGrid,
        const std::vector<float>& vGrid,
        const std::string& outputName) const
{
    std::filesystem::create_directories(m_outputDirectory);

    const std::size_t width = static_cast<std::size_t>(m_textureWidth);
    const std::size_t height = static_cast<std::size_t>(m_textureHeight);
    const std::size_t cellCount = width * height;
    if (uGrid.size() != cellCount || vGrid.size() != cellCount || m_domainMask.size() != cellCount)
    {
        throw std::logic_error("grid output size does not match texture size");
    }

    std::vector<std::array<std::size_t, 2>> validAddresses;
    validAddresses.reserve(cellCount);
    std::vector<unsigned char> validMask(cellCount, 0U);
    for (std::size_t y = 0; y < height; ++y)
    {
        for (std::size_t x = 0; x < width; ++x)
        {
            const std::size_t index = y * width + x;
            if (isValidCell(uGrid[index], vGrid[index], index))
            {
                validMask[index] = 1U;
                validAddresses.push_back({x, y});
            }
        }
    }
    if (validAddresses.empty())
    {
        throw std::runtime_error("no valid flow cells were produced");
    }

    std::vector<unsigned char> uvTexture(width * height * 8U, 0U);
    std::vector<unsigned char> seedTexture(width * height * 4U, 0U);
    for (std::size_t y = 0; y < height; ++y)
    {
        for (std::size_t x = 0; x < width; ++x)
        {
            const std::size_t gridIndex = y * width + x;
            const std::size_t pngRow = height - y - 1U;
            const std::size_t uvBase = pngRow * width * 8U + x * 8U;
            const std::size_t seedBase = pngRow * width * 4U + x * 4U;

            if (validMask[gridIndex] != 0U)
            {
                packFloat(uGrid[gridIndex], uvTexture.data() + uvBase);
                packFloat(vGrid[gridIndex], uvTexture.data() + uvBase + 4U);
                packSeedCoordinate(x, y, seedTexture.data() + seedBase);
            }
            else
            {
                const auto& seed = validAddresses[gridIndex % validAddresses.size()];
                packSeedCoordinate(seed[0], seed[1], seedTexture.data() + seedBase);
            }
        }
    }

    const auto uvPath = uvTexturePath(outputName).string();
    const auto seedPath = seedTexturePath(outputName).string();
    if (!stbi_write_png(uvPath.c_str(), m_textureWidth * 2, m_textureHeight, 4, uvTexture.data(), 0))
    {
        throw std::runtime_error("failed to write uv texture");
    }
    if (!stbi_write_png(seedPath.c_str(), m_textureWidth, m_textureHeight, 4, seedTexture.data(), 0))
    {
        throw std::runtime_error("failed to write seed texture");
    }
}
