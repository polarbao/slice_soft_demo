#pragma once

#include "slicer_core/api/SceneViewDtos.h"

#include <cstdint>

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief Estimates one mesh's serialized descriptor and binary payload bytes.
 * @param mesh Mesh with its requested wire attribute format.
 * @return Conservative serialized mesh byte estimate.
 */
[[nodiscard]] std::uint64_t EstimateViewMeshBytes(
    const ViewMesh& mesh) noexcept;

/**
 * @brief Estimates the serialized payload bytes of inline ViewData resources.
 * @param viewData Complete candidate response.
 * @return Conservative byte estimate including resource payloads.
 */
[[nodiscard]] std::uint64_t EstimateViewDataBytes(
    const SceneViewData& viewData) noexcept;

}  // namespace slicer_core::api::viewdata_detail
