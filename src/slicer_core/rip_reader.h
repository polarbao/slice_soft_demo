#pragma once

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

struct RipValidationResult {
    std::filesystem::path package_dir;
    int width_px{0};
    int height_px{0};
    int layer_count{0};
    std::vector<RipLayerChecksum> layer_checksums;
};

RipValidationResult validate_slice_package(const std::filesystem::path& package_dir);

}  // namespace slicer_core
