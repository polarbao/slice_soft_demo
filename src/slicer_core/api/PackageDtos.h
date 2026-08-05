#pragma once

#include "slicer_core/api/CommonDtos.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core::api {

/** @brief Output grid carried by a package summary. */
struct PackageGrid
{
    int width_px{0};
    int height_px{0};
    int dpi_x{0};
    int dpi_y{0};
};

/** @brief Summary of a validated RGBWSV package. */
struct PackageSummary
{
    std::filesystem::path package_dir;
    std::string package_identity;
    std::string schema;
    int layer_count{0};
    PackageGrid grid;
    std::array<std::string, 6> channels{"R", "G", "B", "W", "S", "V"};
    int bit_depth{8};
    std::string polarity{"black_is_print"};
    std::vector<StructuredJsonObject> per_instance;
    StructuredJsonObject profile_echo;
};

/** @brief Descriptor for one production TIFF layer. */
struct LayerDescriptor
{
    int layer_index{0};
    double z_mm{0.0};
    int width_px{0};
    int height_px{0};
    std::filesystem::path tiff_path;
    std::array<std::uint64_t, 6> print_pixels{};
    std::array<std::uint64_t, 6> empty_pixels{};
    std::string storage_mode;
};

/** @brief Request for a display-only preview decoded from production TIFF. */
struct PreviewRequest
{
    std::filesystem::path package_dir;
    int layer_index{0};
    std::string mode{"composite"};
    std::vector<std::string> channels;
    int max_width_px{1024};
    std::filesystem::path output_path;
};

/** @brief Result of production TIFF preview rendering. */
struct PreviewResult
{
    std::filesystem::path output_path;
    int width_px{0};
    int height_px{0};
    std::string cache_key;
};

/** @brief Structured validation failure reported by package verification. */
struct PackageValidationError
{
    std::string code;
    std::string message;
};

/** @brief Strict package verification result. */
struct VerifyResult
{
    bool valid{false};
    std::vector<PackageValidationError> errors;
    std::vector<std::array<std::uint64_t, 6>> per_layer_checksum;
    int layer_count{0};
    std::vector<std::string> warnings;
};

/** @brief Named package report with its schema and validated object payload. */
struct PackageReport
{
    std::string report_name;
    std::string report_schema;
    StructuredJsonObject data;
    std::filesystem::path source_path;
};

}  // namespace slicer_core::api
