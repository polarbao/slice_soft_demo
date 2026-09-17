#pragma once

// 支撑掩膜生成与统计（F-09 第 4 步从 slicer.cpp 搬出）。
//
// 三个类型放在 slicer_core 而非 support 子命名空间：它们出现在公开函数的签名里，
// 且在 slicer.cpp 中仍有使用（SupportGenerationResult 2 处、SupportPlacementPolicy 2 处、
// ColumnLayerRange 14 处）。留在外层可使 slicer.cpp 侧的类型使用一字不改。
//
// ColumnLayerRange 只是 BoundedMaterialColumnRangeFact 的别名，且第 5 步（贴图/材质）
// 也要用它。暂放此处是因为本步先落地；若第 5 步证实它更属于材质域，届时再迁。

#include "slicer_core/config.h"
#include "slicer_core/geometry/SliceGridSpec.h"
#include "slicer_core/json_value.h"
#include "slicer_core/material/RetainedMaterialLayerComposer.h"
#include "slicer_core/model.h"
#include "slicer_core/output/reports/SliceReportJson.h"
#include "slicer_core/support/SupportBaseProjection.h"
#include "slicer_core/support/SupportType.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core {

struct SupportPlacementPolicy {
    std::string requested_placement{"lower"};
    std::string effective_placement{"lower"};
    bool placement_explicit{false};
    bool lower_enabled{true};
    bool upper_enabled{false};
    bool unsupported_only_enabled{false};
    bool full_vertical_projection_enabled{false};
    bool advanced_debug{false};
};

struct SupportGenerationResult {
    std::vector<std::vector<std::uint8_t>> support_masks;
    std::vector<std::vector<SupportType>> support_type_maps;
    int support_pixels{0};
    int island_count{0};
    int island_pixels{0};
    int unsupported_pixels{0};
    int filtered_island_count{0};
    int filtered_island_pixels{0};
    int bottom_projection_support_pixels{0};
    int unsupported_island_support_pixels{0};
    int full_vertical_projection_support_pixels{0};
    int internal_void_support_pixels{0};
    int upper_projection_support_pixels{0};
    int projection_base_support_pixels{0};
    int layers_with_islands{0};
    int layers_with_support{0};
};

using ColumnLayerRange = BoundedMaterialColumnRangeFact;

namespace support {

void LiftModelForSupportBase(ModelReport& modelReport, const double modelLiftMm);

int ApplyOuterVarnishSupportPriority(
    const std::vector<std::vector<std::uint8_t>>& outerVarnishMasks,
    SupportGenerationResult& supportGeneration,
    std::vector<std::vector<std::size_t>>* clearedSupportIndices);

SupportPlacementPolicy ResolveSupportPlacementPolicy(const SliceConfig& config);

SupportGenerationResult generate_support_masks(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::vector<std::uint8_t>>& model_masks,
    const std::vector<std::vector<std::uint8_t>>& upper_boundary_masks,
    const std::vector<int>& support_source_layers,
    const std::vector<ColumnLayerRange>& column_ranges,
    const std::vector<ColumnLayerRange>& upper_boundary_column_ranges,
    std::vector<LayerDiagnostics>& diagnostics);

void AccumulateSupportLayerStats(
    SupportGenerationResult& result,
    LayerDiagnostics& layerDiagnostics,
    const std::vector<std::uint8_t>& support_mask,
    const std::vector<SupportType>& support_type_map,
    const GridSpec& grid,
    const SliceConfig& config);

void ResetSupportGenerationStats(SupportGenerationResult& result);

void CalculateSupportGenerationStats(
    SupportGenerationResult& result,
    std::vector<LayerDiagnostics>& diagnostics,
    const GridSpec& grid,
    const SliceConfig& config);

Json BuildSupportBaseProjectionReport(
    const SupportBaseProjectionResult& result,
    const SupportGenerationResult& supportGeneration);

}  // namespace support

}  // namespace slicer_core
