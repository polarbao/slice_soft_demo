#pragma once

#include "slicer_core/geometry/MeshRobustnessDiagnostics.h"
#include "slicer_core/geometry/MeshTopologyDiagnostics.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"

#include <cstdint>

namespace slicer_core
{

/**
 * @brief Attribute evidence required before geometry issues can become automatic candidates.
 */
struct MeshRepairEligibilityEvidence
{
    bool duplicateFaceAttributesEvaluated{false};
    std::uint64_t duplicateFaceAttributeConflicts{0U};
};

/**
 * @brief Classify pre-repair diagnostics without changing the source mesh.
 * @param topology Existing topology diagnostics for the final transformed mesh.
 * @param robustness Existing robustness diagnostics for the same mesh.
 * @return Stable issue-level eligibility decisions and aggregate status.
 */
MeshRepairEligibility EvaluateMeshRepairEligibility(
    const MeshTopologyReport& topology,
    const MeshRobustnessReport& robustness);

/**
 * @brief Classify pre-repair diagnostics with explicit attribute evidence.
 * @param topology Existing topology diagnostics for the final transformed mesh.
 * @param robustness Existing robustness diagnostics for the same mesh.
 * @param evidence Attribute facts produced outside the policy.
 * @return Stable issue-level eligibility decisions and aggregate status.
 */
MeshRepairEligibility EvaluateMeshRepairEligibility(
    const MeshTopologyReport& topology,
    const MeshRobustnessReport& robustness,
    const MeshRepairEligibilityEvidence& evidence);

}  // namespace slicer_core
