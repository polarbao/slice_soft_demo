#pragma once

#include "slicer_core/SliceRunTelemetry.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/model.h"
#include "slicer_core/pipeline/SceneRasterTypes.h"
#include "slicer_core/scene/ModelInstance.h"

#include <filesystem>

namespace slicer_core
{

/**
 * @brief Request for extracting one Legacy instance as in-memory RGBWSV layers.
 */
struct LegacySceneLayerAdapterRequest
{
    std::filesystem::path configpath;
    std::filesystem::path modelpathoverride;
    std::string modelformatoverride{"auto"};
    SceneRasterIdentity identity;
    ModelInstance instance;
    const ModelReport* modelreportoverride{nullptr};
    SliceRunProgressCallback progresscallback;

    /** @brief Synchronous, non-owning cancellation source for this adapter run. */
    const api::ICancelToken* canceltoken{nullptr};
};

/**
 * @brief Run the existing Legacy producer without file output.
 * @param request Config path and immutable admitted instance identity.
 * @return Complete instance raster or one structured fail-closed error.
 */
SceneRasterAdapterResult AdaptLegacySceneLayers(
    const LegacySceneLayerAdapterRequest& request);

}  // namespace slicer_core
