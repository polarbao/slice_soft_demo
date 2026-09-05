#pragma once

#include "slicer_core/config.h"
#include "slicer_core/support/SupportType.h"

#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief 逐列的模型层闭区间。
 *
 * `relief_heightfield` 模式的 legacy 采样路径按列求出 `[startLayer, endLayer]`
 * 后原样填 mask（见 slicer.cpp `sample_relief_heightfield_masks`）：
 *
 * ```cpp
 * column.lower_layer = startLayer;  column.upper_layer = endLayer;
 * for (layerIndex in [startLayer, endLayer]) model_masks[layerIndex][pixel] = 1;
 * ```
 *
 * 故整栈 model mask 是本结构的**纯函数**，无需保留栈即可按层重建。
 * `multi_hit`（一列命中多个三角）只影响 z 的取值，填充仍是单一闭区间。
 */
struct BoundedReliefColumnSpan
{
    bool hasModel{false};
    int lowerLayer{-1};
    int upperLayer{-1};
};

/**
 * @brief MF-03X2a 有界路径的准入判定。
 *
 * 判定**只看配置**，故可在采样之前求出，从而连采样阶段的整栈分配一起省掉。
 * fail-safe：任何一项不满足即退回 retained 路径，`reason` 记下是哪一项。
 */
struct BoundedReliefSupportEligibility
{
    bool eligible{false};
    std::string reason{"not_evaluated"};
};

/**
 * @brief 判断本次配置能否走 X2a 有界路径。
 *
 * 要求（缺一不可）：
 * - `slicing_mode == "relief_heightfield"` 且 `geometry_sampling.strategy` 为
 *   `legacy_center_sample` —— 只有该组合下 mask 才是列区间的纯函数；
 *   supersample / layer_slab 候选走 `BuildLayerOccupancy`，另行处理。
 * - 支撑放置只含 bottom projection：不含 upper、unsupported_only、
 *   full_vertical_projection —— 后两者要么整栈随机访问、要么需重算最高模型层。
 * - 不启用支撑形状优化与 base projection —— 两者都整栈进、整栈改。
 * - 不启用外光油 —— 否则 `outerVarnishMasks` / `upperBoundaryMasks` 会整栈分配。
 */
[[nodiscard]] BoundedReliefSupportEligibility EvaluateBoundedReliefSupportPath(
    const SliceConfig& config);

/**
 * @brief 按列区间物化【单层】model mask。缓冲由调用方持有并跨层复用。
 *
 * 尺寸不符即抛异常而非静默截断。
 */
void MaterializeReliefModelLayer(
    const std::vector<BoundedReliefColumnSpan>& spans,
    int layerIndex,
    std::vector<std::uint8_t>& outModelMask);

/**
 * @brief 按 bottom projection 语义物化【单层】support mask 与 type map。
 *
 * 与 retained 路径 `generate_support_masks` 的 `lower_enabled` 分支逐字等价：
 *
 * ```cpp
 * for (col) for (L in [0, support_source_layers[col]))
 *     if (model_masks[L][col] == 0) set_support_pixel(..., BottomProjection);
 * ```
 *
 * 该谓词只依赖【本层】model mask 与按列标量，故无需任何跨层数据。
 * `supportEnabled` 为 false 时输出全零 —— 对应 retained 路径「先 resize 再按
 * `config.support.enabled` 提前返回」的行为（那两次 resize 在检查之前）。
 */
void MaterializeBottomProjectionSupportLayer(
    const std::vector<int>& supportSourceLayers,
    const std::vector<std::uint8_t>& modelMask,
    bool supportEnabled,
    int layerIndex,
    std::vector<std::uint8_t>& outSupportMask,
    std::vector<SupportType>& outSupportTypeMap);

}  // namespace slicer_core
