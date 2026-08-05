#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace slicer_core::api {

using ModelId = std::uint64_t;
using SceneId = std::uint64_t;

/** @brief Axis-aligned bounds in millimeters. */
struct Bounds3d
{
    std::array<double, 3> min_mm{};
    std::array<double, 3> max_mm{};
};

/** @brief Canonical row-major 4x4 transform matrix. */
struct Matrix4d
{
    std::array<double, 16> values{
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0};
};

/** @brief Validated JSON object kept opaque across the Qt-free facade boundary. */
struct StructuredJsonObject
{
    std::string utf8_json{"{}"};
};

/** @brief Stable reference to a cached model instance. */
struct InstanceReference
{
    std::string instance_id;
    ModelId model_id{0};
    Matrix4d world_matrix;
};

}  // namespace slicer_core::api
