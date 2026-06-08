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
    int bit_depth{8};
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

struct AutoOrientConfig {
    bool enabled{true};
    double max_height_mm{6.0};
    std::string strategy{"minimize_height_by_right_angle_rotation"};
};

struct BackgroundConfig {
    std::uint8_t value{255};
};

struct MaterialConfig {
    std::string material_channel{"auto"};
    std::string apply_mode{"solid_volume"};
    std::array<std::uint8_t, 3> rgb{255, 255, 255};
    std::uint8_t white_value{255};
    std::uint8_t varnish_value{255};
};

struct TextureConfig {
    bool enabled{false};
    std::string apply_mode{"solid_volume_from_top_surface"};
    std::string sampler{"bilinear"};
    std::string uv_address_mode{"clamp"};
    bool flip_v{true};
    std::array<std::uint8_t, 3> fallback_rgb{0, 0, 0};
    std::string missing_texture_policy{"warn_and_fallback"};
};

struct RgbPolicyConfig {
    bool enabled{true};
    std::string source{"texture_or_fallback"};
};

struct WhitePolicyConfig {
    bool enabled{false};
    std::string mode{"disabled"};
    std::uint8_t value{0};
    std::string layers{"all_model"};
};

struct VarnishPolicyConfig {
    bool enabled{false};
    std::string mode{"disabled"};
    std::uint8_t value{0};
    int top_layers{1};
};

struct MaterialPolicyConfig {
    bool enabled{false};
    RgbPolicyConfig rgb;
    WhitePolicyConfig white;
    VarnishPolicyConfig varnish;
    std::string conflict_policy{"model_material_over_support"};
};

struct SupportConfig {
    bool enabled{true};
    std::string mode{"bottom_projection"};
    std::uint8_t value{0};
    double offset_mm{0.0};
    int min_area_px{0};
    double min_overlap_ratio{0.2};
    int min_island_area_px{16};
    int connectivity{8};
    std::string unsupported_projection{"project_to_build_plate"};
    int xy_dilation_px{0};
    bool write_support_type_debug{true};
};

struct PreviewConfig {
    bool enabled{false};
    std::string format{"ppm"};
    int interval{10};
    bool has_layer_range{false};
    std::array<int, 2> layer_range{0, -1};
    std::vector<std::string> channels{"rgb", "support", "white", "varnish"};
    bool only_non_empty_layers{false};
};

struct ReliefConfig {
    std::string fill_mode{"surface_to_base"};
    double base_z_mm{0.0};
};

struct SliceConfig {
    std::string slicing_mode{"closed_mesh_scanline"};
    InputConfig input;
    OutputConfig output;
    TransformConfig transform;
    AutoOrientConfig auto_orient;
    BackgroundConfig background;
    MaterialConfig material;
    TextureConfig texture;
    MaterialPolicyConfig material_policy;
    SupportConfig support;
    PreviewConfig preview;
    ReliefConfig relief;
};

SliceConfig load_slice_config(const std::filesystem::path& config_path);
void validate_slice_config(const SliceConfig& config);

}  // namespace slicer_core
