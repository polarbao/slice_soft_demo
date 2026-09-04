#pragma once

#include <cstddef>

namespace slicer_core
{

/**
 * @brief 切片栅格规格：像素幅面、层数与物理尺度的换算基准。
 *
 * 原定义于 slicer.cpp 的匿名命名空间。MF-03X 要把主循环里按层数增长的 mask 构建
 * 逐个下沉到独立编译单元，而每个构建函数都以本结构为参数，故提为共享头。
 * 提头【不改变任何字段与默认值】，只改变可见性。
 */
struct GridSpec
{
    int width_px{0};
    int height_px{0};
    int layer_count{0};
    double pixel_size_x_mm{0.0};
    double pixel_size_y_mm{0.0};
    double origin_x_mm{0.0};
    double origin_y_mm{0.0};
};

/**
 * @brief 行主序像素下标。不做边界检查 —— 由调用方保证 x/y 落在幅面内。
 */
inline std::size_t mask_index(const GridSpec& grid, const int x, const int y)
{
    return static_cast<std::size_t>(y) * grid.width_px + x;
}

}  // namespace slicer_core
