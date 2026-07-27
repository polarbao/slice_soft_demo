#pragma once

#include "slicer_core/pipeline/SceneRasterTypes.h"

namespace slicer_core
{

/**
 * @brief Compose admitted per-instance RGBWSV rasters into one shared scene raster.
 * @param request Immutable scene grid, protocol, revision, and instance layers.
 * @return Complete writer-ready layers or one structured fail-closed error.
 */
SceneLayerComposeResult ComposeSceneLayers(
    const SceneLayerComposeRequest& request);

}  // namespace slicer_core
