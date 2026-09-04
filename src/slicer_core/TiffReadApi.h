#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace slicer_core
{

constexpr int rgbwsv_channel_count{6};

enum class TiffStorageMode
{
    Stripped,
    Tiled
};

/**
 * @brief 将 TIFF 存储模式转换为稳定协议名。
 * @param mode 存储模式。
 * @return `stripped`、`tiled` 或 `unknown`。
 */
std::string tiff_storage_mode_string(TiffStorageMode mode);

/**
 * @brief 标识每个条带或分块使用的 TIFF 载荷压缩方式。
 */
enum class TiffCompressionMode
{
    None,
    PackBits
};

/**
 * @brief 将 TIFF 压缩模式转换为稳定配置名。
 * @param mode 压缩模式。
 * @return `none` 或 `packbits`。
 */
std::string TiffCompressionModeString(TiffCompressionMode mode);

/**
 * @brief 解析稳定的 TIFF 压缩配置名。
 * @param name 压缩名称（`none` 或 `packbits`）。
 * @return 匹配的压缩模式。
 * @throws std::invalid_argument 名称不受支持时抛出。
 */
TiffCompressionMode ParseTiffCompressionMode(std::string_view name);

/**
 * @brief 描述一幅 RGBWSV/RGBWSVT TIFF 图像及其存储布局。
 */
struct TiffImageSpec
{
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

/**
 * @brief 包含按生产极性计算的单个 TIFF 通道统计。
 */
struct TiffChannelStats
{
    std::uint64_t print_pixels{0};
    std::uint64_t full_print_pixels{0};
    std::uint64_t partial_print_pixels{0};
    std::uint64_t empty_pixels{0};
    int min_value{255};
    int max_value{0};
};

/**
 * @brief 包含已解码 RGBWSV 图像及其权威统计。
 */
struct TiffReadResult
{
    TiffImageSpec spec;
    std::vector<std::uint8_t> pixels;
    std::array<std::uint64_t, rgbwsv_channel_count> channel_checksums{};
    std::array<TiffChannelStats, rgbwsv_channel_count> channel_stats{};
};

/**
 * @brief 读取一幅分块存储的 RGBWSV uint8 TIFF。
 * @param path 源 TIFF 路径。
 * @return 已解码图像、存储元数据、校验和及通道统计。
 */
TiffReadResult read_rgbwsv_tiled_tiff(const std::filesystem::path& path);

/**
 * @brief 读取一幅条带存储的 RGBWSV uint8 TIFF。
 * @param path 源 TIFF 路径。
 * @return 已解码图像、存储元数据、校验和及通道统计。
 */
TiffReadResult read_rgbwsv_stripped_tiff(const std::filesystem::path& path);

/**
 * @brief 读取一种受支持存储布局的 RGBWSV uint8 TIFF。
 * @param path 源 TIFF 路径。
 * @return 已解码图像、存储元数据、校验和及通道统计。
 */
TiffReadResult read_rgbwsv_tiff(const std::filesystem::path& path);

}  // namespace slicer_core
