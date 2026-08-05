#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/SceneViewDtos.h"

#include <vector>

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief Extracts deterministic closed local-XY outlines from a surface preview.
 * @param preview Valid straight-alpha RGBA8 surface preview in local bounds.
 * @return Closed outline loops, including disconnected components and holes, or a stable error.
 */
[[nodiscard]] ApiResult<std::vector<ViewOutline>> BuildViewOutlines(
    const SurfacePreview& preview) noexcept;

}  // namespace slicer_core::api::viewdata_detail
