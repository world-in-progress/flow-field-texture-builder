//
// Created by Yucheng Soku on 2023/7/14.
//
#include "flow_field.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "cartesian3.h"
#include "flow_field_builder.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace {
std::filesystem::path resolvePath(const std::filesystem::path& base, const std::filesystem::path& path)
{
    return path.is_absolute() ? path : base / path;
}

struct FlowStepArrays
{
    std::vector<double> xs;
    std::vector<double> ys;
    std::vector<float> us;
    std::vector<float> vs;
};

std::array<double, 4> computeExtent(const std::vector<double>& xs, const std::vector<double>& ys)
{
    const auto [minX, maxX] = std::minmax_element(xs.begin(), xs.end());
    const auto [minY, maxY] = std::minmax_element(ys.begin(), ys.end());
    return {*minX, *minY, *maxX, *maxY};
}

std::array<int, 2> textureSizeFromResolution(const std::vector<double>& xs, const std::vector<double>& ys, double resolution)
{
    if (resolution <= 0.0)
    {
        throw std::invalid_argument("resolution must be positive");
    }
    if (resolution > 65536.0)
    {
        throw std::invalid_argument("resolution exceeds seed texture coordinate range");
    }

    const auto extent = computeExtent(xs, ys);
    const double dx = extent[2] - extent[0];
    const double dy = extent[3] - extent[1];
    const double longestSide = std::max(dx, dy);
    if (!(longestSide > 0.0))
    {
        throw std::invalid_argument("flow step extent must have positive area");
    }

    const double cellSize = longestSide / resolution;
    return {
            std::max(1, static_cast<int>(std::ceil(dx / cellSize))),
            std::max(1, static_cast<int>(std::ceil(dy / cellSize)))};
}

FlowStepArrays readFlowStep(
        const std::filesystem::path& path,
        int headingCount,
        int columnCount,
        const int attributeIndex[4],
        const double boundary[4])
{
    std::ifstream fs(path);
    if (!fs)
    {
        throw std::runtime_error("failed to open flow step file: " + path.string());
    }

    int pointCount = 0;
    fs >> pointCount;
    if (pointCount <= 0)
    {
        throw std::invalid_argument("flow step point count must be positive");
    }

    std::string heading;
    for (int i = 0; i < headingCount; ++i)
    {
        fs >> heading;
    }

    FlowStepArrays arrays;
    arrays.xs.reserve(static_cast<std::size_t>(pointCount));
    arrays.ys.reserve(static_cast<std::size_t>(pointCount));
    arrays.us.reserve(static_cast<std::size_t>(pointCount));
    arrays.vs.reserve(static_cast<std::size_t>(pointCount));

    double values[4] {};
    double ignored = 0.0;
    for (int row = 0; row < pointCount; ++row)
    {
        for (int column = 0; column < columnCount; ++column)
        {
            if (column == attributeIndex[0]) fs >> values[0];
            else if (column == attributeIndex[1]) fs >> values[1];
            else if (column == attributeIndex[2]) fs >> values[2];
            else if (column == attributeIndex[3]) fs >> values[3];
            else fs >> ignored;
        }

        if (!fs)
        {
            throw std::runtime_error("failed to parse flow step file: " + path.string());
        }
        if (values[0] < boundary[0] || values[0] > boundary[2] || values[1] < boundary[1] || values[1] > boundary[3])
        {
            continue;
        }
        arrays.xs.push_back(values[0]);
        arrays.ys.push_back(values[1]);
        arrays.us.push_back(static_cast<float>(values[2]));
        arrays.vs.push_back(static_cast<float>(values[3]));
    }

    if (arrays.xs.size() < 3)
    {
        throw std::invalid_argument("flow step has fewer than three in-bound points");
    }
    return arrays;
}
}


void FlowField::updateFlowBoundary(double u, double v)
{
    // update uMin
    m_flowVelocityRange[0] = u < m_flowVelocityRange[0] ? u : m_flowVelocityRange[0];
    // update uMax
    m_flowVelocityRange[2] = u > m_flowVelocityRange[2] ? u : m_flowVelocityRange[2];
    // update vMin
    m_flowVelocityRange[1] = v < m_flowVelocityRange[1] ? v : m_flowVelocityRange[1];
    // update vMax
    m_flowVelocityRange[3] = v > m_flowVelocityRange[3] ? v : m_flowVelocityRange[3];
}


