#pragma once

#include "slicer_core/preflight/RepairedAssetIntakeTypes.h"

namespace slicer_core
{

/**
 * @brief Audit one candidate against an R4-06 required model family.
 *
 * The service is diagnostic-only. It does not repair geometry, modify source
 * assets, start a slice pipeline, or write production output.
 */
class RepairedAssetIntakeService
{
public:
    /**
     * @brief Execute manifest, identity, attribute, repeatability and post-strict checks.
     * @param request Required-family candidate request.
     * @return Deterministic diagnostic intake result.
     */
    RepairedAssetIntakeResult Run(
        const RepairedAssetIntakeRequest& request) const;
};

}  // namespace slicer_core
