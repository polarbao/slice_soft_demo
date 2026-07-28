#pragma once

#include "slicer_core/pipeline/SceneRasterTypes.h"
#include "slicer_core/scene/ModelInstance.h"

namespace slicer_core
{

/**
 * @brief Immutable request for reusing one translated instance raster.
 */
struct TranslatedSceneRasterReuseRequest
{
    const SceneInstanceRaster* prototype{nullptr};
    ModelInstance prototypeinstance;
    SceneRasterIdentity targetidentity;
    ModelInstance targetinstance;
    double quantizationtolerance{0.500001};
};

/**
 * @brief Rebind writer-ready layers when only the instance XY translation changed.
 * @param request Prototype raster, source/target instances, and admitted target identity.
 * @return Rebound raster or a structured fail-closed adapter error.
 */
SceneRasterAdapterResult ReuseTranslatedSceneRaster(
    const TranslatedSceneRasterReuseRequest& request);

}  // namespace slicer_core

