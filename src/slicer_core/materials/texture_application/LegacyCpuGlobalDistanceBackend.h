#pragma once

#include "slicer_core/materials/texture_application/GlobalTextureFillPartitionService.h"

namespace slicer_core
{

/**
 * @brief Default-build diagnostic backend using CPU closed-mesh occupancy and Euclidean distance.
 */
class LegacyCpuGlobalDistanceBackend final
    : public IGlobalTextureFillPartitionBackend
{
public:
    /**
     * @brief Build one whole-model diagnostic texture/fill partition candidate.
     * @param request Final transformed mesh, target grid, and width options.
     * @return Candidate masks, dynamic width metrics, references, and core evidence.
     */
    GlobalTextureFillPartitionCandidate Evaluate(
        const GlobalTextureFillPartitionRequest& request) const override;
};

}  // namespace slicer_core
