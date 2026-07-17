#pragma once

#include "slicer_core/materials/texture_application/GlobalTextureFillPartitionService.h"

namespace slicer_core
{

/**
 * @brief Optional OpenVDB diagnostic backend for Stage 12E conformance evidence.
 */
class OpenVdbTextureFillConformanceBackend final
    : public IGlobalTextureFillPartitionBackend
{
public:
    /**
     * @brief Build one OpenVDB occupancy and exact-distance partition candidate.
     * @param request Final transformed mesh, target grid, and width options.
     * @return Diagnostic candidate or stable unavailable/blocked evidence.
     */
    GlobalTextureFillPartitionCandidate Evaluate(
        const GlobalTextureFillPartitionRequest& request) const override;
};

}  // namespace slicer_core
