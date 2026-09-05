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
 * @brief 求出 compose 稀疏遍历的活动列表。
 *
 * compose_layer 的分支链是 `if (model) … else if (outer_varnish) … else if (support)`，
 * **没有末尾 else**，故三者皆零的列不写任何字节、保持预填的 background。
 * 因此只要活动列表覆盖三者的并集，稀疏遍历就是精确等价。
 *
 * 直接取「有模型的列」并**不够**：`AddInternalVoidSupportForLayer` 会给面内被模型
 * 围住的空腔写支撑，而环形件孔心那类列在【所有层】都没有模型。
 *
 * 判据：无模型的列若能经由其他无模型列连到幅面边界，则它在任何一层都是外部空白
 * ——那些列在该层同样为空，洪泛必然经它们抵达。故这类列可以安全排除，其余全部保留。
 * 用 4 邻接求连通是保守方向：若空腔洪泛用 8 邻接，只会让更多列被判为外部，
 * 而本函数少判外部只会让活动表偏大，不会漏列。
 *
 * @param spans 逐列闭区间。
 * @param widthPx 幅面宽。
 * @param heightPx 幅面高。
 * @return 升序排列的活动列下标。
 */
[[nodiscard]] std::vector<std::uint32_t> BuildBoundedActiveColumns(
    const std::vector<BoundedReliefColumnSpan>& spans,
    int widthPx,
    int heightPx);

/**
 * @brief 按列区间物化【单层】model mask。缓冲由调用方持有并跨层复用。
 *
 * 尺寸不符即抛异常而非静默截断。
 */
/**
 * @param activeColumns 非空时只重置并只写这些列。表外的列在任何层都无模型，
 *        故调用方跨层复用的缓冲里它们恒为 0，无需每层重填整幅面
 *        —— 这是按幅面计的固定开销，与模型占多少列无关。
 */
void MaterializeReliefModelLayer(
    const std::vector<BoundedReliefColumnSpan>& spans,
    int layerIndex,
    std::vector<std::uint8_t>& outModelMask,
    const std::vector<std::uint32_t>* activeColumns = nullptr);

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
    std::vector<SupportType>& outSupportTypeMap,
    const std::vector<std::uint32_t>* activeColumns = nullptr);

}  // namespace slicer_core
