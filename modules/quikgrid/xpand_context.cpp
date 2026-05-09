#include "xpand_context.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

constexpr float kPi = 3.14159f;

int clampInt(int value, int low, int high)
{
    return std::max(low, std::min(value, high));
}

long clampSample(long value)
{
    return std::max(1L, value);
}

bool isStrictlyIncreasing(const std::vector<float>& values)
{
    return std::adjacent_find(values.begin(), values.end(), [](float left, float right) {
        return !(left < right);
    }) == values.end();
}

} // namespace

XpandContext::XpandContext()
        : m_locator(0, 0, 0)
{
}

void XpandContext::reset(
        const double* xs,
        const double* ys,
        std::size_t count,
        const float* gridXs,
        std::size_t gridXCount,
        const float* gridYs,
        std::size_t gridYCount,
        const XpandOptions& options)
{
    if (xs == nullptr || ys == nullptr)
    {
        throw std::invalid_argument("xs and ys must not be null");
    }
    if (gridXs == nullptr || gridYs == nullptr)
    {
        throw std::invalid_argument("grid coordinates must not be null");
    }
    if (count == 0 || gridXCount == 0 || gridYCount == 0)
    {
        throw std::invalid_argument("point and grid counts must be positive");
    }
    if (count > static_cast<std::size_t>(std::numeric_limits<long>::max())
        || gridXCount > static_cast<std::size_t>(std::numeric_limits<int>::max())
        || gridYCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument("point or grid count exceeds quikgrid index range");
    }

    m_options = options;
    m_options.scanRatio = clampInt(m_options.scanRatio, 1, 100);
    m_options.densityRatio = clampInt(m_options.densityRatio, 1, 10000);
    m_options.edgeFactor = clampInt(m_options.edgeFactor, 1, 10000);
    m_options.sample = clampSample(m_options.sample);

    m_xs.assign(count, 0.0f);
    m_ys.assign(count, 0.0f);
    for (std::size_t index = 0; index < count; ++index)
    {
        m_xs[index] = static_cast<float>(xs[index]);
        m_ys[index] = static_cast<float>(ys[index]);
    }

    m_gridXs.assign(gridXs, gridXs + gridXCount);
    m_gridYs.assign(gridYs, gridYs + gridYCount);
    if (!isStrictlyIncreasing(m_gridXs) || !isStrictlyIncreasing(m_gridYs))
    {
        throw std::invalid_argument("grid coordinates must be strictly increasing");
    }

    const auto [minXIt, maxXIt] = std::minmax_element(m_xs.begin(), m_xs.end());
    const auto [minYIt, maxYIt] = std::minmax_element(m_ys.begin(), m_ys.end());
    const float xRange = *maxXIt - *minXIt;
    const float yRange = *maxYIt - *minYIt;
    const float volume = xRange * yRange;
    const float sampledCount = static_cast<float>(count) / static_cast<float>(m_options.sample);
    const float volumePerPoint = sampledCount > 0.0f ? volume / sampledCount : 0.0f;
    const float radius = std::sqrt(std::max(0.0f, volumePerPoint) / kPi);
    const float diameter = radius * 2.0f;
    const float diameterSquared = diameter * diameter;
    const float densityStopRatio = static_cast<float>(m_options.densityRatio) * 0.01f;
    const float edgeSenseFactor = static_cast<float>(m_options.edgeFactor) * 0.01f;
    m_xyStopDistSquared = diameterSquared * densityStopRatio * densityStopRatio;
    m_edgeSenseDistSquared = diameterSquared * edgeSenseFactor * edgeSenseFactor;

    m_locator.New(static_cast<long>(count), static_cast<long>(gridXCount), static_cast<long>(gridYCount));
    if (count < 3)
    {
        return;
    }

    int locatorX = 0;
    int locatorY = 0;
    for (std::size_t index = 0; index < count; index += static_cast<std::size_t>(m_options.sample))
    {
        if (locateGrid(locatorX, locatorY, m_xs[index], m_ys[index]))
        {
            m_locator.setnext(static_cast<long>(index), locatorX, locatorY);
        }
    }
    m_locator.Sort();
}

