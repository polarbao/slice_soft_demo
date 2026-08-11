#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/viewdata/SceneViewResolvedAsset.h"
#include "slicer_core/scene/SceneModel.h"

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief Controls derived vertex-normal smoothing for display meshes.
 *
 * This is an internal ViewData build policy. It does not extend the public
 * SPI request DTO and therefore keeps existing hosts source-compatible.
 */
struct ViewMeshNormalOptions
{
    double crease_angle_degrees{40.0};
};

/**
 * @brief Builds a seam-safe indexed mesh with complete material bindings.
 * @param model Immutable imported model.
 * @param appearance Closed appearance resources.
 * @param worldMatrix Authoritative instance transform.
 * @param lod Requested actual mesh LOD.
 * @param meshTransform Local or world buffer semantics.
 * @param attributeFormat Serialized mesh attribute scalar encoding.
 * @param cancelToken Cooperative cancellation token.
 * @param normalOptions Derived display-normal smoothing policy.
 * @return Mesh payload or a stable geometry/material error.
 */
[[nodiscard]] ApiResult<ViewMesh> BuildViewMesh(
    const SceneModel& model,
    const ResolvedViewAppearance& appearance,
    const Matrix4d& worldMatrix,
    ViewLod lod,
    MeshTransform meshTransform,
    MeshAttributeFormat attributeFormat,
    const ICancelToken& cancelToken,
    const ViewMeshNormalOptions& normalOptions = {}) noexcept;

}  // namespace slicer_core::api::viewdata_detail
