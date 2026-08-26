#pragma once

#include "slicer_core/api/CommonDtos.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core::api {

/** @brief 生产包摘要携带的输出栅格。 */
struct PackageGrid
{
    int width_px{0};
    int height_px{0};
    int dpi_x{0};
    int dpi_y{0};
};

/** @brief 已验证 RGBWSV 生产包的摘要。 */
struct PackageSummary
{
    std::filesystem::path package_dir;
    std::string package_identity;
    std::string schema;
    std::string production_acceptance;
    int layer_count{0};
    PackageGrid grid;
    std::vector<std::string> channels{"R", "G", "B", "W", "S", "V"};
    int bit_depth{8};
    std::string polarity{"black_is_print"};
    std::vector<StructuredJsonObject> per_instance;
    StructuredJsonObject profile_echo;
};

/** @brief 一个生产 TIFF 层的描述符。 */
struct LayerDescriptor
{
    int layer_index{0};
    double z_mm{0.0};
    int width_px{0};
    int height_px{0};
    std::filesystem::path tiff_path;
    std::vector<std::string> channels{"R", "G", "B", "W", "S", "V"};
    std::vector<std::uint64_t> print_pixels{6U, 0U};
    std::vector<std::uint64_t> empty_pixels{6U, 0U};
    std::string storage_mode;
};

/** @brief 从生产 TIFF 解码仅供显示预览的请求。 */
struct PreviewRequest
{
    std::filesystem::path package_dir;
    int layer_index{0};
    std::string mode{"composite"};
    std::vector<std::string> channels;
    int max_width_px{1024};
    std::filesystem::path output_path;
};

/** @brief 生产 TIFF 预览渲染结果。 */
struct PreviewResult
{
    std::filesystem::path output_path;
    int width_px{0};
    int height_px{0};
    std::string cache_key;
};

/** @brief 生产包验证报告中的结构化失败项。 */
struct PackageValidationError
{
    std::string code;
    std::string message;
};

/** @brief 生产包严格验证结果。 */
struct VerifyResult
{
    bool valid{false};
    std::string production_acceptance;
    std::vector<PackageValidationError> errors;
    std::vector<std::vector<std::uint64_t>> per_layer_checksum;
    int layer_count{0};
    std::vector<std::string> warnings;
};

/** @brief 携带 schema 和已验证 JSON 对象的具名生产包报告。 */
struct PackageReport
{
    std::string report_name;
    std::string report_schema;
    StructuredJsonObject data;
    std::filesystem::path source_path;
};

}  // namespace slicer_core::api
