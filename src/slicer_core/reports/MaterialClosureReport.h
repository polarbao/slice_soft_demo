#pragma once

#include "slicer_core/config.h"
#include "slicer_core/diagnostics/MaterialClosureCandidateDetector.h"
#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"
#include "slicer_core/json_value.h"

#include <vector>

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
 * @brief Builds a candidate-only material-closure report inferred from final RGBWSV layers.
 * @param config Material-closure configuration snapshot.
 * @param layers Candidate evidence for every evaluated package layer.
 * @return Candidate report that is never valid for production acceptance.
 */
Json BuildMaterialClosureCandidateReport(
    const MaterialClosureConfig& config,
    const std::vector<MaterialClosureCandidateLayer>& layers);

/**
 * @brief Builds an exact material-closure report from composer semantic masks.
 * @param config Material-closure configuration snapshot.
 * @param layers Exact semantic evidence for every evaluated package layer.
 * @return Production-evaluable report using post-repair remaining gaps when repair was attempted.
 */
Json BuildMaterialClosureExactReport(
    const MaterialClosureConfig& config,
    const std::vector<MaterialClosureSemanticLayerResult>& layers);

/**
 * @brief Builds the material-closure summary embedded in slice_report totals.
 * @param report Full p0.material_closure.1 report.
 * @return Stable summary object referencing the canonical report path.
 */
Json BuildMaterialClosureSliceSummary(const Json& report);

}  // namespace slicer_core
