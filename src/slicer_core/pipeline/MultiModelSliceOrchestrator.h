#pragma once

#include "slicer_core/layout/SceneCollisionService.h"
#include "slicer_core/pipeline/SceneLayerComposer.h"

#include <cstdint>
#include <vector>

namespace slicer_core
{

/**
 * @brief Immutable request for composing admitted per-instance rasters.
 */
struct MultiModelLayerComposeRequest
{
    SceneCollisionResult admission;
    std::uint64_t currentscenerevision{0U};
    SlicePipelineMode effectivepipelinemode{
        SlicePipelineMode::Legacy};
    std::vector<SceneInstanceRaster> instances;
    double quantizationtolerance{1.0e-6};
};

/**
 * @brief Build one shared grid and compose 13B-04-admitted rasters.
 * @param request Scene admission evidence, revision, mode, and rasters.
 * @return Writer-ready in-memory scene layers or a fail-closed error.
 */
SceneLayerComposeResult ComposeAdmittedSceneRasters(
    const MultiModelLayerComposeRequest& request);

}  // namespace slicer_core
