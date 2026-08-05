#pragma once

#include "slicer_core/config/SlicePipelineConfig.h"
#include "slicer_core/diagnostics/ValidationIssue.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace slicer_core {

inline constexpr int kDefaultOutputDpiX{635};
inline constexpr int kDefaultOutputDpiY{600};
inline constexpr double kDefaultLayerThicknessMm{0.038};
inline constexpr int kMinimumOutputDpi{72};
inline constexpr int kMaximumOutputDpi{2400};
inline constexpr double kMillimetersPerInch{25.4};
inline constexpr double kOutputPixelSizeToleranceMm{1.0e-9};

/**
 * @brief Check whether one output-axis DPI value is supported.
 * @param dpi Output resolution for one raster axis.
 * @return True when dpi is inside the shared defensive range.
 */
bool IsSupportedOutputDpi(int dpi) noexcept;

/**
 * @brief Check whether a physical pixel size matches one output-axis DPI.
 * @param dpi Output resolution for one raster axis.
 * @param pixelSizeMm Physical pixel size in millimeters.
 * @return True when both values are valid and consistent within protocol tolerance.
 */
bool IsOutputPixelSizeConsistent(
    int dpi,
    double pixelSizeMm) noexcept;

struct InputConfig {
    std::filesystem::path model_path;
    std::string format{"auto"};
};

struct OutputConfig {
    std::filesystem::path package_dir{"output/SlicePackage"};
    int dpi_x{kDefaultOutputDpiX};
    int dpi_y{kDefaultOutputDpiY};
    double layer_thickness_mm{kDefaultLayerThicknessMm};
    std::vector<std::string> channel_order{"R", "G", "B", "W", "S", "V"};
    int bit_depth{8};
    std::string planar_config{"contiguous"};
    std::string storage_mode{"stripped"};
    std::string tiff_compression{"none"};
    std::optional<std::string> white_semantics;
    bool tiled{false};
    std::array<int, 2> tile_size{256, 256};
    int rows_per_strip{64};
};

struct TransformConfig {
    std::string unit{"mm"};
    std::array<double, 3> scale{1.0, 1.0, 1.0};
    std::array<double, 3> rotation_deg{0.0, 0.0, 0.0};
    std::array<double, 3> translation_mm{0.0, 0.0, 0.0};
};

