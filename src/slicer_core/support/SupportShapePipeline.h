#pragma once

#include "slicer_core/support/SupportShapeOptimizer.h"
#include "slicer_core/support/SupportType.h"

namespace slicer_core
{

/**
 * @brief 形状优化之后，同步【单层】的支撑类型图。
 *
 * 语义（三条，顺序即优先级）：
 * - 优化后为空的像素，类型归 `None`；
 * - 优化前为空、优化后非空、且类型仍是 `None` 的像素，记为 `BottomProjection`
 *   —— 那是形状运算新添的支撑，它没有自己的来源类型；
 * - 其余不动（已有类型的像素保留其来源）。
 *
 * 之所以提为共享定义：这段逻辑此前在仓库里有三份**逐字副本**（主循环的整栈版、
 * `BoundedSupportShapeScan` 的逐层版、以及测试里的参考实现）。前两份都是生产
 * 代码，各自漂移不会被任何断言发现。做法与 `set_support_pixel` 一致
 * （见 `InternalVoidSupport.h`）。
 *
 * 测试里那一份**有意保留**：对拍的 oracle 必须独立于被测实现，
 * 复用同一份定义会让比对退化成自反。
 */
void SynchronizeSupportShapeTypesForLayer(
    const std::vector<std::uint8_t>& originalSupportMask,
    const std::vector<std::uint8_t>& optimizedSupportMask,
    std::vector<SupportType>& supportTypeMap);

/**
 * @brief 形状优化之后，同步整栈的支撑类型图。逐层调用上面那一份。
 */
void SynchronizeSupportShapeTypeMaps(
    const std::vector<std::vector<std::uint8_t>>& originalSupportMasks,
    const std::vector<std::vector<std::uint8_t>>& optimizedSupportMasks,
    std::vector<std::vector<SupportType>>& supportTypeMaps);

/**
 * @brief Apply support shape policy through the support pipeline facade.
 * @param policy Shape policy.
 * @param modelMasks Per-layer model masks.
 * @param supportMasks Per-layer support masks to modify in place.
 * @param width Mask width in pixels.
 * @param height Mask height in pixels.
 * @param connectivity Connectivity for component analysis.
 * @return Support shape optimization report data.
 */
SupportShapeOptimizationResult ApplySupportShapePolicy(
    const SupportShapePolicy& policy,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    std::vector<std::vector<std::uint8_t>>& supportMasks,
    int width,
    int height,
    int connectivity);

/**
 * @brief Optimize one support layer through the support pipeline facade.
 * @param policy Shape policy.
 * @param modelMask Single-layer model mask.
 * @param supportMask Single-layer support mask to modify in place.
 * @param width Mask width in pixels.
 * @param height Mask height in pixels.
 * @param connectivity Connectivity for component analysis.
 * @return Support shape optimization report data for one layer.
 */
SupportShapeOptimizationResult OptimizeSupportShapeForLayer(
    const SupportShapePolicy& policy,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    int width,
    int height,
    int connectivity);

}  // namespace slicer_core