void XpandContext::interpolate(
        const float* values,
        std::size_t valueCount,
        float* output,
        std::size_t outputCount)
{
    if (values == nullptr || output == nullptr)
    {
        throw std::invalid_argument("values and output must not be null");
    }
    if (valueCount != m_xs.size())
    {
        throw std::invalid_argument("value count must match point count");
    }
    const std::size_t requiredOutputCount = m_gridXs.size() * m_gridYs.size();
    if (outputCount < requiredOutputCount)
    {
        throw std::invalid_argument("output buffer is too small");
    }

    std::fill(output, output + requiredOutputCount, m_options.undefinedValue);
    if (m_xs.size() < 3)
    {
        return;
    }

    for (std::size_t yIndex = 0; yIndex < m_gridYs.size(); ++yIndex)
    {
        for (std::size_t xIndex = 0; xIndex < m_gridXs.size(); ++xIndex)
        {
            SearchState state;
            selectPoints(static_cast<int>(xIndex), static_cast<int>(yIndex), state);
            if (includeGridPoint(state))
            {
                output[yIndex * m_gridXs.size() + xIndex] = weightedAverage(values, state);
            }
        }
    }
}

bool XpandContext::locateGrid(int& ix, int& iy, float x, float y) const
{
    const float xGridMin = m_gridXs.front();
    const float xGridMax = m_gridXs.back();
    const float yGridMin = m_gridYs.front();
    const float yGridMax = m_gridYs.back();

    float distance = xGridMin - x;
    if (x < xGridMin && distance * distance > m_xyStopDistSquared)
    {
        return false;
    }
    distance = xGridMax - x;
    if (x > xGridMax && distance * distance > m_xyStopDistSquared)
    {
        return false;
    }
    distance = yGridMin - y;
    if (y < yGridMin && distance * distance > m_xyStopDistSquared)
    {
        return false;
    }
    distance = yGridMax - y;
    if (y > yGridMax && distance * distance > m_xyStopDistSquared)
    {
        return false;
    }

    auto nearestIndex = [](const std::vector<float>& values, float target) {
        const auto upper = std::lower_bound(values.begin(), values.end(), target);
        if (upper == values.begin())
        {
            return 0;
        }
        if (upper == values.end())
        {
            return static_cast<int>(values.size() - 1);
        }
        const auto previous = upper - 1;
        return std::fabs(*upper - target) < std::fabs(*previous - target)
                ? static_cast<int>(upper - values.begin())
                : static_cast<int>(previous - values.begin());
    };

    ix = nearestIndex(m_gridXs, x);
    iy = nearestIndex(m_gridYs, y);
    return true;
}

void XpandContext::selectPoints(int xIndex, int yIndex, SearchState& state)
{
    for (int index = 0; index < 8; ++index)
    {
        state.pointInOctant[index] = -1;
        state.pointDistSquared[index] = 0.0f;
    }
    state.noFound = 0;

    scanOneGrid(xIndex, yIndex, xIndex, yIndex, state);

    bool topDone = false;
    bool bottomDone = false;
    bool leftDone = false;
    bool rightDone = false;
    const int numX = static_cast<int>(m_gridXs.size());
    const int numY = static_cast<int>(m_gridYs.size());
    const float scanStopRatio = static_cast<float>(m_options.scanRatio);

    for (int shell = 1;; ++shell)
    {
        int start = std::max(0, xIndex - shell);
        int end = std::min(numX, xIndex + shell + 1);

        if (!topDone)
        {
            const int row = yIndex + shell;
            if (row >= numY)
            {
                topDone = true;
            }
            else
            {
                const float testDist = (m_gridYs[row] - m_gridYs[yIndex]) * (m_gridYs[row] - m_gridYs[yIndex]);
                if ((state.noFound > 0 && testDist > state.closestSquared * scanStopRatio) || testDist > m_xyStopDistSquared)
                {
                    topDone = true;
                }
                else
                {
                    for (int ix = start; ix < end; ++ix)
                    {
                        scanOneGrid(ix, row, xIndex, yIndex, state);
                    }
                }
            }
        }

        if (!bottomDone)
        {
            const int row = yIndex - shell;
            if (row < 0)
            {
                bottomDone = true;
            }
            else
            {
                const float testDist = (m_gridYs[yIndex] - m_gridYs[row]) * (m_gridYs[yIndex] - m_gridYs[row]);
                if ((state.noFound > 0 && testDist > state.closestSquared * scanStopRatio) || testDist > m_xyStopDistSquared)
                {
                    bottomDone = true;
                }
                else
                {
                    for (int ix = start; ix < end; ++ix)
                    {
                        scanOneGrid(ix, row, xIndex, yIndex, state);
                    }
                }
            }
        }

        start = std::max(0, yIndex - shell + 1);
        end = std::min(numY, yIndex + shell);

        if (!leftDone)
        {
            const int column = xIndex - shell;
            if (column < 0)
            {
                leftDone = true;
            }
            else
            {
                const float testDist = (m_gridXs[xIndex] - m_gridXs[column]) * (m_gridXs[xIndex] - m_gridXs[column]);
                if ((state.noFound > 0 && testDist > state.closestSquared * scanStopRatio) || testDist > m_xyStopDistSquared)
                {
                    leftDone = true;
                }
                else
                {
                    for (int iy = start; iy < end; ++iy)
                    {
                        scanOneGrid(column, iy, xIndex, yIndex, state);
                    }
                }
            }
        }

        if (!rightDone)
        {
            const int column = xIndex + shell;
            if (column >= numX)
            {
                rightDone = true;
            }
            else
            {
                const float testDist = (m_gridXs[column] - m_gridXs[xIndex]) * (m_gridXs[column] - m_gridXs[xIndex]);
                if ((state.noFound > 0 && testDist > state.closestSquared * scanStopRatio) || testDist > m_xyStopDistSquared)
                {
                    rightDone = true;
                }
                else
                {
                    for (int iy = start; iy < end; ++iy)
                    {
                        scanOneGrid(column, iy, xIndex, yIndex, state);
                    }
                }
            }
        }

        if (topDone && bottomDone && leftDone && rightDone)
        {
            state.totalShells += shell;
            break;
        }
    }
}

