#pragma once

#include "slicer_core/config.h"
#include "slicer_core/geometry/SliceGridSpec.h"
#include "slicer_core/support/SupportType.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace slicer_core
{

/**
 * @brief 置位一个支撑像素，并按优先级仲裁其类型。
 *
 * 原为 slicer.cpp 匿名命名空间内的自由函数。MF-03X2a 把内部空腔支撑下沉到独立
 * 编译单元后，两边都要用它，故提为 inline 共享，避免出现两份定义各自漂移。
 * 语义逐字未动：mask 恒置 1，类型仅在【优先级不低于】既有类型时覆盖。
 */
inline void set_support_pixel(
    std::vector<std::uint8_t>& support_mask,
    std::vector<SupportType>& support_type_map,
    const std::size_t index,
    const SupportType type) {
    support_mask.at(index) = 1;
    if (SupportTypePriority(type) >= SupportTypePriority(support_type_map.at(index))) {
        support_type_map.at(index) = type;
    }
}

/**
 * @brief 为【单层】补写内部空腔支撑。
 *
 * 从幅面边界洪泛标出与外界连通的空白，其余空白即封闭空腔；面积达到
 * `internal_void.min_area_px` 的连通分量整块写为 `SupportType::InternalVoid`。
 *
 * 本函数【逐层独立】——只读本层 model mask、只改本层 support，层间无依赖。
 * 因此 retained 路径（generate_support_masks 内的全层循环）与 MF-03X2a 的有界
 * 路径（主循环内逐层调用）共用同一实现，不存在两份语义。
 *
 * `internal_void.enabled` 默认为 **true**，漏调本函数会把本应 InternalVoid 的
 * 像素误标成 BottomProjection —— 这正是 X2a 首次接线时 r01 漂移的原因。
 */
void AddInternalVoidSupportForLayer(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<SupportType>& supportTypeMap);

}  // namespace slicer_core
