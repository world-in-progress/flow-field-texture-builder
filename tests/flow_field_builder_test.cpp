#include <array>
#include <cmath>
#include <cstring>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "flow_field_builder.h"
#include "stb_image.h"

namespace {

struct Image
{
    int width = 0;
    int height = 0;
    int components = 0;
    unsigned char* data = nullptr;

    explicit Image(const std::filesystem::path& path)
    {
        data = stbi_load(path.string().c_str(), &width, &height, &components, 4);
        components = 4;
        if (data == nullptr)
        {
            throw std::runtime_error("failed to load image: " + path.string());
        }
    }

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    ~Image()
    {
        stbi_image_free(data);
    }

    const unsigned char* pixel(int x, int y) const
    {
        return data + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U;
    }
};

float unpackFloat(const unsigned char* data)
{
    float value = 0.0f;
    std::memcpy(&value, data, sizeof(float));
    return value;
}

std::array<int, 2> unpackSeed(const unsigned char* data)
{
    return {static_cast<int>(data[0]) * 256 + static_cast<int>(data[1]),
            static_cast<int>(data[2]) * 256 + static_cast<int>(data[3])};
}

void expectNear(float actual, float expected, const char* label)
{
    if (std::fabs(actual - expected) > 1.0e-5f)
    {
        std::cerr << label << ": expected " << expected << ", got " << actual << '\n';
        std::exit(1);
    }
}

void expectSeed(const unsigned char* pixel, int expectedX, int expectedY, const char* label)
{
    const auto seed = unpackSeed(pixel);
    if (seed[0] != expectedX || seed[1] != expectedY)
    {
        std::cerr << label << ": expected (" << expectedX << ", " << expectedY << "), got ("
                  << seed[0] << ", " << seed[1] << ")\n";
        std::exit(1);
    }
}

std::filesystem::path outputDir()
{
    auto path = std::filesystem::current_path() / "flow_field_builder_test_output";
    std::filesystem::create_directories(path);
    return path;
}

void testRawSingleStepBuild(const std::filesystem::path& directory)
{
    const std::array<double, 4> xs {0.0, 1.0, 0.0, 1.0};
    const std::array<double, 4> ys {0.0, 0.0, 1.0, 1.0};
    const std::array<float, 4> us {10.0f, 20.0f, 30.0f, 40.0f};
    const std::array<float, 4> vs {-1.0f, -2.0f, -3.0f, -4.0f};

    FlowFieldBuilder builder(directory);
    builder.setDomainFromArrays(xs.data(), ys.data(), xs.size());
    builder.setQuikgridOptions(XpandOptions{});
    builder.setTextureSize(2, 2);
    builder.buildUvTexture(us.data(), vs.data(), us.size(), "raw");

    Image uv(builder.uvTexturePath("raw"));
    if (uv.width != 4 || uv.height != 2)
    {
        std::cerr << "unexpected uv texture size: " << uv.width << "x" << uv.height << '\n';
        std::exit(1);
    }
    expectNear(unpackFloat(uv.pixel(0, 1)), 10.0f, "raw lower-left u");
    expectNear(unpackFloat(uv.pixel(1, 1)), -1.0f, "raw lower-left v");
    expectNear(unpackFloat(uv.pixel(2, 0)), 40.0f, "raw upper-right u");
    expectNear(unpackFloat(uv.pixel(3, 0)), -4.0f, "raw upper-right v");

    Image seed(builder.seedTexturePath("raw"));
    if (seed.width != 2 || seed.height != 2)
    {
        std::cerr << "unexpected seed texture size: " << seed.width << "x" << seed.height << '\n';
        std::exit(1);
    }
    expectSeed(seed.pixel(0, 1), 0, 0, "raw lower-left seed");
    expectSeed(seed.pixel(1, 0), 1, 1, "raw upper-right seed");
}

void testVectorDomainHole(const std::filesystem::path& directory)
{
    const std::array<double, 9> xs {0.0, 1.0, 2.0, 0.0, 1.0, 2.0, 0.0, 1.0, 2.0};
    const std::array<double, 9> ys {0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 2.0, 2.0, 2.0};
    const std::array<float, 9> us {5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f};
    const std::array<float, 9> vs {6.0f, 6.0f, 6.0f, 6.0f, 6.0f, 6.0f, 6.0f, 6.0f, 6.0f};

    const auto vectorPath = directory / "domain_with_hole.geojson";
    std::ofstream vectorFile(vectorPath);
    vectorFile << R"({
  "type": "FeatureCollection",
  "features": [
    {
      "type": "Feature",
      "properties": {},
      "geometry": {
        "type": "Polygon",
        "coordinates": [
          [[0,0],[2,0],[2,2],[0,2],[0,0]],
          [[0.75,0.75],[0.75,1.25],[1.25,1.25],[1.25,0.75],[0.75,0.75]]
        ]
      }
    }
  ]
})";
    vectorFile.close();

    FlowFieldBuilder builder(directory);
    builder.setTextureSize(3, 3);
    builder.setDomainFromVector(xs.data(), ys.data(), xs.size(), vectorPath);
    builder.buildUvTexture(us.data(), vs.data(), us.size(), "hole");

    Image uv(builder.uvTexturePath("hole"));
    expectNear(unpackFloat(uv.pixel(2, 1)), 0.0f, "hole center u");
    expectNear(unpackFloat(uv.pixel(3, 1)), 0.0f, "hole center v");

    Image seed(builder.seedTexturePath("hole"));
    const auto centerSeed = unpackSeed(seed.pixel(1, 1));
    if (centerSeed[0] == 1 && centerSeed[1] == 1)
    {
        std::cerr << "hole center seeded to itself\n";
        std::exit(1);
    }
}

