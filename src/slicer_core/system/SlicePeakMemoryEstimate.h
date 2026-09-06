#pragma once

#include "slicer_core/config.h"
#include "slicer_core/geometry/SliceGridSpec.h"

#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/** @brief 峰值模型里的一项。用于把估算值拆开给人看，不参与判定。 */
struct SlicePeakMemoryTerm
{
    std::string name;
    std::uint64_t bytes{0};
    /// 该项是否随层数增长（true 表示 O(列数 x 层数)）。
    bool scalesWithLayers{false};
};

/** @brief 一条路径的峰值估算。 */
struct SlicePeakMemoryPathEstimate
{
    std::uint64_t totalBytes{0};
    std::vector<SlicePeakMemoryTerm> terms;
};

/**
 * @brief 两条路径的峰值估算结果。
 *
 * `bounded` 仅在 `boundedEligible` 为真时有意义 —— 否则该配置根本走不了
 * 有界路径，报一个它永远不会达到的数字只会误导。
 */
struct SlicePeakMemoryEstimate
{
    SlicePeakMemoryPathEstimate retained;
    SlicePeakMemoryPathEstimate bounded;
    bool boundedEligible{false};
    /// 未准入时的原因，与 `EvaluateBoundedReliefSupportPath` 的 reason 一致。
    std::string boundedRejectReason;
};

/**
 * @brief 按【已建模的大宗分配项】估算切片峰值内存。
 *
 * ## 这个估算的契约是单向的：允许高估，禁止低估。
 *
 * 因为它的用途是「预算不够就在作业开始前失败」。高估让一个本可跑通的作业
 * 被拒，用户会立刻发现并调高预算；低估则是**承诺了预算然后 OOM**，
 * 那比不做路由更糟 —— 用户按承诺排了生产，结果跑到一半崩。
 * 故本函数在每一处取舍上都选偏大的那一侧，并额外加一笔固定余量（见下）。
 *
 * ## 它建模了什么
 *
 * 只建模 O(列数) 与 O(列数 x 层数) 的大宗分配。这些项的字节数全部由
 * `sizeof` 就地取得，不复制常量 —— 复制的常量必然与结构定义漂移，
 * 而漂移的方向无法预测，可能正好导致低估。
 *
 * ## 它【没有】建模什么（这份清单必须随代码更新）
 *
 * - 进程与运行时的固定开销（CRT、加载的模块）；
 * - `texture_runtime` 的纹理像素（随贴图分辨率与材质数变化，与栅格无关）；
 * - `materialVolumePlan` 与 OpenVDB 后端的体数据；
 * - 模型网格本身（顶点/三角）；
 * - 分配器未归还给 OS 的部分 —— 注意实测 peak working set 含这一块，
 *   故实测值天然高于「分配量之和」，这也是必须留余量的原因之一。
 *
 * 上述未建模项由 `kUnmodelledReserveBytes` 一笔兜住。该常量是**经验上界**，
 * 由三个实测点标定（见 .cpp 注释），不是推导值。
 *
 * @param config 切片配置。条件分配（外光油、表面光油、形状优化等）据此判定。
 * @param grid 栅格。必须在采样【之前】即可得 —— 这正是本估算能用于
 *        「作业开始前选路」的前提；`make_grid_spec` 只依赖配置与包围盒。
 */
[[nodiscard]] SlicePeakMemoryEstimate EstimateSlicePeakMemory(
    const SliceConfig& config,
    const GridSpec& grid);

/// 未建模项的固定余量。见上方说明与 .cpp 里的标定记录。
inline constexpr std::uint64_t kUnmodelledReserveBytes{256ULL * 1024ULL * 1024ULL};

}  // namespace slicer_core