bool FlowField::toTextureGridUv(ScatData& sc_u, ScatData& sc_v, SurfaceGrid& grd_u, SurfaceGrid& grd_v, const char* textureFile, const char* maskFile, const char* poolFile)
{
    std::vector<size_t> validAddress;
    std::vector<char> texture, mask, rebirthPool;
    texture.resize(grd_u.xsize() * 2 * grd_u.ysize() * 4, 0);
    mask.resize(grd_u.xsize() * grd_u.ysize() * 4, 0);

    double u, v;
    char r, g, b, a;
    size_t index;
    Texel rgba_u, rgba_v;
    int nx = grd_u.xsize();
    int ny = grd_u.ysize();
    bool bmpMaskPass = true;

    // Test valid addresses
    for (size_t iy = 0; iy < ny; ++iy)
    {
        for (size_t ix = 0; ix < nx; ++ix)
        {

            u = (grd_u.z(ix, iy) - m_flowVelocityRange[0]) / (m_flowVelocityRange[2] - m_flowVelocityRange[0]);
            v = (grd_v.z(ix, iy) - m_flowVelocityRange[1]) / (m_flowVelocityRange[3] - m_flowVelocityRange[1]);

            bmpMaskPass = m_bmpMask[(ny - iy - 1) * nx * 3 + ix * 3];

            if (bmpMaskPass && u != m_outlier && u >= 0.0 && v != m_outlier && v >= 0.0)
            {
                validAddress.emplace_back(ix);
                validAddress.emplace_back(iy);
            }
            else m_bmpMask[(ny - iy - 1) * nx * 3 + ix * 3] = 0;
        }
    }

    for (size_t iy = 0; iy < ny; ++iy)
    {
        for (size_t ix = 0; ix < nx; ++ix)
        {
            rgba_u.f32 = 0.0, rgba_v.f32 = 0.0;
            r = g = b = a = 0;
            u = (grd_u.z(ix, iy) - m_flowVelocityRange[0]) / (m_flowVelocityRange[2] - m_flowVelocityRange[0]);
            v = (grd_v.z(ix, iy) - m_flowVelocityRange[1]) / (m_flowVelocityRange[3] - m_flowVelocityRange[1]);

            bmpMaskPass = m_bmpMask[(ny - iy - 1) * nx * 3 + ix * 3];

            // Mask pass
            if (bmpMaskPass)
            {
                rgba_u.f32 = u, rgba_v.f32 = v;

                r = int(ix) >> 8;
                g = int(ix) & 0xff;
                b = int(iy) >> 8;
                a = int(iy) & 0xff;
                mask[(ny - iy - 1) * nx * 4 + ix * 4 + 0] = r;
                mask[(ny - iy - 1) * nx * 4 + ix * 4 + 1] = g;
                mask[(ny - iy - 1) * nx * 4 + ix * 4 + 2] = b;
                mask[(ny - iy - 1) * nx * 4 + ix * 4 + 3] = a;
            }
            else
            {
                index = rand() % (validAddress.size() / 2);
                r = int(validAddress[index * 2 + 0]) >> 8;
                g = int(validAddress[index * 2 + 0]) & 0xff;
                b = int(validAddress[index * 2 + 1]) >> 8;
                a = int(validAddress[index * 2 + 1]) & 0xff;
                mask[(ny - iy - 1) * nx * 4 + ix * 4 + 0] = r;
                mask[(ny - iy - 1) * nx * 4 + ix * 4 + 1] = g;
                mask[(ny - iy - 1) * nx * 4 + ix * 4 + 2] = b;
                mask[(ny - iy - 1) * nx * 4 + ix * 4 + 3] = a;
            }

//                mask[(ny - iy - 1) * nx * 4 + ix * 4 + 0] = isFlow;
//                mask[(ny - iy - 1) * nx * 4 + ix * 4 + 1] = isFlow;
//                mask[(ny - iy - 1) * nx * 4 + ix * 4 + 2] = isFlow;
//                mask[(ny - iy - 1) * nx * 4 + ix * 4 + 3] = isFlow;


//                texture[(ny - iy - 1) * nx * 4 + ix * 4 + 0] = r;
//                texture[(ny - iy - 1) * nx * 4 + ix * 4 + 1] = g;
//                texture[(ny - iy - 1) * nx * 4 + ix * 4 + 2] = b;
//                texture[(ny - iy - 1) * nx * 4 + ix * 4 + 3] = a;
//                {
            texture[(ny - iy - 1) * nx * 8 + ix * 8 + 0] = rgba_u.rgba8[0];
            texture[(ny - iy - 1) * nx * 8 + ix * 8 + 1] = rgba_u.rgba8[1];
            texture[(ny - iy - 1) * nx * 8 + ix * 8 + 2] = rgba_u.rgba8[2];
            texture[(ny - iy - 1) * nx * 8 + ix * 8 + 3] = rgba_u.rgba8[3];
            texture[(ny - iy - 1) * nx * 8 + ix * 8 + 4] = rgba_v.rgba8[0];
            texture[(ny - iy - 1) * nx * 8 + ix * 8 + 5] = rgba_v.rgba8[1];
            texture[(ny - iy - 1) * nx * 8 + ix * 8 + 6] = rgba_v.rgba8[2];
            texture[(ny - iy - 1) * nx * 8 + ix * 8 + 7] = rgba_v.rgba8[3];
//                }
        }
    }

    long ix, iy, tx, ty;
    std::vector<long> filterAddress;
    for (size_t index = 0; index < validAddress.size() / 2; ++index)
    {
        ix = validAddress[index * 2 + 0]; iy = validAddress[index * 2 + 1];
        for (long mx = - 1; mx < 2; ++mx)
        {
            tx = ix + mx;
            if (tx < 0 || tx > nx - 1) continue;
            for (long my = -1; my < 2; ++my)
            {
                ty = iy + my;
                if (ty < 0 || ty > ny - 1) continue;
                if (mx == 0 && my == 0) continue;
                if (m_bmpMask[(ny - ty - 1) * nx * 3 + tx * 3] || m_bmpMask[(ny - ty - 1) * nx * 3 + tx * 3 + 1] == 1) continue;
                else
                {
                    filterAddress.emplace_back(tx);
                    filterAddress.emplace_back(ty);
                    m_bmpMask[(ny - ty - 1) * nx * 3 + tx * 3 + 1] = 1;
                }
            }
        }
    }

    float validCount;
    for (size_t index = 0; index < filterAddress.size() / 2; ++index)
    {
        validCount = 0.0;
        rgba_u.f32 = rgba_v.f32 = 0.0;
        ix = filterAddress[index * 2 + 0];
        iy = filterAddress[index * 2 + 1];
        for (int mx = -1; mx < 2; ++mx)
        {
            tx = ix + mx;
            if (tx < 0 || tx > nx - 1) continue;
            for (int my = -1; my < 2; ++my)
            {
                ty = iy + my;
                if (ty < 0 || ty > ny - 1) continue;

                if (m_bmpMask[(ny - ty - 1) * nx * 3 + tx * 3])
                {
                    validCount += 1.0f / sqrt(float(mx * mx + my * my));
                    rgba_u.f32 += (grd_u.z(tx, ty) - m_flowVelocityRange[0]) / (m_flowVelocityRange[2] - m_flowVelocityRange[0]) / sqrt(float(mx * mx + my * my));
                    rgba_v.f32 += (grd_v.z(tx, ty) - m_flowVelocityRange[1]) / (m_flowVelocityRange[3] - m_flowVelocityRange[1]) / sqrt(float(mx * mx + my * my));
                }
            }
        }
        rgba_u.f32 /= validCount;
        rgba_v.f32 /= validCount;

        texture[(ny - iy - 1) * nx * 8 + ix * 8 + 0] = rgba_u.rgba8[0];
        texture[(ny - iy - 1) * nx * 8 + ix * 8 + 1] = rgba_u.rgba8[1];
        texture[(ny - iy - 1) * nx * 8 + ix * 8 + 2] = rgba_u.rgba8[2];
        texture[(ny - iy - 1) * nx * 8 + ix * 8 + 3] = rgba_u.rgba8[3];
        texture[(ny - iy - 1) * nx * 8 + ix * 8 + 4] = rgba_v.rgba8[0];
        texture[(ny - iy - 1) * nx * 8 + ix * 8 + 5] = rgba_v.rgba8[1];
        texture[(ny - iy - 1) * nx * 8 + ix * 8 + 6] = rgba_v.rgba8[2];
        texture[(ny - iy - 1) * nx * 8 + ix * 8 + 7] = rgba_v.rgba8[3];
    }

    // Build the rebirth pool texture
//        size_t x, y;
//        size_t poolSize = ceil(sqrt(double(validAddress.size()) / 2.0));
//        rebirthPool.resize(poolSize * poolSize * 4);
//        for (size_t iy = 0; iy < poolSize; ++iy)
//        {
//            for (size_t ix = 0; ix < poolSize; ++ix)
//            {
//                index = (iy * poolSize + ix) % (validAddress.size() / 2);
//
//                x = validAddress[index * 2 + 0];
//                y = validAddress[index * 2 + 1];
//
//                r = int(x) >> 8;
//                g = int(x) & 0xff;
//                b = int(y) >> 8;
//                a = int(y) & 0xff;
//                rebirthPool[index * 4 + 0] = r;
//                rebirthPool[index * 4 + 1] = g;
//                rebirthPool[index * 4 + 2] = b;
//                rebirthPool[index * 4 + 3] = a;
//            }
//        }

    m_result["texture_size"]["flow_field"][0] = nx;  m_result["texture_size"]["flow_field"][1] = ny;
    m_result["texture_size"]["area_mask"][0] = nx;  m_result["texture_size"]["area_mask"][1] = ny;
//        m_result["texture_size"]["valid_address"][0] = poolSize;  m_result["texture_size"]["valid_address"][1] = poolSize;

    int status1 = stbi_write_png(textureFile, nx * 2, ny, 4, texture.data(), 0);
    int status2 = stbi_write_png(maskFile, nx, ny, 4, mask.data(), 0);
//        int status3 = stbi_write_png(poolFile, poolSize, poolSize, 4, rebirthPool.data(), 0);
//        return status1 && status2 && status3;
    return status1 && status2;
}

