#pragma once

#include "slicer_core/geometry/MeshRobustnessDiagnostics.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"

#include <optional>
#include <string>

namespace slicer_core
{

/**
 * @brief Inputs for the explicit R2 conservative cleanup operation set.
 */
struct MeshRepairCleanupRequest
{
    const AdaptedTriangleMesh* mesh{nullptr};
    MeshRepairInputSummary input;
    MeshRepairOptions options;
    MeshRobustnessOptions robustnessOptions;
    std::optional<std::string> sourceHash;
};

/**
 * @brief Isolated candidate mesh and non-production cleanup evidence.
 */
struct MeshRepairCleanupResult
{
    AdaptedTriangleMesh candidate;
    MeshRepairResult evidence;
};

/**
 * @brief Remove only explicit degenerates and same-attribute same-winding exact duplicates.
 * @param request Adapted mesh, repair options, identity and robustness limits.
 * @return Isolated candidate and deterministic cleanup evidence.
 * @throws MeshRepairError when the request or source mapping is invalid.
 */
MeshRepairCleanupResult ExecuteMeshRepairCleanup(
    const MeshRepairCleanupRequest& request);

}  // namespace slicer_core
