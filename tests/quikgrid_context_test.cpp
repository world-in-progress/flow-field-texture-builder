#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "gridxtyp.h"
#include "xpand_context.h"

namespace {

void expectNear(float actual, float expected, const char* label)
{
    if (std::fabs(actual - expected) > 1.0e-5f)
    {
        std::cerr << label << ": expected " << expected << ", got " << actual << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    const std::array<double, 4> xs {0.0, 1.0, 0.0, 1.0};
    const std::array<double, 4> ys {0.0, 0.0, 1.0, 1.0};
    const std::array<float, 4> firstValues {10.0f, 20.0f, 30.0f, 40.0f};
    const std::array<float, 4> secondValues {-1.0f, -2.0f, -3.0f, -4.0f};
    const std::array<float, 2> gridXs {0.0f, 1.0f};
    const std::array<float, 2> gridYs {0.0f, 1.0f};

    XpandContext context;
    context.reset(xs.data(), ys.data(), xs.size(), gridXs.data(), gridXs.size(), gridYs.data(), gridYs.size(), XpandOptions{});

    std::array<float, 4> output {};
    context.interpolate(firstValues.data(), firstValues.size(), output.data(), output.size());

    expectNear(output[0], 10.0f, "first lower-left");
    expectNear(output[1], 20.0f, "first lower-right");
    expectNear(output[2], 30.0f, "first upper-left");
    expectNear(output[3], 40.0f, "first upper-right");

    context.interpolate(secondValues.data(), secondValues.size(), output.data(), output.size());

    expectNear(output[0], -1.0f, "second lower-left");
    expectNear(output[1], -2.0f, "second lower-right");
    expectNear(output[2], -3.0f, "second upper-left");
    expectNear(output[3], -4.0f, "second upper-right");

    const std::array<float, 2> unsortedGridXs {1.0f, 0.0f};
    bool rejectedUnsortedGrid = false;
    try
    {
        context.reset(xs.data(), ys.data(), xs.size(), unsortedGridXs.data(), unsortedGridXs.size(), gridYs.data(), gridYs.size(), XpandOptions{});
    }
    catch (const std::invalid_argument&)
    {
        rejectedUnsortedGrid = true;
    }
    if (!rejectedUnsortedGrid)
    {
        std::cerr << "context accepted unsorted grid coordinates\n";
        return 1;
    }

    bool rejectedOversizedGrid = false;
    try
    {
        context.reset(
                xs.data(),
                ys.data(),
                xs.size(),
                gridXs.data(),
                static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1U,
                gridYs.data(),
                gridYs.size(),
                XpandOptions{});
    }
    catch (const std::invalid_argument&)
    {
        rejectedOversizedGrid = true;
    }
    if (!rejectedOversizedGrid)
    {
        std::cerr << "context accepted oversized grid coordinates\n";
        return 1;
    }

    GridXType locator(2, 2, 9000);
    locator.setnext(111, 0, 8192);
    locator.setnext(222, 1, 0);
    locator.Sort();
    if (locator.Search(0, 8192, 0) != 111 || locator.Search(1, 0, 0) != 222)
    {
        std::cerr << "locator collided when grid height exceeded 8192\n";
        return 1;
    }

    return 0;
}
