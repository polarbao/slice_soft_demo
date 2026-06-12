#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/materials/texture_application/SurfaceShellTexturePrototype.h"

namespace slicer_core
{

/**
 * @brief Build the 09B surface shell texture report JSON.
 * @param result Prototype result.
 * @return Report JSON.
 */
Json MakeSurfaceShellTextureReport(const SurfaceShellTextureResult& result);

}  // namespace slicer_core
