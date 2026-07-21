#pragma once

#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"

#include <cstdint>

namespace slicer_core
{

/**
 * @brief Explicit limits for deterministic complete self-intersection analysis.
 */
struct MeshCompleteSelfIntersectionOptions
{
    double epsilonMm{1.0e-6};
    std::uint64_t maxCandidatePairs{5000000U};
};

/**
 * @brief Enumerate and classify all non-adjacent AABB-overlapping triangle pairs.
 * @param mesh Indexed triangle mesh in final slicing coordinates.
 * @param options Geometric tolerance and explicit candidate-pair budget.
 * @return Complete evidence or a stable budget/resource blocker.
 * @throws MeshRepairError when mesh indices or numeric inputs are invalid.
 */
MeshCompleteSelfIntersectionAnalysis AnalyzeCompleteMeshSelfIntersections(
    const TriangleMeshData& mesh,
    const MeshCompleteSelfIntersectionOptions& options);

}  // namespace slicer_core
