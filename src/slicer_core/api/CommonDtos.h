#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace slicer_core::api {

using ModelId = std::uint64_t;
using SceneId = std::uint64_t;

/** @brief 以毫米表示的轴对齐边界。 */
struct Bounds3d
{
    std::array<double, 3> min_mm{};
    std::array<double, 3> max_mm{};
};

/** @brief 规范的行主序 4x4 变换矩阵。 */
struct Matrix4d
{
    std::array<double, 16> values{
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0};
};

/** @brief 跨无 Qt Facade 边界保持不透明的已验证 JSON 对象。 */
struct StructuredJsonObject
{
    std::string utf8_json{"{}"};
};

/** @brief 对缓存模型实例的稳定引用。 */
struct InstanceReference
{
    std::string instance_id;
    ModelId model_id{0};
    Matrix4d world_matrix;
};

}  // namespace slicer_core::api
