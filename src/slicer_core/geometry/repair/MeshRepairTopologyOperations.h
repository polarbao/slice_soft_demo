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
 * @brief Inputs for the guarded R2-02 vertex-weld and winding operation set.
 */
struct MeshRepairTopologyOperationRequest
{
    const AdaptedTriangleMesh* mesh{nullptr};
    MeshRepairOptions options;
    MeshRobustnessOptions robustnessOptions;
    std::uint64_t firstOperationId{1U};
};

/**
 * @brief Isolated topology candidate, provenance, and an optional stable blocker.
 */
struct MeshRepairTopologyOperationResult
{
    AdaptedTriangleMesh candidate;
    std::vector<MeshRepairOperation> operations;
    std::vector<MeshRepairVertexMapping> vertexMappings;
    bool blocked{false};
    std::string attributeStatus{"passed"};
    std::string blockerCode;
};

/**
 * @brief Execute only guarded vertex weld and unique winding propagation.
 * @param request Isolated mesh, explicit options, robustness limits, and operation id base.
 * @return Candidate and deterministic evidence; blocked results retain the input mesh.
 * @throws MeshRepairError when the request is structurally invalid.
 */
MeshRepairTopologyOperationResult ExecuteMeshRepairTopologyOperations(
    const MeshRepairTopologyOperationRequest& request);

}  // namespace slicer_core