struct AutoOrientConfig {
    bool enabled{true};
    double max_height_mm{9.0};
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

/**
 * @brief Stage 12E global 3D texture-surface shell configuration.
 */
struct TextureSurfaceShellConfig
{
    std::string geometry_mode{"global_3d_distance"};
    std::string mode{"partial_shell"};
    double width_mm{0.10};
    double width_step_mm{0.01};
    std::string minimum_width_policy{"two_cells_floor_0_10_mm"};
    std::string surface_scope{"all_closed_surfaces"};
    bool full_texture_at_model_limit{true};
};

struct TextureConfig {
    bool enabled{false};
    std::string apply_mode{"solid_volume_from_top_surface"};
    int top_surface_layers{1};
    std::string sampler{"bilinear"};
    std::string uv_address_mode{"clamp"};
    bool flip_v{true};
    std::array<std::uint8_t, 3> fallback_rgb{0, 0, 0};
    std::string missing_texture_policy{"warn_and_fallback"};
    std::string non_surface_rgb_policy{"model_material"};
    std::string unprintable_white_policy{"fail_closed"};
    std::uint8_t unprintable_white_ink_threshold{0};
    std::uint8_t unprintable_white_value{0};
    TextureSurfaceShellConfig surface_shell;
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

/**
 * @brief Explicit model fill semantics for Stage 12A material composition.
 */
struct ModelFillConfig {
    bool enabled{false};
    std::string material{"white"};
    std::string scope{"below_texture_surface"};
    std::uint8_t value{0};
    bool empty_allowed_in_production{false};
    bool legacy_rgb_fallback{false};
};

struct MaterialProcessRgbConfig {
    bool enabled{true};
    std::string source{"texture_or_color"};
};

struct MaterialProcessWhiteConfig {
    bool enabled{false};
    std::string mode{"underbase"};
    std::string coverage{"all_model"};
    std::uint8_t value{0};
    int expand_px{0};
    int shrink_px{0};
};

struct MaterialProcessVarnishConfig {
    bool enabled{false};
    std::string mode{"top_n_layers"};
    int top_layers{2};
    std::uint8_t value{0};
    std::string coverage{"model_surface"};
};

struct MaterialProcessSupportConfig {
    bool expected{false};
    std::string mode{"existing_support_pipeline"};
};

struct MaterialProcessValidationConfig {
    bool require_rgb_pixels{false};
    bool require_white_pixels{false};
    bool require_varnish_pixels{false};
    bool require_support_pixels{false};
    int max_unexpected_overlap_pixels{0};
};

struct MaterialProcessProfileConfig {
    bool enabled{false};
    std::string name;
    std::string target;
    MaterialProcessRgbConfig rgb;
    MaterialProcessWhiteConfig white;
    MaterialProcessVarnishConfig varnish;
    MaterialProcessSupportConfig support;
    MaterialProcessValidationConfig validation;
};

struct MaterialRoleRuleConfig {
    std::string match_name_contains;
    std::string role{"rgb"};
};

struct MaterialRoleMappingConfig {
    bool enabled{false};
    std::string mode{"rules_then_default"};
    std::string default_role{"rgb"};
    bool allow_input_support_material{false};
    std::vector<MaterialRoleRuleConfig> rules;
};

/**
 * @brief Internal void support options for enclosed non-model regions.
 */
struct InternalVoidSupportConfig {
    bool enabled{true};
    int min_area_px{16};
    std::string fill_rule{"all_internal_voids"};
};

/**
 * @brief Upper detachable support placement options.
 */
struct UpperSupportConfig {
    bool enabled{false};
    std::string outside{"outer_varnish_shell"};
    std::string reason{"optional_detachable_surface_support"};
};

/**
 * @brief Optional maximum support-footprint base applied to the first layers.
 */
struct SupportBaseProjectionConfig
{
    bool enabled{false};
    int layer_count{30};
    std::string layer_placement{"overlay_existing"};
    std::string source{"max_support_footprint"};
};

struct SupportConfig {
    bool enabled{true};
    std::string mode{"bottom_projection"};
    std::string placement{"lower"};
    bool placement_explicit{false};
    std::uint8_t value{0};
    double offset_mm{0.0};
    int min_area_px{0};
    double min_overlap_ratio{0.2};
    int min_island_area_px{16};
    int connectivity{8};
    std::string unsupported_projection{"project_to_build_plate"};
    int xy_dilation_px{0};
    bool write_support_type_debug{true};
    bool shape_enabled{false};
    int shape_min_component_area_px{0};
    int shape_xy_dilation_px{0};
    int shape_closing_radius_px{0};
    int shape_bridge_gap_px{0};
    bool shape_preserve_model_priority{true};
    double shape_max_added_support_ratio{0.25};
    InternalVoidSupportConfig internal_void;
    UpperSupportConfig upper;
    SupportBaseProjectionConfig base_projection;
};

/**
 * @brief Optional outside varnish shell configuration.
 */
struct OuterVarnishShellConfig {
    bool enabled{false};
    double thickness_mm{0.0};
    double thickness_step_mm{0.01};
    double pixel_pitch_um{42.3};
    bool allow_xy_expansion{true};
    std::string conflict_policy{"varnish_shell_wins"};
    std::uint8_t value{0};
};

/**
 * @brief Optional varnish layer written on model surface pixels.
 */
struct SurfaceVarnishConfig {
    bool enabled{false};
    bool outer_surface{true};
    bool inner_surface{true};
    int thickness_px{1};
    std::uint8_t value{0};
    std::string source{"explicit"};
};

/**
 * @brief Material assignment rules reserved for exact one-pixel closure repair.
 */
struct MaterialClosureRepairConfig
{
    bool enabled{false};
    std::string color_fill_gap{"model_fill"};
    std::string model_support_gap{"contextual"};
    std::string internal_void_gap{"support"};
    std::string varnish_support_gap{"support"};
};

/**
 * @brief Stage 12D material-closure diagnostic configuration.
 */
struct MaterialClosureConfig
{
    bool enabled{true};
    std::string mode{"diagnostic"};
    int connectivity{8};
    int max_gap_px{1};
    MaterialClosureRepairConfig repair;
    bool fail_on_gap{true};
    bool write_gap_preview{false};
};

/**
 * @brief Diagnostic image output policy and display-only rendering settings.
 */
struct PreviewConfig {
    std::string output_policy{"tiff_native"};
    bool enabled{false};
    std::string format{"ppm"};
    int interval{10};
    bool has_layer_range{false};
    std::array<int, 2> layer_range{0, -1};
    std::vector<std::string> channels{"rgb", "support", "white", "varnish"};
    bool only_non_empty_layers{false};
    std::array<std::uint8_t, 3> empty_color{255, 255, 255};
    std::array<std::uint8_t, 3> support_color{0, 255, 0};
    std::array<std::uint8_t, 3> white_color{0, 170, 255};
    std::array<std::uint8_t, 3> varnish_color{127, 127, 127};
};

struct ReliefConfig {
    std::string fill_mode{"surface_to_base"};
    double base_z_mm{0.0};
};

/**
 * @brief Feature-gated configuration for the experimental OpenVDB pipeline.
 */
struct ExperimentalOpenVdbPipelineConfig
{
    bool enabled{false};
    std::string engine{"legacy"};
    std::string admission_mode{"strict_closed"};
    std::string failure_policy{"fail_fast"};
    bool allow_non_production_output{false};
    bool write_production_rgbwsv{false};
};

/**
 * @brief Experimental configuration namespace; all fields are safe-off by default.
 */
struct ExperimentalConfig
{
    ExperimentalOpenVdbPipelineConfig openvdb_pipeline;
};

struct SliceConfig {
    std::optional<std::string> white_semantics;
    std::string slicing_mode{"closed_mesh_scanline"};
    SlicePipelineConfig slice_pipeline;
    InputConfig input;
    OutputConfig output;
    TransformConfig transform;
    AutoOrientConfig auto_orient;
    BackgroundConfig background;
    MaterialConfig material;
    TextureConfig texture;
    MaterialPolicyConfig material_policy;
    ModelFillConfig model_fill;
    MaterialProcessProfileConfig material_process_profile;
    MaterialRoleMappingConfig material_role_mapping;
    SupportConfig support;
    OuterVarnishShellConfig outer_varnish;
    SurfaceVarnishConfig surface_varnish;
    MaterialClosureConfig material_closure;
    PreviewConfig preview;
    ReliefConfig relief;
    ExperimentalConfig experimental;
};

SliceConfig load_slice_config(const std::filesystem::path& config_path);
void validate_slice_config(const SliceConfig& config);

/**
 * @brief Resolve the job-level white semantics written to the package manifest.
 * @param config Validated slice Profile and output configuration.
 * @return Manifest override when present, otherwise the Profile default; empty for legacy Profiles.
 */
std::optional<std::string> ResolveWhiteSemantics(const SliceConfig& config);

/**
 * @brief Build diagnostics for experimental OpenVDB pipeline configuration.
 * @param config Slice configuration.
 * @return Stable validation issues; empty when no experimental diagnostic is required.
 */
std::vector<ValidationIssue> BuildExperimentalOpenVdbPipelineDiagnostics(const SliceConfig& config);

}  // namespace slicer_core
