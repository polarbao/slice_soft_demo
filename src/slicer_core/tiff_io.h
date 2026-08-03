#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace slicer_core {

constexpr int rgbwsv_channel_count{6};

enum class TiffStorageMode {
    Stripped,
    Tiled
};

std::string tiff_storage_mode_string(TiffStorageMode mode);

/**
 * @brief Identifies the TIFF payload compression used by each strip or tile.
 */
enum class TiffCompressionMode
{
    None,
    PackBits
};

/**
 * @brief Converts a TIFF compression mode to its stable configuration name.
 * @param mode Compression mode.
 * @return `none` or `packbits`.
 */
std::string TiffCompressionModeString(TiffCompressionMode mode);

/**
 * @brief Parse a stable TIFF compression configuration name.
 * @param name Compression name (`none` or `packbits`).
 * @return Matching compression mode.
 * @throws std::invalid_argument When the name is unsupported.
 */
TiffCompressionMode ParseTiffCompressionMode(std::string_view name);

struct TiffImageSpec {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t tile_width{0};
    std::uint32_t tile_height{0};
    std::uint32_t rows_per_strip{64};
    std::uint16_t samples_per_pixel{rgbwsv_channel_count};
    std::uint16_t bits_per_sample{8};
    std::uint16_t planar_config{1};
    TiffStorageMode storage_mode{TiffStorageMode::Stripped};
    TiffCompressionMode compression_mode{TiffCompressionMode::None};
};

struct TiffChannelStats {
    std::uint64_t print_pixels{0};
    std::uint64_t full_print_pixels{0};
    std::uint64_t partial_print_pixels{0};
    std::uint64_t empty_pixels{0};
    int min_value{255};
    int max_value{0};
};

struct TiffReadResult {
    TiffImageSpec spec;
    std::vector<std::uint8_t> pixels;
    std::array<std::uint64_t, rgbwsv_channel_count> channel_checksums{};
    std::array<TiffChannelStats, rgbwsv_channel_count> channel_stats{};
};

void write_rgbwsv_tiled_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    std::span<const std::uint8_t> pixels);

void write_rgbwsv_stripped_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    std::span<const std::uint8_t> pixels);

void write_rgbwsv_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    std::span<const std::uint8_t> pixels);

TiffReadResult read_rgbwsv_tiled_tiff(const std::filesystem::path& path);
TiffReadResult read_rgbwsv_stripped_tiff(const std::filesystem::path& path);
TiffReadResult read_rgbwsv_tiff(const std::filesystem::path& path);

}  // namespace slicer_core
