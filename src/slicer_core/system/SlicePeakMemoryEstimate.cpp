#include "slicer_core/system/SlicePeakMemoryEstimate.h"

#include "slicer_core/geometry/ReliefColumnInfo.h"
#include "slicer_core/material/RetainedMaterialLayerComposer.h"
#include "slicer_core/materials/varnish_geometry/SurfaceVarnishMasks.h"
#include "slicer_core/support/BoundedReliefSupportPlan.h"
#include "slicer_core/support/SupportType.h"

#include <cstddef>
#include <utility>

namespace slicer_core
{

namespace
{

/// RGBWSV 六通道，compose 的输出缓冲每列这么多字节。
constexpr std::uint64_t kComposeBytesPerColumn{6ULL};

void AddTerm(
    SlicePeakMemoryPathEstimate& path,
    std::string name,
    const std::uint64_t bytes,
    const bool scalesWithLayers)
{
    if (bytes == 0ULL)
    {
        return;
    }
    path.totalBytes += bytes;
    path.terms.push_back(
        SlicePeakMemoryTerm{std::move(name), bytes, scalesWithLayers});
}

}  // namespace

SlicePeakMemoryEstimate EstimateSlicePeakMemory(
    const SliceConfig& config,
    const GridSpec& grid)
{
    SlicePeakMemoryEstimate estimate;

    const auto columns = grid.width_px > 0 && grid.height_px > 0
        ? static_cast<std::uint64_t>(grid.width_px)
            * static_cast<std::uint64_t>(grid.height_px)
        : 0ULL;
    const auto layers =
        grid.layer_count > 0 ? static_cast<std::uint64_t>(grid.layer_count) : 0ULL;
    const std::uint64_t stack{columns * layers};

    const auto verdict = EvaluateBoundedReliefSupportPath(config);
    estimate.boundedEligible = verdict.eligible;
    estimate.boundedRejectReason = verdict.eligible ? std::string{} : verdict.reason;

    const bool isRelief{config.slicing_mode == "relief_heightfield"};
    // 表面光油的两个单层缓冲。直接调用生产判据本身，而不是照抄它的条件 ——
    // 抄一份就等于埋一处必然漂移的常量，而漂移方向不可控、可能正好导致低估。
    const bool surfaceVarnish{SurfaceVarnishMasksRequired(config)};

    // ---- 两条路径共有的逐列项 ----
    for (SlicePeakMemoryPathEstimate* path :
         {&estimate.retained, &estimate.bounded})
    {
        if (isRelief)
        {
            AddTerm(
                *path,
                "relief_columns",
                columns * sizeof(ReliefColumnInfo),
                false);
        }
        // column_ranges 与 upper_boundary_column_ranges 各一份。
        AddTerm(
            *path,
            "column_ranges",
            columns * sizeof(BoundedMaterialColumnRangeFact) * 2ULL,
            false);
        AddTerm(*path, "support_source_layers", columns * sizeof(int), false);
        AddTerm(*path, "compose_output_layer", columns * kComposeBytesPerColumn, false);
        if (surfaceVarnish)
        {
            AddTerm(*path, "surface_varnish_layers", columns * 2ULL, false);
        }
    }

    // ---- retained 独有：整栈 ----
    // model / support / support_type 三份恒分配（后两者的 resize 在
    // config.support.enabled 检查【之前】，故与该开关无关）。
    AddTerm(estimate.retained, "model_masks", stack, true);
    AddTerm(estimate.retained, "support_masks", stack, true);
    AddTerm(estimate.retained, "support_type_maps", stack * sizeof(SupportType), true);
    if (config.outer_varnish.enabled)
    {
        AddTerm(estimate.retained, "outer_varnish_masks", stack, true);
        // 外光油开启时上边界整栈才会被真正构建（否则退化为 model_masks 的引用）。
        AddTerm(estimate.retained, "upper_support_boundary_masks", stack, true);
    }
    if (config.support.shape_enabled)
    {
        // 形状优化前对整栈 support_masks 做一次深拷贝作为 originalSupportMasks。
        AddTerm(estimate.retained, "shape_original_support_masks", stack, true);
    }

    // ---- bounded 独有：逐列 ----
    AddTerm(
        estimate.bounded,
        "bounded_relief_spans",
        columns * sizeof(BoundedReliefColumnSpan),
        false);
    // 活动列表的元素是 uint32_t，条目数不超过列数 —— 取上界，不猜稀疏度。
    AddTerm(
        estimate.bounded,
        "bounded_active_columns",
        columns * sizeof(std::uint32_t),
        false);
    AddTerm(
        estimate.bounded,
        "bounded_single_layers",
        columns * (1ULL + 1ULL + sizeof(SupportType)),
        false);

    // ---- 未建模余量 ----
    //
    // 标定依据（三个实测点，均为 peak working set，含分配器未归还部分）：
    //
    //   r01      166,131 列 x   94 层  bounded   实测   90 MB
    //   gubao04  192,960 列 x  129 层  bounded   实测  132 MB
    //   a-2@10um 7,369,346 列 x 1429 层 bounded  实测  0.84 GiB
    //
    // 前两个是小作业，建模项只有 20~24 MB，实测却是 90/132 MB —— 差额由进程与
    // 纹理运行时占据，且随材质数变化（gubao04 六材质，差额比 r01 大约 38 MB）。
    // 取 256 MB 作上界，使三点全部被高估覆盖。
    //
    // 对大作业它可忽略（a-2@10um 的建模项已近 1 GB）；对小作业它是数倍高估，
    // 但小作业本来就不会触及预算，高估无害 —— 这正是「宁可高估」的取舍。
    AddTerm(estimate.retained, "unmodelled_reserve", kUnmodelledReserveBytes, false);
    AddTerm(estimate.bounded, "unmodelled_reserve", kUnmodelledReserveBytes, false);

    return estimate;
}

}  // namespace slicer_core
