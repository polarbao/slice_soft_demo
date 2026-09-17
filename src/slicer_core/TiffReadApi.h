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
enum class TiffRowOrder { MinYFirst, MaxYFirst };

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
    // On-disk row order. Reader pixels and Writer inputs are always minY-first.
    TiffRowOrder row_order{TiffRowOrder::MinYFirst};
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

/**
 * @brief 只解出规格、逐通道统计与校验和，**不物化整幅面像素**。
 *
 * MF-13c。包发布的读回全量校验只用 `spec` / `channel_stats` /
 * `channel_checksums`，从不读 `pixels`；而物化一层是
 * `width * height * 6` 字节（本场景 44 MB/层，整包 143 层约 6.3 GB 的
 * 分配与拷贝）。跳过它对校验判据**没有任何影响**：越界判据改用解析出的
 * 整幅面字节数，与物化时逐字一致；统计仍由同一个
 * `AccumulateContiguousChannelStats` 从条带载荷直接累计。
 *
 * 返回值的 `pixels` 为空 —— **需要像素的调用方必须用
 * `read_rgbwsv_tiff`**。当前仅 stripped 存储走此优化（生产包一律 stripped）；
 * tiled 仍照原样物化，行为不变。
 */
TiffReadResult read_rgbwsv_tiff_stats(const std::filesystem::path& path);

/// TIFF 声明的解码尺寸相对【文件实际大小】的最大可信膨胀比（F-52）。
///
/// PackBits 的理论上限是 64:1（2 字节编码 128 个重复字节），这里取 128 留一倍余量。
/// 实测真实产物的比值是 1.0–1.2:1，离该上界有约百倍余量。
///
/// 刻意【不】设绝对上限：那需要一个拿不出证据的数字，定小了会打断大幅面生产。
/// 比值判据有原理依据，且正好挡住本条要挡的东西——「文件很小却声称自己很大」。
constexpr std::uintmax_t kMaxTiffDecodedExpansion{128U};

/**
 * @brief 声明尺寸算出的解码字节数若远超文件所能承载的上界即抛出（F-52）。
 * @param decodedBytes 按文件头的宽×高×通道算出的字节数。
 * @param fileBytes 文件实际字节数；0 表示未知，此时不作判断。
 * @param path 仅用于错误信息。
 * @throws std::runtime_error 超出可信膨胀比时。
 */
void EnsureDecodedSizeIsPlausible(
    std::uintmax_t decodedBytes,
    std::uintmax_t fileBytes,
    const std::filesystem::path& path);

}  // namespace slicer_core
