#pragma once

// 层掩膜的栅格化与构建（F-09 第 3 步从 slicer.cpp 搬出）。
//
// 包含两类紧耦合的工作：三角面切片 -> 线段 -> 栅格化成逐层掩膜，
// 以及在其上构建的外光油掩膜与上表面支撑边界掩膜。
//
// UpperSupportBoundaryInfo 放在 slicer_core 而非 masks 子命名空间：
// 它在 slicer.cpp 里还有 1 处使用，留在外层可使那处一字不改。
//
// ApplyOuterVarnishSupportPriority 【刻意不在本步】：它的参数是
// SupportGenerationResult（支撑域类型，slicer.cpp 内另有 8 处使用），
// 属第 4 步「支撑」的范围；硬拉进本单元会把一个支撑类型塞进掩膜头文件。

#include "slicer_core/config.h"
#include "slicer_core/geometry/SliceGridSpec.h"
#include "slicer_core/model.h"
#include "slicer_core/output/reports/SliceReportJson.h"

#include <cstdint>
#include <vector>

namespace slicer_core {

struct UpperSupportBoundaryInfo
{
    bool includes_outer_varnish_shell{false};
    std::string source{"model_envelope"};
};

namespace masks {

std::vector<std::vector<std::uint8_t>> BuildOuterVarnishMasks(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::vector<std::uint8_t>>& modelMasks);

UpperSupportBoundaryInfo ResolveUpperSupportBoundaryInfo(const SliceConfig& config);

std::vector<std::vector<std::uint8_t>> BuildUpperSupportBoundaryMasks(
    const GridSpec& grid,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    const std::vector<std::vector<std::uint8_t>>& outerVarnishMasks,
    const UpperSupportBoundaryInfo& boundaryInfo);

std::vector<std::vector<std::uint8_t>> sample_model_masks(
    const ModelReport& model_report,
    const GridSpec& grid,
    const double layer_thickness_mm,
    std::vector<LayerDiagnostics>& diagnostics);

bool point_in_triangle_xy(
    const Vec3& p,
    const Triangle& triangle,
    double& w0,
    double& w1,
    double& w2);

int first_layer_at_or_above_z(const double z_mm, const double layer_thickness_mm);

int last_layer_at_or_below_z(const double z_mm, const double layer_thickness_mm);

}  // namespace masks

}  // namespace slicer_core
