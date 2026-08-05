#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core::api {

/** @brief Summary of a validated RGBWSV package. */
struct PackageSummary
{
    std::filesystem::path package_dir;
    std::string schema;
    int layer_count{0};
    int width_px{0};
    int height_px{0};
    int dpi_x{0};
    int dpi_y{0};
};

/** @brief Descriptor for one production TIFF layer. */
struct LayerDescriptor
{
    int layer_index{0};
    double z_mm{0.0};
    std::filesystem::path tiff_path;
    std::array<std::uint64_t, 6> print_pixels{};
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

/** @brief Strict package verification result. */
struct VerifyResult
{
    bool valid{false};
    std::vector<std::string> warnings;
};

}  // namespace slicer_core::api
