#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/support/SupportShapeOptimizer.h"

namespace slicer_core
{

/**
 * @brief Convert support shape optimization result to report JSON.
 * @param policy Support shape policy.
 * @param result Support shape optimization result.
 * @return Report JSON object.
 */
Json MakeSupportShapeReport(
    const SupportShapePolicy& policy,
    const SupportShapeOptimizationResult& result);

}  // namespace slicer_core
