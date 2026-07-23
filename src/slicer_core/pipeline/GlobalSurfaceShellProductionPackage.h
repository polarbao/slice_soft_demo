#pragma once

#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"
#include "slicer_core/pipeline/GlobalSurfaceShellProductionLayerAdapter.h"

namespace slicer_core
{

/**
 * @brief Publish admitted Global Surface Shell adapter output through the shared writer.
 * @param request Package metadata with an empty layer list and admitted production state.
 * @param adapter Exact full-closure adapter output. Passed by value to allow layer moves.
 * @return Published RGBWSV package summary.
 */
RgbwsvProductionPackageWriteResult
WriteGlobalSurfaceShellProductionPackage(
    RgbwsvProductionPackageWriteRequest request,
    GlobalSurfaceShellProductionLayerAdapterResult adapter);

}  // namespace slicer_core
