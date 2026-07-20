#pragma once

#include "slicer_core/geometry/MeshRobustnessDiagnostics.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"

#include <optional>
#include <string>

namespace slicer_core
{

/**
 * @brief Inputs for a read-only mesh-repair eligibility preflight.
 */
struct MeshRepairPreflightRequest
{
    const AdaptedTriangleMesh* mesh{nullptr};
    MeshRepairInputSummary input;
    MeshRepairOptions options;
    MeshRobustnessOptions robustnessOptions;
    std::optional<std::string> sourceHash;
};

/**
 * @brief Evaluate repair eligibility and deterministic evidence without changing the mesh.
 * @param request Adapted mesh, input identity, options and robustness limits.
 * @return Complete pre-repair diagnostic result with no operations or production output.
 * @throws MeshRepairError when the request is invalid.
 */
MeshRepairResult EvaluateMeshRepairPreflight(
    const MeshRepairPreflightRequest& request);

}  // namespace slicer_core
