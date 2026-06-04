#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace slicer_core {

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

