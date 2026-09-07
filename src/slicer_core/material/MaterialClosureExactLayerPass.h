#pragma once

#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"
#include "slicer_core/material/MaterialClosureRepair.h"

#include <cstdint>
#include <span>

namespace slicer_core
{

/**
 * @brief Caller-owned storage reused across layers by the exact closure pass.
 *
 * MF-09。此前主循环直接调 owning 便利版本，它每层【新建】一个 workspace 并在
 * 返回前把 7 个 mask 拷进 owning 结构，两笔都按幅面计：Prepare 的 7 次 resize
 * 约 110 MB/层，返回前的 7 次拷贝约 51.6 MB/层。10um 大幅面（736 万列）下
 * 合计二十余 GB 的分配、归零与拷贝。
 *
 * 把这两个 workspace 持有在层循环之外即可全部消除：`Prepare` 内部全是
 * `resize`，对已有容量是 no-op（**不写**）。这与 §14.1.2 那次失败的尝试不同 ——
 * 那里复用的是 `assign(n, 0)` 填出来的输入缓冲，复用后字节照样要写。
 *
 * 跨层复用不会串味：View 版本的分析与修复计划在入口处各自重置全部借出 mask
 * （6 次 fill + 洪泛函数自身的 fill；修复侧 2 次 copy + 7 次 fill）。
 */
struct MaterialClosureExactLayerWorkspace
{
    MaterialClosureSemanticWorkspace semantic;
    MaterialClosureRepairWorkspace repair;
};

/** @brief Layer-invariant knobs for one exact closure pass. */
struct MaterialClosureExactLayerRequest
{
    int connectivity{8};
    int maxGapPx{1};
    bool repair{false};
    MaterialClosureRepairValues repairValues;
};

/** @brief Per-layer closure result plus the pixels repair added to the layer. */
struct MaterialClosureExactLayerOutcome
{
    MaterialClosureSemanticLayerResult result;
    int repairedModelFillPixels{0};
    int repairedSupportPixels{0};
    int repairedInternalVoidPixels{0};
};

/**
 * @brief Analyze one layer's exact material closure, optionally repairing it.
 * @param input Semantic masks for the layer; repair mutates them in place.
 * @param request Layer-invariant connectivity, radius, and repair values.
 * @param layer Interleaved RGBWSV channels repaired in place.
 * @param workspace Storage reused across layers; see the struct comment.
 * @throws std::invalid_argument When dimensions, connectivity, or radius are invalid.
 */
MaterialClosureExactLayerOutcome RunMaterialClosureExactLayerPass(
    MaterialClosureSemanticLayerInput& input,
    const MaterialClosureExactLayerRequest& request,
    std::span<std::uint8_t> layer,
    MaterialClosureExactLayerWorkspace& workspace);

}  // namespace slicer_core
