#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/viewdata/SceneViewResolvedAsset.h"
#include "slicer_core/scene/SceneModel.h"

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief Builds a seam-safe indexed mesh with complete material bindings.
 * @param model Immutable imported model.
 * @param appearance Closed appearance resources.
 * @param worldMatrix Authoritative instance transform.
 * @param lod Requested actual mesh LOD.
 * @param meshTransform Local or world buffer semantics.
 * @param attributeFormat Serialized mesh attribute scalar encoding.
 * @param cancelToken Cooperative cancellation token.
 * @return Mesh payload or a stable geometry/material error.
 */
[[nodiscard]] ApiResult<ViewMesh> BuildViewMesh(
    const SceneModel& model,
    const ResolvedViewAppearance& appearance,
    const Matrix4d& worldMatrix,
    ViewLod lod,
    MeshTransform meshTransform,
    MeshAttributeFormat attributeFormat,
    const ICancelToken& cancelToken) noexcept;

}  // namespace slicer_core::api::viewdata_detail
