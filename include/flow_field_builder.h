#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "xpand_context.h"

class FlowFieldBuilder
{
public:
    FlowFieldBuilder();
    explicit FlowFieldBuilder(std::filesystem::path outputDirectory);

    void setOutputDirectory(std::filesystem::path outputDirectory);
    void setTextureSize(int width, int height);
    void setProjectionTextureSize(int width, int height);
    void setQuikgridOptions(const XpandOptions& options);

    void setDomainFromArrays(const double* xs, const double* ys, std::size_t count);
    void setDomainFromArrays(const double* xs, const double* ys, std::size_t count, int sourceEpsg);
    void setDomainFromVector(
            const double* xs,
            const double* ys,
            std::size_t count,
            const std::filesystem::path& vectorPath);
    void setDomainFromVector(
            const double* xs,
            const double* ys,
            std::size_t count,
            const std::filesystem::path& vectorPath,
            int sourceEpsg);

    void buildUvTexture(
            const float* us,
            const float* vs,
            std::size_t count,
            const std::string& outputName);
    void buildProjectionTexture(const std::string& target, const std::string& outputName);

    int textureWidth() const { return m_textureWidth; }
    int textureHeight() const { return m_textureHeight; }
    int projectionTextureWidth() const { return m_projectionTextureWidth; }
    int projectionTextureHeight() const { return m_projectionTextureHeight; }
    int sourceEpsg() const { return m_sourceEpsg; }
    const std::array<double, 4>& domainExtent() const { return m_domainExtent; }
    const std::array<double, 4>& projectionExtent() const { return m_projectionExtent; }
    std::filesystem::path uvTexturePath(const std::string& outputName) const;
    std::filesystem::path seedTexturePath(const std::string& outputName) const;
    std::filesystem::path projectionTexturePath(const std::string& outputName) const;

private:
    enum class DomainMode
    {
        None,
        Arrays,
        Vector
    };

    void setDomainArrays(const double* xs, const double* ys, std::size_t count);
    void setSourceEpsg(int epsg);
    void rebuildDomain();
    void rebuildGridCoordinates();
    void rebuildVectorMask();
    void validateReadyToBuild(std::size_t count, const std::string& outputName) const;
    void validateReadyToBuildProjection(const std::string& target, const std::string& outputName) const;

    bool isDefined(float value) const;
    bool isValidCell(float u, float v, std::size_t index) const;
    void writeTextures(
            const std::vector<float>& uGrid,
            const std::vector<float>& vGrid,
            const std::string& outputName) const;

    std::filesystem::path m_outputDirectory;
    int m_textureWidth = 0;
    int m_textureHeight = 0;
    int m_projectionTextureWidth = 0;
    int m_projectionTextureHeight = 0;
    int m_sourceEpsg = 0;
    XpandOptions m_options;
    DomainMode m_domainMode = DomainMode::None;
    std::filesystem::path m_vectorPath;
    std::vector<double> m_xs;
    std::vector<double> m_ys;
    std::array<double, 4> m_domainExtent {0.0, 0.0, 0.0, 0.0};
    std::array<double, 4> m_projectionExtent {0.0, 0.0, 0.0, 0.0};
    std::vector<float> m_gridXs;
    std::vector<float> m_gridYs;
    std::vector<unsigned char> m_domainMask;
    XpandContext m_context;
};