void FlowField::mercatorCoordsFromLonLat(OGRCoordinateTransformation* poCT, double& lon, double& lat)
{
    poCT->Transform(1, &lat, &lon);

    lon = (180.0 + lon) / 360.0;
    lat = (180.0 - (180.0 / M_PI * log(tan(M_PI / 4.0 + lat * M_PI / 360.0)))) / 360.0;
}

void FlowField::epsgFromLonLat(OGRCoordinateTransformation* poCT, double& lon, double& lat)
{
    poCT->Transform(1, &lat, &lon);
}

void FlowField::cesiumWgs84ToCartesian(OGRCoordinateTransformation* poCT, double& lon, double& lat, double& altitude)
{

    poCT->Transform(1, &lat, &lon);

    auto cartesian = new Cartesian3(lon, lat);
    cartesian = Cartesian3::fromDegrees(lon, lat, altitude, cartesian);
    lon = cartesian->x;
    lat = cartesian->y;
    altitude = cartesian->z;
}

OGRGeometry* FlowField::createVectorMask()
{
    GDALAllRegister();

    const auto vectorMaskPath = m_inputDir / m_vMask;
    const auto vectorMaskPathString = vectorMaskPath.string();
    m_ds = (GDALDataset*) GDALOpenEx(vectorMaskPathString.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
    if (m_ds == NULL)
    {
        std::cout << "No shp file was founded, vector mask function will not be activated." << std::endl;
        return nullptr;
    }

    OGRLayer *vLayer;
    vLayer = m_ds->GetLayer(0);
    OGRFeature* vFeature = vLayer->GetFeature(0);
    OGRGeometry* mask = vFeature->GetGeometryRef()->clone();

    char* pszWKT = NULL;
    m_sRef = vLayer->GetSpatialRef();
    m_sRef->exportToWkt(&pszWKT);

    // Update boundary
    OGREnvelope envelope;
    mask->getEnvelope(&envelope);
    m_boundary[0] = envelope.MinX;
    m_boundary[1] = envelope.MinY;
    m_boundary[2] = envelope.MaxX;
    m_boundary[3] = envelope.MaxY;

    // set vector mask options
    m_maskOptions = NULL;

    m_maskOptions = CSLAddString(m_maskOptions, "-i");

    m_maskOptions = CSLAddString(m_maskOptions, "-a");
    m_maskOptions = CSLAddString(m_maskOptions, "Id");

    m_maskOptions = CSLAddString(m_maskOptions, "-l");
    m_maskOptions = CSLAddString(m_maskOptions, vLayer->GetName());

    m_maskOptions = CSLAddString(m_maskOptions, "-init");
    m_maskOptions = CSLAddString(m_maskOptions, "255");

    m_maskOptions = CSLAddString(m_maskOptions, "-of");
    m_maskOptions = CSLAddString(m_maskOptions, "BMP");

    m_maskOptions = CSLAddString(m_maskOptions, "-a_srs");
    m_maskOptions = CSLAddString(m_maskOptions, pszWKT);

    m_maskOptions = CSLAddString(m_maskOptions, "-ot");
    m_maskOptions = CSLAddString(m_maskOptions, "byte");

    return mask;
}

void FlowField::buildBmpMask(int width, int height)
{
    if (m_ds == NULL)
        return;
    m_maskOptions = CSLAddString(m_maskOptions, "-ts");
    m_maskOptions = CSLAddString(m_maskOptions, std::to_string(width).c_str());
    m_maskOptions = CSLAddString(m_maskOptions, std::to_string(height).c_str());

    auto options = GDALRasterizeOptionsNew(m_maskOptions, NULL);

    int error;
    const auto tempMaskPathString = m_tempMaskPath.string();
    GDALClose(GDALRasterize(tempMaskPathString.c_str(), NULL, m_ds, options, &error));
    GDALRasterizeOptionsFree(options);
    readBmpMask();
}

void FlowField::closeDataSet()
{
    if (m_ds != nullptr)
    {
        GDALClose(m_ds);
        m_ds = nullptr;
    }
}
void FlowField::readBmpMask()
{
    int width, height, n;
    const auto tempMaskPathString = m_tempMaskPath.string();
    auto data = stbi_load(tempMaskPathString.c_str(), &width, &height, &n, 0);
    m_bmpMask = std::vector<char>(data, data + width * height * n);
    stbi_image_free(data);
}

FlowField::FlowField(const char* descriptionPath, Json& resultJson)
        :m_descriptionPath(descriptionPath),
         m_attributeIndex {-1, -1, -1, -1},
         m_flowVelocityRange {
                 std::numeric_limits<double>::max(),
                 std::numeric_limits<double>::max(),
                 std::numeric_limits<double>::lowest(),
                 std::numeric_limits<double>::lowest()},
         m_ds(nullptr),
         m_sRef(nullptr),
         m_vectorMask(nullptr),
         m_maskOptions(nullptr),
         m_result(resultJson)
{
    std::ifstream  iStream(m_descriptionPath);
    Json descriptiveJson;
    iStream >> descriptiveJson;

    const std::filesystem::path descriptionFilePath{m_descriptionPath};
    m_baseDir = descriptionFilePath.has_parent_path() ? descriptionFilePath.parent_path() : std::filesystem::current_path();

    m_inUrl = descriptiveJson["input_url"];
    m_outUrl = descriptiveJson["output_url"];
    m_inputDir = resolvePath(m_baseDir, m_inUrl);
    m_outputDir = resolvePath(m_baseDir, m_outUrl);
    std::filesystem::create_directories(m_outputDir);
    m_tempMaskPath = m_outputDir / "mask.bmp";
    m_columnNum = descriptiveJson["column_num"];
    m_xName = descriptiveJson["coordinates"][0];
    m_yName = descriptiveJson["coordinates"][1];
    m_uName = descriptiveJson["velocities"][0];
    m_vName = descriptiveJson["velocities"][1];
    m_outlier = descriptiveJson["outlier"];
    m_headingNum = descriptiveJson["headings"].size();
    m_resolution = descriptiveJson["resolution"];
    m_boundary[0] = descriptiveJson["boundary"][0];
    m_boundary[1] = descriptiveJson["boundary"][1];
    m_boundary[2] = descriptiveJson["boundary"][2];
    m_boundary[3] = descriptiveJson["boundary"][3];
    m_vMask = descriptiveJson["vector_mask"];
    m_sourceSpace = descriptiveJson["source_space"];

    urlPath = "/images/";
    resultPath = (m_outputDir / "flow_field_description.json").string();

    for (const std::string& s : descriptiveJson["target_space"])
    {
        m_targetSpaces.emplace_back(s);
    }

    int count = 0;
    for (const std::string& s : descriptiveJson["headings"])
    {
        if (s.compare(m_xName) == 0) m_attributeIndex[0] = count;
        else if (s.compare(m_yName) == 0) m_attributeIndex[1] = count;
        else if (s.compare(m_uName) == 0) m_attributeIndex[2] = count;
        else if (s.compare(m_vName) == 0) m_attributeIndex[3] = count;
        count++;
    }
    for (int index : m_attributeIndex)
    {
        if (index < 0)
        {
            throw std::invalid_argument("description headings must contain coordinate and velocity fields");
        }
    }

    for (const auto& f: descriptiveJson["files"])
    {
        FlowFieldFile file {.in = f["in"], .sn = f["sn"]};
        m_files.emplace_back(file);
    }
}

FlowField::~FlowField()
{
    closeDataSet();
}

FlowField* FlowField::create(const char* descriptionPath, Json& resultJson)
{
    return new FlowField{descriptionPath, resultJson};
}

void FlowField::preprocess()
{
    // Kept for old callers; raw float32 UV output no longer needs a global flow range scan.
}

void FlowField::buildProjectionTexture(int width, int height, size_t index)
{
//        m_result["projection_width"] = width;
//        m_result["projection_height"] = height;

    OGRSpatialReference target, source;
    source.importFromEPSG(std::stoi(this->m_sourceSpace));
//    target.importFromEPSG(4979);

    PlatformType platformType;
    if (m_targetSpaces[index] == "mapbox")
    {
        platformType = PlatformType::Mapbox;
        target.importFromEPSG(4326);
    }
    else if (m_targetSpaces[index] == "cesium")
    {
        platformType = PlatformType::Cesium;
        target.importFromEPSG(4326);
    }
    else
    {
        platformType = PlatformType::Epsg;
        target.importFromEPSG(std::stoi(m_targetSpaces[index]));
    }

    OGRCoordinateTransformation *poCT;
    poCT = OGRCreateCoordinateTransformation(&source, &target);

    Texel rgba_lon, rgba_lat, rgba_alt;
    int nx = width;
    int ny = height;
    double dx = (m_boundary[2] - m_boundary[0]) / nx;
    double dy = (m_boundary[3] - m_boundary[1]) / ny;
    std::vector<double> xArray, yArray, coordsArray;
    std::vector<char> texture;
    texture.resize(nx * ny * 4 * 3, 0);

    for (int i = 0; i < nx; ++i)
    {
        xArray.emplace_back(m_boundary[0] + 0.5 * dx + double(i) * dx);
    }
    for (int j = 0; j < ny; ++j)
    {
        yArray.emplace_back(m_boundary[1] + 0.5 * dy + double(j) * dy);
    }

    // Transform boundary (geographic -> projected: _x -> y, _y -> x)
    double _xMin = m_boundary[0], _xMax = m_boundary[2], _yMin = m_boundary[1], _yMax = m_boundary[3], al = 0.0;
    switch (platformType) {
        case PlatformType::Mapbox:
            mercatorCoordsFromLonLat(poCT, _xMin, _yMin);
            mercatorCoordsFromLonLat(poCT, _xMax, _yMax);
            break;
        case PlatformType::Cesium:
            al = 0.0;
            cesiumWgs84ToCartesian(poCT, _xMin, _yMin, al);
            al = 0.0;
            cesiumWgs84ToCartesian(poCT, _xMax, _yMax, al);
            break;
        case PlatformType::Epsg:
            epsgFromLonLat(poCT, _xMin, _yMin);
            epsgFromLonLat(poCT, _xMax, _yMax);
            break;
    }

    m_result["extent"][0] = _xMin;
    m_result["extent"][1] = _yMin;
    m_result["extent"][2] = _xMax;
    m_result["extent"][3] = _yMax;

    // Transform coords (geographic -> projected: _lon -> lat, _lat -> lon)
    double lon, lat;
    int components = 0, sizePerCell = 0;
    for (int j = 0; j < ny; ++j)
    {
        for (int i = 0; i < nx; ++i)
        {
            lon = xArray[i];
            lat = yArray[j];
            al = 0.0;
            switch (platformType) {
                case PlatformType::Mapbox:
                    mercatorCoordsFromLonLat(poCT, lon, lat);
                    components = 2;
                    break;
                case PlatformType::Cesium:
                    cesiumWgs84ToCartesian(poCT, lon, lat, al);
                    components = 3;
                    break;
                case PlatformType::Epsg:
                    epsgFromLonLat(poCT, lon, lat);
                    break;
            }

            sizePerCell = components * 4;
            rgba_lon.f32 = lon, rgba_lat.f32 = lat, rgba_alt.f32 = al;
            texture[(height - j - 1) * nx * sizePerCell + i * sizePerCell + 0] = rgba_lon.rgba8[0];
            texture[(height - j - 1) * nx * sizePerCell + i * sizePerCell + 1] = rgba_lon.rgba8[1];
            texture[(height - j - 1) * nx * sizePerCell + i * sizePerCell + 2] = rgba_lon.rgba8[2];
            texture[(height - j - 1) * nx * sizePerCell + i * sizePerCell + 3] = rgba_lon.rgba8[3];
            texture[(height - j - 1) * nx * sizePerCell + i * sizePerCell + 4] = rgba_lat.rgba8[0];
            texture[(height - j - 1) * nx * sizePerCell + i * sizePerCell + 5] = rgba_lat.rgba8[1];
            texture[(height - j - 1) * nx * sizePerCell + i * sizePerCell + 6] = rgba_lat.rgba8[2];
            texture[(height - j - 1) * nx * sizePerCell + i * sizePerCell + 7] = rgba_lat.rgba8[3];
            if (components == 3)
            {
                texture[(height - j - 1) * nx * sizePerCell + i * sizePerCell + 8] = rgba_alt.rgba8[0];
                texture[(height - j - 1) * nx * sizePerCell + i * sizePerCell + 9] = rgba_alt.rgba8[1];
                texture[(height - j - 1) * nx * sizePerCell + i * sizePerCell + 10] = rgba_alt.rgba8[2];
                texture[(height - j - 1) * nx * sizePerCell + i * sizePerCell + 11] = rgba_alt.rgba8[3];
            }
        }
    }
    auto projectionFile = m_outputDir / ("projection_" + m_targetSpaces[index] + ".png");
    m_result["projection"].push_back(urlPath + "projection_" + m_targetSpaces[index] + ".png");
    m_result["texture_size"]["projection"][0] = width;  m_result["texture_size"]["projection"][1] = height;

    const auto projectionFileString = projectionFile.string();
    int status1 = stbi_write_png(projectionFileString.c_str(), width * components, height, 4, texture.data(), 0);
}

void FlowField::buildTextures()
{
    FlowFieldBuilder builder(m_outputDir);
    XpandOptions quikgridOptions;
    quikgridOptions.densityRatio = 350;
    builder.setQuikgridOptions(quikgridOptions);

    for (const auto& file : m_files)
    {
        std::cout << "==== Process File " << file.in << " ====" << std::endl;
        const auto readFile = m_inputDir / file.in;
        auto step = readFlowStep(
                readFile,
                m_headingNum,
                m_columnNum,
                m_attributeIndex,
                m_boundary);
        const auto textureSize = textureSizeFromResolution(step.xs, step.ys, m_resolution);
        builder.setTextureSize(textureSize[0], textureSize[1]);
        if (!m_vMask.empty())
        {
            builder.setDomainFromVector(step.xs.data(), step.ys.data(), step.xs.size(), m_inputDir / m_vMask);
        }
        else
        {
            builder.setDomainFromArrays(step.xs.data(), step.ys.data(), step.xs.size());
        }
        builder.buildUvTexture(step.us.data(), step.vs.data(), step.us.size(), file.sn);

        m_result["flow_fields"].push_back(urlPath + "uv_" + file.sn + ".png");
        m_result["seed_textures"].push_back(urlPath + "seed_" + file.sn + ".png");
        m_result["area_masks"].push_back(urlPath + "seed_" + file.sn + ".png");
        m_result["texture_size"]["flow_field"][0] = textureSize[0];
        m_result["texture_size"]["flow_field"][1] = textureSize[1];
        m_result["texture_size"]["seed"][0] = textureSize[0];
        m_result["texture_size"]["seed"][1] = textureSize[1];
        m_result["texture_size"]["area_mask"][0] = textureSize[0];
        m_result["texture_size"]["area_mask"][1] = textureSize[1];
        std::cout << "Texture " << file.sn << " Building finished: SUCCESSED" << std::endl;
    }
}
