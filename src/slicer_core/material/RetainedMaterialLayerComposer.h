#pragma once

#include "slicer_core/config.h"
#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"
#include "slicer_core/material/MaterialClosureRepair.h"
#include "slicer_core/support/SupportType.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace slicer_core
{

inline constexpr std::size_t kRetainedMaterialChannelCount{6U};

/**
 * @brief 「无材质所有者」哨兵值，与 MaterialVolumePlan 的 kNoMaterialOwner 同值。
 *
 * 刻意【不】include MaterialVolumePlan.h：materials/pipeline 边界不得反向依赖
 * materials/volume 的私有实现（DEV 第 3 节模块禁止事项）。两处同值由本注释与
 * 下方 static_assert 之外的一致性检查保证——若 MATVOL 侧改动该值，
 * 接线处的 span 转换会立刻暴露不匹配。
 */
inline constexpr std::uint32_t kRetainedNoMaterialOwner{0xFFFFFFFFU};

struct BoundedMaterialColumnRangeFact
{
    bool hasModel{false};
    int lowerLayer{-1};
    int upperLayer{-1};
};

struct BoundedTextureColumnFact
{
    bool hasColor{false};
    std::array<std::uint8_t, 3> rgb{0U, 0U, 0U};
    bool sampledTexture{false};
    bool usedFallback{false};
    bool uvOutOfRange{false};
};

enum class BoundedMaterialRole : std::uint8_t
{
    Rgb,
    White,
    Varnish,
    Ignore,
    SupportCandidate,
    Support,
};

struct BoundedMaterialRoleColumnFact
{
    bool hasRole{false};
    BoundedMaterialRole role{BoundedMaterialRole::Rgb};
    std::array<std::uint8_t, 3> rgb{0U, 0U, 0U};
};

enum class BoundedModelFillMaterial : std::uint8_t
{
    Rgb,
    White,
    Varnish,
    None,
};

struct BoundedMaterialReplayPolicy
{
    std::uint8_t backgroundValue{255U};

    std::string materialChannel{"auto"};
    std::array<std::uint8_t, 3> materialRgb{255U, 255U, 255U};
    std::uint8_t materialWhiteValue{255U};
    std::uint8_t materialVarnishValue{255U};

    bool textureEnabled{false};
    std::string textureApplyMode{"solid_volume_from_top_surface"};
    int textureTopSurfaceLayers{1};
    std::array<std::uint8_t, 3> textureFallbackRgb{0U, 0U, 0U};
    std::string textureNonSurfaceRgbPolicy{"model_material"};
    std::string textureUnprintableWhitePolicy{"fail_closed"};
    std::uint8_t textureUnprintableWhiteInkThreshold{0U};
    std::uint8_t textureUnprintableWhiteValue{0U};

    bool materialPolicyEnabled{false};
    RgbPolicyConfig materialPolicyRgb;
    WhitePolicyConfig materialPolicyWhite;
    VarnishPolicyConfig materialPolicyVarnish;

    bool modelFillEnabled{false};
    std::string modelFillMaterial{"white"};
    std::string modelFillScope{"below_texture_surface"};
    std::uint8_t modelFillValue{0U};
    bool modelFillLegacyRgbFallback{false};

    bool processProfileEnabled{false};
    bool processProfileWhiteEnabled{false};
    std::string processProfileWhiteMode{"underbase"};
    bool processProfileVarnishEnabled{false};
    std::string processProfileVarnishMode{"top_n_layers"};

    bool materialRoleMappingEnabled{false};
    bool supportEnabled{true};
    std::uint8_t supportValue{0U};
    std::uint8_t outerVarnishValue{0U};
    std::string surfaceVarnishSource{"explicit"};
    std::uint8_t surfaceVarnishValue{0U};

    /**
     * @brief MATVOL 相关开关。本组由 2026-09-04 接线补入。
     *
     * 本 policy 原系 2026-08-21 照当时的 compose_layer 抽取，彼时 MATVOL 的
     * 三条能力（M1 owner-vs-顶面判据、M2 逐材质贴图、MO-04 不透明度判光油）
     * 尚未落地，故整组缺失。缺失的后果不是编译失败而是【静默降级】：
     * bounded 路径会退回逐列顶面取色，多图层资产的下层贴图与光油映射一起丢。
     */
    bool materialVolumeEnabled{false};
    bool opacityVarnishEnabled{false};

    MaterialClosureConfig closure;
};

struct BoundedMaterialChannelStats
{
    std::uint64_t print_pixels{0U};
    std::uint64_t full_print_pixels{0U};
    std::uint64_t partial_print_pixels{0U};
    std::uint64_t empty_pixels{0U};
    int min_value{255};
    int max_value{0};
};

struct BoundedMaterialLayerSemanticStats
{
    int texture_surface_pixels{0};
    std::uint64_t unprintable_white_carrier_pixels{0U};
    int model_fill_pixels{0};
    int support_pixels{0};
    int internal_void_support_pixels{0};
    int outer_varnish_pixels{0};
    int outer_surface_varnish_pixels{0};
    int inner_surface_varnish_pixels{0};
    /**
     * @brief MO-04：因不透明度判据改写 V 通道的像素数。
     *
     * 本 DTO 于 2026-08-21 按当时的 slicer.cpp LayerSemanticStats 复制而成，
     * 而 MO-04 的该字段是此后才加入的。合并时若不补齐，bounded 路径的
     * 光油像素统计会静默归零——统计缺失不会让切片失败，只会让报告少一项，
     * 属于最难在验收中发现的那类漂移。
     */
    std::uint64_t opacity_varnish_pixels{0U};
};

struct BoundedTextureComposeStats
{
    std::uint64_t sampledPixels{0U};
    std::uint64_t fallbackPixels{0U};
    std::uint64_t uvOutOfRangePixels{0U};
};

struct BoundedMaterialPolicyComposeStats
{
    std::uint64_t rgbPrintPixels{0U};
    std::uint64_t whitePrintPixels{0U};
    std::uint64_t varnishPrintPixels{0U};
};

struct BoundedMaterialLayerComposeResult
{
    int modelPixels{0};
    int supportPixels{0};
    BoundedMaterialLayerSemanticStats semantic;
    BoundedTextureComposeStats texture;
    BoundedMaterialPolicyComposeStats materialPolicy;
};

struct BoundedMaterialLayerChannelStats
{
    std::array<BoundedMaterialChannelStats, kRetainedMaterialChannelCount>
        channels{};
    int rgbNonZeroPixels{0};
    int whiteNonZeroPixels{0};
    int supportNonZeroPixels{0};
    int varnishNonZeroPixels{0};
};

struct BoundedMaterialLayerComposeRequest
{
    const BoundedMaterialReplayPolicy* policy{nullptr};
    int widthPx{0};
    int heightPx{0};
    int layerIndex{0};
    std::span<const std::uint8_t> modelMask;
    std::span<const std::uint8_t> outerVarnishMask;
    std::span<const std::uint8_t> outerSurfaceVarnishMask;
    std::span<const std::uint8_t> innerSurfaceVarnishMask;
    std::span<const std::uint8_t> supportMask;
    std::span<const SupportType> supportTypeMap;
    std::span<const BoundedTextureColumnFact> textureColumns;
    std::span<const BoundedMaterialRoleColumnFact> materialRoleColumns;
    std::span<const BoundedMaterialColumnRangeFact> columnRanges;

    /**
     * @brief MATVOL 逐层材质 RGB（长度为 列数*3）；空则该分支不参与。
     *
     * 与 textureColumns 的区别是它【按层重算】——同一 XY 列在不同层可属不同材质，
     * 这正是多材质纵深的核心，逐列顶面取色表达不了。
     */
    std::span<const std::uint8_t> materialVolumeRgb;
    /// @brief MO-04：本层判为光油的列掩码；空则不改写 V 通道。
    std::span<const std::uint8_t> materialVolumeVarnishMask;
    /**
     * @brief M1：逐列顶面材质在 plan 材质表中的下标；kNoMaterialOwner 表示未知。
     *
     * 与 materialVolumeOwner 成对使用：两者一致说明逐列顶面贴图对本像素是
     * 正确来源，不一致则说明本像素属于被顶面遮住的下层材质，须让位给 MATVOL。
     */
    std::span<const std::uint32_t> topMaterialIndexByColumn;
    /// @brief M1：本层逐列材质所有者。
    std::span<const std::uint32_t> materialVolumeOwner;
    /**
     * @brief M2：按 (材质, 列) 预采的各材质自身贴图色，长度为 材质数*列数。
     *
     * 索引为 materialIndex * columnCount + column。空则 MATVOL 分支沿用
     * materialVolumeRgb 的逐材质 Kd。
     */
    std::span<const BoundedTextureColumnFact> perMaterialTextureColumns;
};

[[nodiscard]] BoundedMaterialReplayPolicy MakeBoundedMaterialReplayPolicy(
    const SliceConfig& config);

[[nodiscard]] BoundedModelFillMaterial
ResolveRetainedProfileDefaultModelFillMaterial(
    const BoundedMaterialReplayPolicy& policy) noexcept;

[[nodiscard]] std::string BoundedModelFillMaterialName(
    BoundedModelFillMaterial material);

[[nodiscard]] MaterialClosureRepairValues
ResolveRetainedMaterialClosureRepairValues(
    const BoundedMaterialReplayPolicy& policy);

[[nodiscard]] std::uint8_t ResolveRetainedSurfaceVarnishValue(
    const BoundedMaterialReplayPolicy& policy) noexcept;

[[nodiscard]] bool ShouldApplyRetainedTextureToLayer(
    const BoundedMaterialReplayPolicy& policy,
    std::span<const BoundedMaterialColumnRangeFact> columnRanges,
    std::size_t pixelIndex,
    int layerIndex) noexcept;

[[nodiscard]] MaterialClosureSemanticLayerInput
InitializeRetainedMaterialClosureSemanticInput(
    int layerIndex,
    double zMm,
    int widthPx,
    int heightPx,
    std::span<const std::uint8_t> modelEnvelopeMask,
    std::span<const std::uint8_t> finalSupportMask,
    std::span<const std::uint8_t> clearedOuterOverlapMask,
    std::span<const std::uint8_t> outerVarnishShellMask);

[[nodiscard]] MaterialClosureSemanticLayerInput
InitializeRetainedMaterialClosureSemanticInputFromIndices(
    int layerIndex,
    double zMm,
    int widthPx,
    int heightPx,
    std::span<const std::uint8_t> modelEnvelopeMask,
    std::span<const std::uint8_t> finalSupportMask,
    std::span<const std::size_t> clearedOuterOverlapIndices,
    std::span<const std::uint8_t> outerVarnishShellMask);

void PopulateRetainedMaterialClosureEmptyMask(
    std::span<const std::uint8_t> rgbwsv,
    MaterialClosureSemanticLayerInput& semantic);

[[nodiscard]] BoundedMaterialLayerComposeResult ComposeRetainedMaterialLayer(
    const BoundedMaterialLayerComposeRequest& request,
    std::span<std::uint8_t> outputRgbwsv,
    MaterialClosureSemanticLayerInput* semantic);

[[nodiscard]] BoundedMaterialLayerChannelStats
AnalyzeRetainedMaterialLayerChannels(std::span<const std::uint8_t> rgbwsv);

void AccumulateRetainedMaterialChannelStats(
    std::array<BoundedMaterialChannelStats, kRetainedMaterialChannelCount>& totals,
    const std::array<BoundedMaterialChannelStats,
                     kRetainedMaterialChannelCount>& layer) noexcept;

void AccumulateRetainedMaterialSemanticStats(
    BoundedMaterialLayerSemanticStats& totals,
    const BoundedMaterialLayerSemanticStats& layer) noexcept;

}  // namespace slicer_core
