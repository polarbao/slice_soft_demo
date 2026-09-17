#pragma once

// 浮雕高度场采样：逐列求交、超采样与列区间计算（F-09 第 7 步从 slicer.cpp 搬出）。
//
// 本簇不在原立项方案 §4 的六步表里——那张表漏了它与横截面统计两簇。
// 实测它函数层面自足（不调用任何簇外函数），5 个入口全部只被 run_slicer 调用。
//
// ReliefSamplingResult 放在 slicer_core 而非 relief 子命名空间：
// 它在 slicer.cpp 里还有 1 处使用，留在外层可使那处一字不改。

#include "slicer_core/config.h"
#include "slicer_core/geometry/ReliefColumnInfo.h"
#include "slicer_core/geometry/SliceGridSpec.h"
#include "slicer_core/materials/SliceMaterialTexture.h"
#include "slicer_core/model.h"
#include "slicer_core/output/reports/SliceReportJson.h"
#include "slicer_core/support/SliceSupportGeneration.h"

#include <cstdint>
#include <vector>

namespace slicer_core {

struct ReliefSamplingResult {
    std::vector<std::vector<std::uint8_t>> model_masks;
    std::vector<ReliefColumnInfo> columns;
    ReliefPerMaterialTopSurface per_material_top;
    ReliefReportData report;
};

namespace relief {

ReliefSamplingResult sample_relief_heightfield_masks(
    const SliceConfig& config,
    const ModelReport& model_report,
    const GridSpec& grid,
    std::vector<LayerDiagnostics>& diagnostics,
    // M2：MATVOL 的材质名表。非空时按 (材质, 列) 记录逐材质顶面，使被遮住的
    // 下层材质也能采到自己的贴图；为空（MATVOL 未启用）时完全不建表，零开销。
    const std::span<const std::string>* planMaterialNames = nullptr,
    // MF-03X2a：false 时【不分配也不填充】整栈 model mask，只产出 columns。
    // 10um 大幅面场景该整栈约 10.4 GB，是三个无条件分配之一。
    // 列区间仍照常写入 columns，主循环据此按层重建（见 BoundedReliefSupportPlan）。
    const bool materializeModelMaskStack = true);

std::vector<int> compute_first_model_layers(const std::vector<std::vector<std::uint8_t>>& model_masks, const GridSpec& grid);

std::vector<int> compute_relief_lower_layers(const std::vector<ReliefColumnInfo>& columns);

std::vector<ColumnLayerRange> compute_relief_column_ranges(const std::vector<ReliefColumnInfo>& columns);

std::vector<ColumnLayerRange> compute_mask_column_ranges(
    const std::vector<std::vector<std::uint8_t>>& model_masks,
    const GridSpec& grid);

}  // namespace relief

}  // namespace slicer_core
