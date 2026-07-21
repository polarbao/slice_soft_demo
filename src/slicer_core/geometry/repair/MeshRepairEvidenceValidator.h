#pragma once

#include "slicer_core/geometry/MeshRobustnessDiagnostics.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"

namespace slicer_core
{

/**
 * @brief Immutable inputs for the R2-04 post-repair evidence validator.
 */
struct MeshRepairEvidenceValidationRequest
{
    const AdaptedTriangleMesh* originalMesh{nullptr};
    const AdaptedTriangleMesh* candidateMesh{nullptr};
    const MeshRepairResult* evidence{nullptr};
    MeshRobustnessOptions robustnessOptions;
};

/**
 * @brief Independent mapping, attribute, strict, and hash validation result.
 */
struct MeshRepairEvidenceValidationResult
{
    MeshRepairEvidenceValidation validation;
    MeshRepairAttributePreservation attributePreservation;
    MeshRepairDiagnosticsSummary postRepair;
    double attributeValidationMs{0.0};
    double postDiagnosticsMs{0.0};
};

/**
 * @brief Validate a repair candidate without mutating it or performing repair.
 * @param request Original/candidate meshes, repair evidence, and strict diagnostic options.
 * @return Ordered validation gates and independently recomputed post evidence.
 * @throws MeshRepairError when required request pointers or mesh attributes are invalid.
 */
MeshRepairEvidenceValidationResult ValidateMeshRepairEvidence(
    const MeshRepairEvidenceValidationRequest& request);

}  // namespace slicer_core
