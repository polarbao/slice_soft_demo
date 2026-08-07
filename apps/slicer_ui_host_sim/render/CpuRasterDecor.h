#pragma once

#include "IRenderBackend.h"

namespace cpu_raster_detail
{

/** @brief Draws the host-local grid, build volume and XYZ axes. */
void DrawSceneDecor(
    const slicer::render::FrameDesc& frame,
    slicer::render::ImageOut* output);

}  // namespace cpu_raster_detail