void testValidation(const std::filesystem::path& directory)
{
    const std::array<double, 4> xs {0.0, 1.0, 0.0, 1.0};
    const std::array<double, 4> ys {0.0, 0.0, 1.0, 1.0};
    const std::array<float, 4> values {1.0f, 2.0f, 3.0f, 4.0f};

    bool rejectedBuildBeforeDomain = false;
    try
    {
        FlowFieldBuilder builder(directory);
        builder.setTextureSize(2, 2);
        builder.buildUvTexture(values.data(), values.data(), values.size(), "invalid");
    }
    catch (const std::logic_error&)
    {
        rejectedBuildBeforeDomain = true;
    }
    if (!rejectedBuildBeforeDomain)
    {
        std::cerr << "builder accepted build before domain\n";
        std::exit(1);
    }

    bool rejectedBadOutputName = false;
    try
    {
        FlowFieldBuilder builder(directory);
        builder.setTextureSize(2, 2);
        builder.setDomainFromArrays(xs.data(), ys.data(), xs.size());
        builder.buildUvTexture(values.data(), values.data(), values.size(), "../escape");
    }
    catch (const std::invalid_argument&)
    {
        rejectedBadOutputName = true;
    }
    if (!rejectedBadOutputName)
    {
        std::cerr << "builder accepted path traversal output name\n";
        std::exit(1);
    }

    bool rejectedBackslashOutputName = false;
    try
    {
        FlowFieldBuilder builder(directory);
        builder.setTextureSize(2, 2);
        builder.setDomainFromArrays(xs.data(), ys.data(), xs.size());
        builder.buildUvTexture(values.data(), values.data(), values.size(), R"(nested\escape)");
    }
    catch (const std::invalid_argument&)
    {
        rejectedBackslashOutputName = true;
    }
    if (!rejectedBackslashOutputName)
    {
        std::cerr << "builder accepted backslash path output name\n";
        std::exit(1);
    }

    bool rejectedMismatchedVelocityCount = false;
    try
    {
        FlowFieldBuilder builder(directory);
        builder.setTextureSize(2, 2);
        builder.setDomainFromArrays(xs.data(), ys.data(), xs.size());
        builder.buildUvTexture(values.data(), values.data(), values.size() - 1, "mismatched");
    }
    catch (const std::invalid_argument&)
    {
        rejectedMismatchedVelocityCount = true;
    }
    if (!rejectedMismatchedVelocityCount)
    {
        std::cerr << "builder accepted mismatched velocity count\n";
        std::exit(1);
    }
}

} // namespace

int main()
{
    const auto directory = outputDir();
    testRawSingleStepBuild(directory);
    testVectorDomainHole(directory);
    testValidation(directory);
    return 0;
}
