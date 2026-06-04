#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace slicer_core {

constexpr int rgbwsv_channel_count{6};

struct TiffImageSpec {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t tile_width{0};
    std::uint32_t tile_height{0};
    std::uint16_t samples_per_pixel{rgbwsv_channel_count};
    std::uint16_t bits_per_sample{16};
    std::uint16_t planar_config{1};
};

struct TiffReadResult {
    TiffImageSpec spec;
    std::array<std::uint64_t, rgbwsv_channel_count> channel_checksums{};
};

void write_rgbwsv_tiled_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    const std::vector<std::uint16_t>& pixels);

TiffReadResult read_rgbwsv_tiled_tiff(const std::filesystem::path& path);

}  // namespace slicer_core

