#pragma once

#include "slicer_core/tiff_io.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace slicer_core {

enum class ValidationErrorCode {
    PackageNotFound,
    ManifestMissing,
    ManifestParseFailed,
    SchemaUnsupported,
    ChannelOrderInvalid,
    ChannelCountInvalid,
    BitDepthInvalid,
    PolarityInvalid,
    PrintEmptyValueInvalid,
    GridInvalid,
    LayerListInvalid,
    LayerCountMismatch,
    LayerMissing,
    LayerSizeMismatch,
    TiffOpenFailed,
    TiffSampleCountInvalid,
    TiffBitDepthInvalid,
    TiffPlanarConfigInvalid,
    TiffStorageModeInvalid,
    TiffStorageMismatch,
    RowsPerStripInvalid,
    TileSizeInvalid,
    TiffReadFailed,
};

std::string validation_error_code_string(ValidationErrorCode code);

class ValidationError : public std::runtime_error {
public:
    ValidationError(ValidationErrorCode code, const std::string& message);

    ValidationErrorCode code() const noexcept;

private:
    ValidationErrorCode code_;
};

struct RipLayerChecksum {
    int index{0};
    std::array<std::uint64_t, 6> channels{};
};

/**
 * @brief Strictly validated metadata and channel statistics for one RGBWSV package.
 */
struct RipValidationResult {
    std::filesystem::path package_dir;
    std::string schema;
    std::string storage_mode;
    int bit_depth{0};
    std::array<std::string, 6> channel_order{"R", "G", "B", "W", "S", "V"};
    int width_px{0};
    int height_px{0};
    int layer_count{0};
    int dpi_x{0};
    int dpi_y{0};
    double pixel_size_x_mm{0.0};
    double pixel_size_y_mm{0.0};
    int warnings_count{0};
    std::array<TiffChannelStats, 6> total_channel_stats{};
    std::vector<RipLayerChecksum> layer_checksums;
};

/**
 * @brief Validate one RGBWSV package, including grid, TIFF, and layer-list consistency.
 * @param package_dir Package root containing manifest.json and production TIFF layers.
 * @return Strict validation result with independent X/Y DPI and physical pixel sizes.
 */
RipValidationResult validate_slice_package(const std::filesystem::path& package_dir);

}  // namespace slicer_core
