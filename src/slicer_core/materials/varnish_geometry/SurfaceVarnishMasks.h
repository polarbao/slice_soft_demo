#pragma once

#include "slicer_core/config.h"
#include "slicer_core/geometry/SliceGridSpec.h"

#include <cstdint>
#include <vector>

namespace slicer_core
{

/**
 * @brief 从幅面边界向内洪泛，标出与模型【不相连】的空白像素。
 *
 * 用于区分「模型外侧的空气」与「模型内部的封闭空腔」：前者属外表面，
 * 后者属内表面。洪泛走四邻域，遇 modelMask 非零即止。
 */
std::vector<std::uint8_t> BuildExternalEmptyMask(
    const GridSpec& grid,
    const std::vector<std::uint8_t>& modelMask);

/**
 * @brief 判断本次配置是否需要表面光油 mask。
 *
 * MaterializeSurfaceVarnishLayer 直接调用本函数做提前返回，故「是否需要」与
 * 「是否物化」由同一处判定，不存在两处条件漂移的可能。
 */
[[nodiscard]] bool SurfaceVarnishMasksRequired(const SliceConfig& config);

/**
 * @brief MF-03X：把【单层】表面光油 mask 物化进调用方缓冲。
 *
 * 原全层构建版本（已随本次改动删除）一次分配全部层，在 10um 大幅面场景下两个
 * 容器各占约 10.4 GB（列数 x 层数）。而该算法【逐层独立】——每层只读本层 model
 * mask，层间无依赖，故改为按需单层计算，峰值由 O(列数 x 层数) 降为 O(列数)。
 *
 * 缓冲由调用方持有并跨层复用，本函数只覆写不分配；尺寸不符即抛异常而非静默截断。
 */
void MaterializeSurfaceVarnishLayer(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::uint8_t>& modelMask,
    const int layerIndex,
    std::vector<std::uint8_t>& outerSurfaceMask,
    std::vector<std::uint8_t>& innerSurfaceMask);

}  // namespace slicer_core
