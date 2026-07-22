#pragma once

#include "slicer_core/preflight/ModelPreflightTypes.h"

namespace slicer_core
{

/**
 * @brief Explicit runtime capabilities used by model preflight admission.
 */
struct ModelPreflightAdmissionContext
{
    bool global_backend_available{false};
};

/**
 * @brief Derive legacy and global mode admission from shared preflight facts.
 * @param diagnosticResult Fresh backend-neutral model diagnostics.
 * @param context Explicit backend capabilities for mode-specific admission.
 * @return A result copy with deterministic legacy and global admission decisions.
 */
ModelPreflightResult EvaluateModelPreflightAdmissions(
    const ModelPreflightResult& diagnosticResult,
    const ModelPreflightAdmissionContext& context);

}  // namespace slicer_core
