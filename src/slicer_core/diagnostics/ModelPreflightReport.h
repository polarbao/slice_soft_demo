#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/preflight/ModelPreflightTypes.h"

namespace slicer_core
{

/**
 * @brief Build a not-run model preflight report skeleton.
 * @param identity Stable cache identity for the imported model state.
 * @return JSON conforming to slicesoft.model_preflight.12e_08c_r4.1.
 */
Json BuildModelPreflightReportSkeleton(
    const ModelPreflightCacheIdentity& identity);

/**
 * @brief Serialize one backend-neutral model preflight result.
 * @param result Model preflight result with both mode admissions.
 * @return JSON conforming to slicesoft.model_preflight.12e_08c_r4.1.
 */
Json BuildModelPreflightReport(const ModelPreflightResult& result);

}  // namespace slicer_core
