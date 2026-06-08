#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core {

struct TextureImage {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> rgba;
};

struct TextureSampleOptions {
    std::string sampler{"bilinear"};
    std::string uv_address_mode{"clamp"};
    bool flip_v{true};
};

TextureImage load_texture_image(const std::filesystem::path& path);
std::array<std::uint8_t, 3> sample_texture_rgb(
    const TextureImage& image,
    double u,
    double v,
    const TextureSampleOptions& options,
    bool& uv_out_of_range);

}  // namespace slicer_core
