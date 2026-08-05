#pragma once

#include "slicer_core/api/SceneViewDtos.h"

#include <string>

namespace slicer_core::api::viewdata_detail
{

/** @brief Computes a content identity for decoded texture pixels. */
[[nodiscard]] std::string ComputeTextureIdentity(
    const ViewTexture& texture);

/** @brief Computes a material-binding identity for one appearance. */
[[nodiscard]] std::string ComputeAppearanceIdentity(
    const ViewAppearance& appearance);

/** @brief Computes a geometry and LOD identity for one mesh. */
[[nodiscard]] std::string ComputeMeshIdentity(const ViewMesh& mesh);

/** @brief Computes a local-space textured top-preview identity. */
[[nodiscard]] std::string ComputePreviewIdentity(
    const SurfacePreview& preview);

/** @brief Computes the identity of a complete scene view snapshot. */
[[nodiscard]] std::string ComputeViewDataIdentity(
    const SceneViewData& viewData);

}  // namespace slicer_core::api::viewdata_detail
