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
/**
 * @brief 跨层复用的洪泛暂存。
 *
 * 原实现每层新建两个 `pixelCount` 字节的缓冲，10um 大幅面场景下等于每层触碰
 * 约 22 MB 新页 —— 实测这才是该函数的主要开销（剪枝后仍占 143 ms/层）。
 * 复用后配合 activeColumns，每层只需重置表内的列。
 *
 * 由调用方持有；首次使用时按幅面初始化，之后只增量重置。
 */
struct InternalVoidScratch
{
    std::vector<std::uint8_t> externalEmpty;
    std::vector<std::uint8_t> visited;
    std::vector<int> stack;
    bool initialized{false};
};

/**
 * @param activeColumns 非空时只在这些列内洪泛与找分量，其余列直接视为外部空白。
 *        调用方须保证表外的列在【任何层】都无模型且能连到幅面边界
 *        （`BuildBoundedActiveColumns` 正是按此判据求出）。这是精确等价的剪枝：
 *        表外的列在本层同样为空，原实现的洪泛必然也会把它们标成外部。
 *        实测 a-2/0.2.obj 只有 2.53% 的列在表内，其余 97.47% 无需逐层重算。
 */
void AddInternalVoidSupportForLayer(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<SupportType>& supportTypeMap,
    const std::vector<std::uint32_t>* activeColumns = nullptr,
    InternalVoidScratch* scratch = nullptr);

}  // namespace slicer_core
