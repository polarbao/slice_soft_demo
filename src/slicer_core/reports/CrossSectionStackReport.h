#pragma once

// 横截面材料栈报告与逐层通道统计（F-09 第 8 步从 slicer.cpp 搬出）。
//
// 本簇与第 7 步的浮雕采样同为原立项方案 §4 六步表【遗漏】的两簇。
// 实测它函数层面自足，且【不依赖 slicer.cpp 的任何顶层类型】——八步里唯一一个
// 不需要把类型一并外移的簇，所以本头文件只有函数声明。
//
// 命名空间沿用 slicer_core::reports，与第 2 步的 output/reports/SliceReportJson 同域：
// 两者都是报告序列化，函数名不重叠。

#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/output/reports/SliceReportJson.h"
#include "slicer_core/support/SliceSupportGeneration.h"

#include <vector>

namespace slicer_core::reports {

void update_layer_channel_stats(
    const std::vector<std::uint8_t>& layer,
    LayerDiagnostics& diagnostics,
    const std::vector<std::uint32_t>* active_columns = nullptr,
    const std::uint8_t background_value = 255U);

void merge_channel_stats(std::array<ChannelStats, rgbwsv_channel_count>& totals, const LayerDiagnostics& diagnostics);

void merge_semantic_stats(LayerSemanticStats& totals, const LayerSemanticStats& layer);

Json BuildCrossSectionMaterialStackReport(
    const SliceConfig& config,
    const LayerSemanticStats& semanticStats,
    const SupportGenerationResult& supportGeneration,
    const SupportPlacementPolicy& supportPlacementPolicy);

Json BuildSingleMaterialConsistencyHint(const SliceConfig& config);

}  // namespace slicer_core::reports
