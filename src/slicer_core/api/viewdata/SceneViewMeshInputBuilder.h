#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/viewdata/MeshSimplifier.h"
#include "slicer_core/scene/SceneModel.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief Builds shared topology for one material before LOD simplification.
 * @param model Immutable imported model.
 * @param triangleIndices Source triangles owned by one material.
 * @param worldMatrix Authoritative instance transform.
 * @param meshTransform Local or world buffer semantics.
 * @param cancelToken Cooperative cancellation token.
 * @return Indexed positions and UVs or a stable geometry error.
 */
[[nodiscard]] ApiResult<MeshSimplificationInput> BuildViewMeshGroupInput(
    const SceneModel& model,
    const std::vector<std::size_t>& triangleIndices,
    const Matrix4d& worldMatrix,
    MeshTransform meshTransform,
    const ICancelToken& cancelToken);

/**
 * @brief Reads one position from a validated simplification input.
 * @param input Validated simplification input.
 * @param index Vertex index.
 * @return Position value.
 */
[[nodiscard]] Vec3 ReadSimplificationPoint(
    const MeshSimplificationInput& input,
    std::uint32_t index);

/**
 * @brief Reads one UV coordinate from a validated simplification input.
 * @param input Validated simplification input.
 * @param index Vertex index.
 * @return UV value.
 */
[[nodiscard]] TexCoord ReadSimplificationUv(
    const MeshSimplificationInput& input,
    std::uint32_t index);

}  // namespace slicer_core::api::viewdata_detail
