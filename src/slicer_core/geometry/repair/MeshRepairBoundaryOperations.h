#pragma once

#include "slicer_core/geometry/MeshRobustnessDiagnostics.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Inputs for the guarded R2-03 simple boundary-loop operation set.
 */
struct MeshRepairBoundaryOperationRequest
{
    const AdaptedTriangleMesh* mesh{nullptr};
    MeshRepairOptions options;
    MeshRobustnessOptions robustnessOptions;
    std::uint64_t firstOperationId{1U};
};

/**
 * @brief Isolated boundary-fill candidate, provenance, and an optional blocker.
 */
struct MeshRepairBoundaryOperationResult
{
    AdaptedTriangleMesh candidate;
    std::vector<MeshRepairOperation> operations;
    std::vector<MeshRepairGeneratedTriangleMapping> generatedTriangleMappings;
    bool blocked{false};
    std::string attributeStatus{"passed"};
    std::string blockerCode;
};

/**
 * @brief Fill only simple planar convex boundary loops under explicit budgets.
 * @param request Isolated mesh, explicit budgets, attribute policy, and operation id base.
 * @return Candidate and deterministic evidence; blocked results retain the input mesh.
 * @throws MeshRepairError when the request or explicit policy is structurally invalid.
 */
MeshRepairBoundaryOperationResult ExecuteMeshRepairBoundaryOperations(
    const MeshRepairBoundaryOperationRequest& request);

}  // namespace slicer_core
