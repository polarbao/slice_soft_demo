#pragma once

#include <array>

namespace slicer_core
{

/**
 * @brief `relief_heightfield` 采样得到的逐列信息。
 *
 * 每列一份，故整表是 O(列数)。10um 大幅面下列数达 736 万，该表本身就是
 * 数百 MB 级的单项 —— 它是峰值内存模型里必须计入的一项，
 * 也是本结构从 `slicer.cpp` 下沉到头文件的原因：
 * 峰值预估器要对它 `sizeof`，而常量若在两处各写一份必然漂移。
 *
 * 与 `BoundedReliefColumnSpan` 的关系：后者是本结构的三个字段
 * （`has_model` / `lower_layer` / `upper_layer`）的投影，MF-03X2a 用它
 * 按层重建 mask 而无需保留整栈。注意 `has_model` 为真时
 * `lower_layer` / `upper_layer` 仍可能是 -1
 * （采样在 `startLayer > endLayer` 时 continue，而 has_model 已先置真），
 * 故读取上界必须走 `BoundedSpanLastModelLayer` 的归一化守卫。
 */
struct ReliefColumnInfo {
    bool has_model{false};
    int lower_layer{-1};
    int upper_layer{-1};
    double z_min_mm{0.0};
    double z_max_mm{0.0};
    int hit_count{0};
    bool multi_hit{false};
    int top_triangle_index{-1};
    std::array<double, 3> top_barycentric{0.0, 0.0, 0.0};
};

}  // namespace slicer_core
