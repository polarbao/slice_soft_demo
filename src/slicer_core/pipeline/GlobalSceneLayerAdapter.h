#pragma once

#include "slicer_core/pipeline/GlobalSurfaceShellProductionLayerAdapter.h"
#include "slicer_core/pipeline/SceneRasterTypes.h"

namespace slicer_core
{

/**
 * @brief Request for wrapping one Global writer-ready layer list.
 */
struct GlobalSceneLayerAdapterRequest
{
    SceneRasterIdentity identity;
    SceneRasterGrid localgrid;
    const GlobalSurfaceShellProductionLayerAdapterResult* source{
        nullptr};
};

/**
 * @brief Wrap existing Global writer-ready layers for scene composition.
 * @param request Admitted identity, explicit local grid, and Global layers.
 * @return Complete instance raster or one structured fail-closed error.
 */
SceneRasterAdapterResult AdaptGlobalSceneLayers(
    const GlobalSceneLayerAdapterRequest& request);

}  // namespace slicer_core
