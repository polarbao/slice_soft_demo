#pragma once

#include "slicer_core/config.h"
#include "slicer_core/geometry/SliceGridSpec.h"
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
 * @brief 有界路径要走的支撑放置档位。
 *
 * 两档互斥 —— `ResolveSupportPlacementPolicy` 下 `full_vertical_projection`
 * 与 `lower` 不可能同时为真，故任一层最多只有一段在写 support 缓冲。
 * 将来若放开 `upper`（它可与 `lower` 并存，`placement == "both"`），
 * 这里就必须改成位集合，且重置职责必须从 `Materialize*` 提到调用方
 * —— 见 `ResetBoundedSupportLayer` 的注释。
 */
enum class BoundedSupportPlacement
{
    BottomProjection,
    FullVerticalProjection,
};

/**
 * @brief MF-03X2a/X2b 有界路径的准入判定。
 *
 * 判定**只看配置**，故可在采样之前求出，从而连采样阶段的整栈分配一起省掉。
 * fail-safe：任何一项不满足即退回 retained 路径，`reason` 记下是哪一项。
 */
struct BoundedReliefSupportEligibility
{
    bool eligible{false};
    /// 仅在 `eligible` 为真时有意义。
    BoundedSupportPlacement placement{BoundedSupportPlacement::BottomProjection};
    std::string reason{"not_evaluated"};
};

/**
 * @brief 判断本次配置能否走 X2a 有界路径。
 *
 * 要求（缺一不可）：
 * - `slicing_mode == "relief_heightfield"` 且 `geometry_sampling.strategy` 为
 *   `legacy_center_sample` —— 只有该组合下 mask 才是列区间的纯函数；
 *   supersample / layer_slab 候选走 `BuildLayerOccupancy`，另行处理。
 * - 支撑放置为 `lower`（bottom projection）或 `full_vertical_projection`
 *   二者之一，不含 upper 与 unsupported_only —— 前者需要 upper boundary 的
 *   整栈，后者是逐层向下的整栈随机访问，都不是列区间的纯函数。
 *   `full_vertical_projection` 于 MF-03X2b 放开：它同样是 `[0, lastLayer)`
 *   的按列半开区间，而 `lastLayer` 正是列区间的 `upperLayer`。
 * - 不启用支撑形状优化与 base projection —— 两者都整栈进、整栈改。
 * - 不启用外光油 —— 否则 `outerVarnishMasks` / `upperBoundaryMasks` 会整栈分配。
 */
[[nodiscard]] BoundedReliefSupportEligibility EvaluateBoundedReliefSupportPath(
    const SliceConfig& config);

/**
 * @brief 该列的最后模型层；无模型或区间无效时为 -1。
 *
 * **这是一条归一化守卫，三处物化都必须经它，不可直接读 `upperLayer`。**
 * 采样路径在 `startLayer > endLayer` 时 `continue`，而 `has_model` 已在此之前
 * 置真（slicer.cpp `sample_relief_heightfield_masks`），故确实存在
 * `hasModel == true` 但 `lowerLayer/upperLayer` 仍为 -1 的列。这类列在 retained
 * 的整栈里一层都没被写过 1，`compute_last_model_layers` 对它的结果正是 -1。
 */
[[nodiscard]] int BoundedSpanLastModelLayer(const BoundedReliefColumnSpan& span);

/**
 * @brief retained 路径的同一量：从整栈 model mask 归约出每列的最后模型层。
 *
 * 与 `BoundedSpanLastModelLayer` 是同一概念的两种取法 —— 一个扫整栈、一个读
 * 列区间。**两者必须逐列同值**，这是 full vertical projection 档能有界化的
 * 全部依据，故并置于此：谁改了其中一份，另一份就在同屏可见。
 *
 * 它仍是 retained 生产路径在用的实现（`generate_support_masks` 的
 * full_vertical_projection 分支），不是另写的对照品 —— 同一份代码兼任二职，
 * 也就不存在「对照实现自己先漂移了」这种情况。
 */
[[nodiscard]] std::vector<int> ComputeRetainedLastModelLayers(
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    const GridSpec& grid);

/**
 * @brief 把一层的 support 缓冲重置为空。
 *
 * 之所以单独成函（而非留在各 `Materialize*` 里各写一份）：重置与写入是两件事，
 * 一旦将来出现【两段放置同时写同一层】（例如放开 `upper`，它与 `lower` 并存），
 * 后一段自带的重置就会把前一段的结果清掉 —— 那是一类不报错、只是支撑少一半的
 * 静默错误。届时的正确改法是让调用方显式重置一次、两段都不再自重置。
 *
 * 现阶段两档互斥，故 `Materialize*` 仍在内部调用它，对外行为与 X2a 逐字一致。
 */
void ResetBoundedSupportLayer(
    std::vector<std::uint8_t>& outSupportMask,
    std::vector<SupportType>& outSupportTypeMap,
    const std::vector<std::uint32_t>* activeColumns = nullptr);

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

/**
 * @brief 按 full vertical projection 语义物化【单层】support mask 与 type map。
 *
 * 与 retained 路径 `generate_support_masks` 的 `full_vertical_projection` 分支
 * 逐字等价：
 *
 * ```cpp
 * last_model_layers = compute_last_model_layers(model_masks, grid);
 * for (col) for (L in [0, last_model_layers[col]))
 *     if (model_masks[L][col] == 0) set_support_pixel(..., FullVerticalProjection);
 * ```
 *
 * 与 bottom projection 只差两处：上界由按列标量 `support_source_layers` 换成
 * 列区间的 `upperLayer`（经 `BoundedSpanLastModelLayer` 归一化），类型换成
 * `FullVerticalProjection`。同为半开区间，同样只依赖【本层】model mask。
 *
 * retained 那边的 `compute_last_model_layers` 是对整栈的一次归约；有界路径
 * 无需该归约，因为列区间本身就携带了它的结果 —— 这也是本档能放开的原因。
 */
void MaterializeFullVerticalProjectionSupportLayer(
    const std::vector<BoundedReliefColumnSpan>& spans,
    const std::vector<std::uint8_t>& modelMask,
    bool supportEnabled,
    int layerIndex,
    std::vector<std::uint8_t>& outSupportMask,
    std::vector<SupportType>& outSupportTypeMap,
    const std::vector<std::uint32_t>* activeColumns = nullptr);

/**
 * @brief 按准入判出的档位物化【单层】support，主循环的唯一入口。
 *
 * 档位分派留在本编译单元，而不是摊在主循环里 —— 放开新档时改这一处即可，
 * 也使「重置与写入的配合」这条约束只有一个地方需要维护。
 */
void MaterializeBoundedSupportLayer(
    BoundedSupportPlacement placement,
    const std::vector<BoundedReliefColumnSpan>& spans,
    const std::vector<int>& supportSourceLayers,
    const std::vector<std::uint8_t>& modelMask,
    bool supportEnabled,
    int layerIndex,
    std::vector<std::uint8_t>& outSupportMask,
    std::vector<SupportType>& outSupportTypeMap,
    const std::vector<std::uint32_t>* activeColumns = nullptr);

}  // namespace slicer_core
