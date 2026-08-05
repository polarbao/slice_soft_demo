#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/viewdata/SceneViewResolvedAsset.h"
#include "slicer_core/scene/SceneModel.h"

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief Rasterizes a local +Z textured surface preview.
 * @param model Immutable imported model.
 * @param appearance Closed appearance resources.
 * @param maximumDimension Maximum preview width or height.
 * @param cancelToken Cooperative cancellation token.
 * @return Straight-alpha RGBA8 preview or a stable error.
 */
[[nodiscard]] ApiResult<SurfacePreview> BuildSurfacePreview(
    const SceneModel& model,
    const ResolvedViewAppearance& appearance,
    int maximumDimension,
    const ICancelToken& cancelToken) noexcept;

}  // namespace slicer_core::api::viewdata_detail
