#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "flow_field.h"
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
    const auto root = std::filesystem::current_path() / "flow_field_cli_adapter_test_output";
    const auto input = root / "input";
    const auto output = root / "output";
    std::filesystem::create_directories(input);
    std::filesystem::create_directories(output);

    {
        std::ofstream dataFile(input / "step.txt");
        dataFile << "4\n";
        dataFile << "x y u v\n";
        dataFile << "0 0 10 -1\n";
        dataFile << "1 0 20 -2\n";
        dataFile << "0 1 30 -3\n";
        dataFile << "1 1 40 -4\n";
    }

    const auto descriptionPath = root / "description.json";
    {
        std::ofstream description(descriptionPath);
        description << R"({
  "input_url": "input",
  "output_url": "output",
  "column_num": 4,
  "coordinates": ["x", "y"],
  "velocities": ["u", "v"],
  "outlier": -99999,
  "headings": ["x", "y", "u", "v"],
  "resolution": 2,
  "boundary": [0, 0, 1, 1],
  "vector_mask": "",
  "source_space": "4326",
  "target_space": [],
  "files": [
    {"in": "step.txt", "sn": "step"}
  ]
})";
    }

    Json result;
    FlowField flowField(descriptionPath.string().c_str(), result);
    flowField.buildTextures();

    if (result.contains("flow_boundary"))
    {
        std::cerr << "CLI adapter emitted flow_boundary\n";
        return 1;
    }
    if (result["flow_fields"].size() != 1 || result["seed_textures"].size() != 1 || result["area_masks"].size() != 1)
    {
        std::cerr << "CLI adapter did not emit expected texture paths\n";
        return 1;
    }

    Image uv(output / "uv_step.png");
    if (uv.width != 4 || uv.height != 2)
    {
        std::cerr << "unexpected CLI uv texture size: " << uv.width << "x" << uv.height << '\n';
        return 1;
    }
    expectNear(unpackFloat(uv.pixel(0, 1)), 10.0f, "CLI lower-left u");
    expectNear(unpackFloat(uv.pixel(1, 1)), -1.0f, "CLI lower-left v");
    expectNear(unpackFloat(uv.pixel(2, 0)), 40.0f, "CLI upper-right u");
    expectNear(unpackFloat(uv.pixel(3, 0)), -4.0f, "CLI upper-right v");

    Image seed(output / "seed_step.png");
    if (seed.width != 2 || seed.height != 2)
    {
        std::cerr << "unexpected CLI seed texture size: " << seed.width << "x" << seed.height << '\n';
        return 1;
    }

    return 0;
}
