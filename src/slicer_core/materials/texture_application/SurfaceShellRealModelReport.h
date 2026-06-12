#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/materials/texture_application/SurfaceShellRealModelPrototype.h"

namespace slicer_core
{

/**
 * @brief Build the 09B-R1 real-model report schema v2.
 * @param result Real-model prototype result.
 * @return Report JSON.
 */
Json MakeSurfaceShellRealModelReport(const SurfaceShellRealModelResult& result);

}  // namespace slicer_core
