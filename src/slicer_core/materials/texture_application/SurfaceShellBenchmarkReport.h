#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/materials/texture_application/SurfaceShellRealModelPrototype.h"

namespace slicer_core
{

/**
 * @brief Build a non-strict performance benchmark report for 09B-R2.
 * @param result Real-model prototype result.
 * @param fixtureId Stable fixture identifier.
 * @param buildConfig Build configuration name.
 * @return Benchmark report JSON.
 */
Json MakeSurfaceShellBenchmarkReport(
    const SurfaceShellRealModelResult& result,
    const std::string& fixtureId,
    const std::string& buildConfig);

}  // namespace slicer_core
