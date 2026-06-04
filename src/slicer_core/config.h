#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core {

struct InputConfig {
    std::filesystem::path model_path;
    std::string format{"auto"};
};

struct OutputConfig {
    std::filesystem::path package_dir{"output/SlicePackage"};
    int dpi_x{600};
    int dpi_y{600};
    double layer_thickness_mm{0.01};
    std::vector<std::string> channel_order{"R", "G", "B", "W", "S", "V"};
    int bit_depth{16};
    std::string planar_config{"contiguous"};
    bool tiled{true};
    std::array<int, 2> tile_size{256, 256};
};

struct TransformConfig {
    std::string unit{"mm"};
    std::array<double, 3> scale{1.0, 1.0, 1.0};
    std::array<double, 3> rotation_deg{0.0, 0.0, 0.0};
    std::array<double, 3> translation_mm{0.0, 0.0, 0.0};
};

struct MaterialConfig {
    std::array<std::uint16_t, 3> rgb{65535, 65535, 65535};
    std::uint16_t white_strength{0};
    std::uint16_t varnish_strength{0};
};

struct SupportConfig {
    bool enabled{true};
    std::string mode{"bottom_projection"};
    std::uint16_t strength{65535};
    double offset_mm{0.0};
    int min_area_px{0};
};

struct PreviewConfig {
    bool enabled{false};
    int interval{10};
    std::vector<std::string> channels{"rgb", "support"};
};

struct SliceConfig {
    InputConfig input;
    OutputConfig output;
    TransformConfig transform;
    MaterialConfig material;
    SupportConfig support;
    PreviewConfig preview;
};

SliceConfig load_slice_config(const std::filesystem::path& config_path);
void validate_slice_config(const SliceConfig& config);

}  // namespace slicer_core

