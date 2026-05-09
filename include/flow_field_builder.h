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
    void setQuikgridOptions(const XpandOptions& options);

    void setDomainFromArrays(const double* xs, const double* ys, std::size_t count);
    void setDomainFromVector(
            const double* xs,
            const double* ys,
            std::size_t count,
            const std::filesystem::path& vectorPath);

    void buildUvTexture(
            const float* us,
            const float* vs,
            std::size_t count,
            const std::string& outputName);

    int textureWidth() const { return m_textureWidth; }
    int textureHeight() const { return m_textureHeight; }
    const std::array<double, 4>& domainExtent() const { return m_domainExtent; }
    std::filesystem::path uvTexturePath(const std::string& outputName) const;
    std::filesystem::path seedTexturePath(const std::string& outputName) const;

private:
    enum class DomainMode
    {
        None,
        Arrays,
        Vector
    };

    void setDomainArrays(const double* xs, const double* ys, std::size_t count);
    void rebuildDomain();
    void rebuildGridCoordinates();
    void rebuildVectorMask();
    void validateReadyToBuild(std::size_t count, const std::string& outputName) const;

    bool isDefined(float value) const;
    bool isValidCell(float u, float v, std::size_t index) const;
    void writeTextures(
            const std::vector<float>& uGrid,
            const std::vector<float>& vGrid,
            const std::string& outputName) const;

    std::filesystem::path m_outputDirectory;
    int m_textureWidth = 0;
    int m_textureHeight = 0;
    XpandOptions m_options;
    DomainMode m_domainMode = DomainMode::None;
    std::filesystem::path m_vectorPath;
    std::vector<double> m_xs;
    std::vector<double> m_ys;
    std::array<double, 4> m_domainExtent {0.0, 0.0, 0.0, 0.0};
    std::vector<float> m_gridXs;
    std::vector<float> m_gridYs;
    std::vector<unsigned char> m_domainMask;
    XpandContext m_context;
};