void XpandContext::scanOneGrid(int locatorX, int locatorY, int xIndex, int yIndex, SearchState& state)
{
    for (long offset = 0;; ++offset)
    {
        const long dataIndex = m_locator.Search(locatorX, locatorY, static_cast<int>(offset));
        if (dataIndex < 0)
        {
            return;
        }
        putInOctant(m_gridXs[xIndex], m_gridYs[yIndex], dataIndex, state);
        ++state.totalRange;
    }
}

void XpandContext::putInOctant(float x, float y, long dataIndex, SearchState& state) const
{
    const float xDistance = m_xs[dataIndex] - x;
    const float yDistance = m_ys[dataIndex] - y;

    int octant = 0;
    if (yDistance < 0.0f)
    {
        octant = 4;
    }
    if (xDistance < 0.0f)
    {
        octant += 2;
    }
    if (std::fabs(xDistance) < std::fabs(yDistance))
    {
        octant += 1;
    }

    const float distSquared = xDistance * xDistance + yDistance * yDistance;
    if (state.noFound == 0)
    {
        state.closestSquared = distSquared;
    }
    if (distSquared < state.closestSquared)
    {
        state.closestSquared = distSquared;
    }
    ++state.noFound;

    if (state.pointInOctant[octant] == -1 || distSquared < state.pointDistSquared[octant])
    {
        state.pointInOctant[octant] = dataIndex;
        state.pointDistSquared[octant] = distSquared;
    }
}

bool XpandContext::includeGridPoint(const SearchState& state) const
{
    static constexpr int lookupTable[11] = {0, 1, 3, 2, 6, 7, 5, 4, 0, 1, 3};

    if (state.noFound == 0)
    {
        return false;
    }
    if (state.closestSquared > m_xyStopDistSquared)
    {
        return false;
    }
    if (state.closestSquared < m_edgeSenseDistSquared)
    {
        return true;
    }

    int consecutiveEmptyOctants = 0;
    for (int index : lookupTable)
    {
        if (state.pointInOctant[index] > -1)
        {
            consecutiveEmptyOctants = 0;
        }
        else
        {
            ++consecutiveEmptyOctants;
            if (consecutiveEmptyOctants == 4)
            {
                return false;
            }
        }
    }
    return true;
}

float XpandContext::weightedAverage(const float* values, const SearchState& state) const
{
    float sumSquared = 0.0f;
    float sumWeights = 0.0f;
    for (int index = 0; index < 8; ++index)
    {
        const long point = state.pointInOctant[index];
        if (point > -1)
        {
            const float distanceSquared = state.pointDistSquared[index];
            if (distanceSquared == 0.0f)
            {
                return values[point];
            }
            sumSquared += 1.0f / distanceSquared;
            sumWeights += (1.0f / distanceSquared) * values[point];
        }
    }
    if (sumSquared == 0.0f)
    {
        return m_options.undefinedValue;
    }
    return sumWeights / sumSquared;
}
