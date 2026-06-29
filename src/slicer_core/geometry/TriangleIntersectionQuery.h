#pragma once

#include "slicer_core/geometry/TriangleMeshData.h"

#include <array>
#include <cstddef>

namespace slicer_core
{

/**
 * @brief Narrow-phase triangle intersection classification.
 */
enum class TriangleIntersectionKind
{
    None,
    AabbOnly,
    TouchingOnly,
    ConfirmedIntersection,
    CoplanarOverlap,
};

/**
 * @brief Result for one triangle-triangle intersection query.
 */
struct TriangleIntersectionResult
{
    bool aabb_candidate{false};
    TriangleIntersectionKind kind{TriangleIntersectionKind::None};
};

/**
 * @brief Test whether two indexed mesh triangles share any vertex index.
 * @param left First triangle indices.
 * @param right Second triangle indices.
 * @return True when at least one vertex index is shared.
 */
bool TrianglesShareVertexIndex(
    const std::array<int, 3>& left,
    const std::array<int, 3>& right);

/**
 * @brief Run a narrow-phase triangle-triangle intersection test.
 * @param mesh Indexed mesh containing both triangles.
 * @param leftIndex First triangle index.
 * @param rightIndex Second triangle index.
 * @param epsilonMm Geometric tolerance in millimeters.
 * @return Intersection classification.
 */
TriangleIntersectionResult TestTriangleIntersection(
    const TriangleMeshData& mesh,
    std::size_t leftIndex,
    std::size_t rightIndex,
    double epsilonMm);

}  // namespace slicer_core
