#pragma once

#include <cstddef>
#include <vector>

#include "gridxtyp.h"

struct XpandOptions
{
    int scanRatio = 16;
    int densityRatio = 150;
    int edgeFactor = 100;
    float undefinedValue = -99999.0f;
    long sample = 1;
};

class XpandContext
{
public:
    XpandContext();

    void reset(
            const double* xs,
            const double* ys,
            std::size_t count,
            const float* gridXs,
            std::size_t gridXCount,
            const float* gridYs,
            std::size_t gridYCount,
            const XpandOptions& options);

    void interpolate(
            const float* values,
            std::size_t valueCount,
            float* output,
            std::size_t outputCount);

    std::size_t pointCount() const { return m_xs.size(); }
    std::size_t gridXCount() const { return m_gridXs.size(); }
    std::size_t gridYCount() const { return m_gridYs.size(); }

private:
    struct SearchState
    {
        float pointDistSquared[8] {};
        long pointInOctant[8] {};
        float closestSquared = 0.0f;
        long noFound = 0;
        long totalRange = 0;
        long totalShells = 0;
    };

    bool locateGrid(int& ix, int& iy, float x, float y) const;
    void selectPoints(int xIndex, int yIndex, SearchState& state);
    void scanOneGrid(int locatorX, int locatorY, int xIndex, int yIndex, SearchState& state);
    void putInOctant(float x, float y, long dataIndex, SearchState& state) const;
    bool includeGridPoint(const SearchState& state) const;
    float weightedAverage(const float* values, const SearchState& state) const;

    std::vector<float> m_xs;
    std::vector<float> m_ys;
    std::vector<float> m_gridXs;
    std::vector<float> m_gridYs;
    XpandOptions m_options;
    GridXType m_locator;
    float m_xyStopDistSquared = 0.0f;
    float m_edgeSenseDistSquared = 0.0f;
};
