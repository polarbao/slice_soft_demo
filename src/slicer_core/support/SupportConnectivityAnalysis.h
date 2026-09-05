#pragma once

#include "slicer_core/geometry/SliceGridSpec.h"

#include <cstdint>
#include <vector>

namespace slicer_core
{

struct SupportComponentSummary {
    int area_px{0};
    int min_x{0};
    int min_y{0};
    int max_x{0};
    int max_y{0};
};

struct SupportConnectivityDiagnostics {
    bool enabled{false};
    int component_count{0};
    int largest_component_pixels{0};
    int small_component_count{0};
    int tiny_component_count{0};
    std::vector<SupportComponentSummary> components;
};

/**
 * @brief 统计单层支撑的连通分量。
 *
 * 原为 slicer.cpp 匿名命名空间内的函数。下沉的直接原因是它每层都要新建一个
 * `pixel_count` 字节的 visited 缓冲并做一次全幅面扫描 —— 在 10um 大幅面场景下
 * 属于按幅面计的固定开销，是继内部空腔之后的下一个优化目标；
 * 独立编译单元便于单独改造与测试。语义逐字未动。
 */
SupportConnectivityDiagnostics analyze_support_connectivity(
    const std::vector<std::uint8_t>& support_mask,
    const GridSpec& grid,
    int connectivity);

}  // namespace slicer_core
