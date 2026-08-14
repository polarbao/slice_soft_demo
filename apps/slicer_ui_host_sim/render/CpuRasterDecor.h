#pragma once

#include "IRenderBackend.h"

namespace cpu_raster_detail
{

/** @brief 绘制宿主本地网格、构建体积与 XYZ 坐标轴。 */
void DrawSceneDecor(
    const slicer::render::FrameDesc& frame,
    slicer::render::ImageOut* output);

}  // namespace cpu_raster_detail
