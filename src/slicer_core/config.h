#pragma once

#include "slicer_core/config/SlicePipelineConfig.h"
#include "slicer_core/config/OutputResolution.h"
#include "slicer_core/diagnostics/ValidationIssue.h"
#include "slicer_core/model/ModelLoadConfig.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace slicer_core {

struct OutputConfig {
    std::filesystem::path package_dir{"output/SlicePackage"};
    std::string package_protocol{"p0.rgbwsv.2"};
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

struct MaterialVolumeOpenSurfaceConfig {
    std::string mode{"reject"};
    double thickness_mm{0.0};
    std::string placement{"below_surface"};
};

struct MaterialVolumeOverlapRuleConfig {
    std::string match_material_name;
    int priority{0};
};

/**
 * @brief 材质体积重叠的裁决方式。
 *
 * `explicit_priority`（默认）要求逐材质显式声明 priority，缺声明或同级均 fail-closed。
 * `auto_by_material_name` 改由命名规范推导：材质名形如 `<素材名>-L<层号>`，
 * 优先级按「层序主导 + 同层类别次序」自动生成，`rules` 此时必须为空。
 * 规范见 docs/slice/DOC/DOC_SPEC_MATERIAL_NAMING_多图层素材命名与语义标识规范.md。
 */
struct MaterialVolumeOverlapConfig {
    std::string mode{"explicit_priority"};
    std::vector<MaterialVolumeOverlapRuleConfig> rules;
};

/**
 * @brief Per-layer per-pixel material ownership policy; safe-off by default.
 */
/// @brief 拓扑准入口径。
///
/// `selfIntersectionPolicy = reject`（默认）沿用既有行为：分类为 self_intersecting 即拒绝。
///
/// `tolerate_closed_self_intersection` 是【有界放宽】，其口径必须如实理解：
/// - 前置：该材质必须仍是闭合曲面（真开边与非流形边均为 0）。此时由 Jordan–Brouwer，
///   一般位置射线的交点数恒为偶数，区间因此【形状良好】，不会触发 IntersectionUnpaired。
///   注意这意味着奇偶性对本类恒真，不能把它当作安全性证据。
/// - 未被消除的风险：自交处缠绕数可能大于 1，而奇偶法则把双重覆盖判为外部，
///   故【自交邻域内的材质归属可能错误】。误差范围被相交三角面的投影界住。
/// - `maxSelfIntersectionPairs` 把放宽限制在【局部缺陷】：仓库中被判定「需重建」的资产
///   自交对数为数千至数万，远超该上限，仍会 fail closed，本放宽不是对它们的旁路。
/// - 放行材质名必须进入 plan 并由报告披露，不得静默。
struct MaterialVolumeTopologyConfig {
    std::string self_intersection_policy{"reject"};
    int max_self_intersection_pairs{64};
    /// 真开边数上限（MQ-06）。默认 0 即要求闭合，与放宽前行为一致。
    /// 设为正数后允许有界开边进入区间求解，由逐列 IntersectionUnpaired 兜底——
    /// 注意开边存在时 Jordan–Brouwer 不再保证偶数交点，故该门在本类上是
    /// 【真检查而非恒真】，这正是本次放宽依据强于 MQ-05 的原因。
    int max_boundary_edges{0};
};

/**
 * @brief 由材质不透明度推导光油（V）归属；默认关闭，须显式 opt-in。
 *
 * 判据只看 MTL 的 `d`（`Tr` 已在解析层归一为同一 opacity）。
 * `opacity <= opacity_max` 的材质其体积改写 V 通道而非 RGB。
 */
struct MaterialVolumeOpacityVarnishConfig {
    bool enabled{false};
    /// @brief 判为光油的不透明度上限；必须为正且 < 1。
    double opacity_max{0.001};
    /**
     * @brief `opacity_max < opacity < 1` 的半透明材质的落位角色。
     *
     * 只允许 `rgb`：半透明按工艺语义【不是】光油。取该值时必须出诊断，
     * 不得静默丢弃设计意图——静默丢弃正是本专项要根治的原始缺陷。
     */
    std::string semi_transparent_role{"rgb"};
};

struct MaterialVolumePolicyConfig {
    bool enabled{false};
    std::string mode{"closed_intervals"};
    MaterialVolumeOpenSurfaceConfig open_surface;
    MaterialVolumeOverlapConfig overlap;
    MaterialVolumeTopologyConfig topology;
    std::string missing_material{"fail_closed"};
    MaterialVolumeOpacityVarnishConfig opacity_varnish;
};

/**
 * @brief Explicit material-colour routing for the optional transfer channel.
 *
 * Colours are profile data. The implementation must never infer this role
 * from a material name or a model file name.
 */
struct TransferChannelPolicyConfig {
    bool enabled{false};
    std::string match_source{"material_diffuse_rgb"};
    std::vector<std::array<std::uint8_t, 3>> material_diffuse_rgb_values;
    std::string missing_region{"allow_empty"};
    std::string multiple_matches{"fail_closed"};
    std::uint8_t value{0};
    MaterialVolumeTopologyConfig topology;
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
    std::array<std::uint8_t, 3> transfer_color{255, 0, 255};
};

struct ReliefConfig {
    std::string fill_mode{"surface_to_base"};
    double base_z_mm{0.0};
};

/** @brief Stage 16 geometry occupancy sampling selection. */
struct GeometrySamplingConfig
{
    std::string strategy{"legacy_center_sample"};
    /**
     * @brief 退化面判定阈值，单位为面积平方（mm^4）；面积^2 <= 该值的三角形被丢弃。
     *
     * 默认 1e-12 等价于面积门 1e-6 mm^2。CAD/NURBS 导出（如犀牛）的多材质资产
     * 常含 nm^2 量级的合法薄面，默认门会把它们误判为退化面并丢弃，
     * 从而在【本来闭合】的网格上制造出边界边，使逐材质拓扑被判为开放表面。
     *
     * 取 0 或负值表示沿用适配器默认值，既有工艺行为因此保持不变。
     * 推荐值见 docs/slice/DOC/DOC_POLICY_INDEX_冲突裁决与工艺逻辑策略总表.md。
     */
    double degenerate_area_epsilon_mm2{0.0};
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
    // 宿主有效 Profile 顶层的溯源标识（HostRequestBuilder 发射 profileVersion 与
    // profileHash）。此前不被保留，导致写包时无从产出 manifest.profileEcho，
    // 而结果页与包摘要都以该字段承载「这一包由哪份 Profile 切出」。
    // CLI 直接喂配置文件时两者为空——那种场景本就没有 Profile 溯源，
    // 故按空处理并【不发射】该字段，既不编造也不影响既有 golden 包的字节。
    std::string profile_version;
    std::string profile_hash;
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
    MaterialVolumePolicyConfig material_volume_policy;
    TransferChannelPolicyConfig transfer_channel_policy;
    SupportConfig support;
    OuterVarnishShellConfig outer_varnish;
    SurfaceVarnishConfig surface_varnish;
    MaterialClosureConfig material_closure;
    PreviewConfig preview;
    ReliefConfig relief;
    GeometrySamplingConfig geometry_sampling;
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
