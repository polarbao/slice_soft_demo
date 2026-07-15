#pragma once

#include "slicer_core/config.h"
#include "slicer_core/json_value.h"

namespace slicer_core
{

/**
 * @brief Builds the Stage 12D material-closure report before a diagnostic source is available.
 * @param config Material-closure configuration snapshot.
 * @param layerCount Number of layers in the generated package.
 * @return Report object conforming to p0.material_closure.1.
 */
Json BuildMaterialClosureReportSkeleton(const MaterialClosureConfig& config, int layerCount);

/**
 * @brief Builds the material-closure summary embedded in slice_report totals.
 * @param report Full p0.material_closure.1 report.
 * @return Stable summary object referencing the canonical report path.
 */
Json BuildMaterialClosureSliceSummary(const Json& report);

}  // namespace slicer_core
