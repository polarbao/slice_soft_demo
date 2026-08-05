#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/viewdata/SceneViewResources.h"

#include <map>
#include <string>
#include <vector>

namespace slicer_core::api::viewdata_detail
{

/** @brief One source-material binding resolved into ViewData identities. */
struct ResolvedViewMaterial
{
    std::string source_name;
    ViewMaterial material;
    bool has_texture{false};
    std::size_t texture_index{0U};
};

/** @brief Closed material and texture resources used by one model. */
struct ResolvedViewAppearance
{
    ViewAppearance appearance;
    std::vector<ResolvedViewMaterial> materials;
    std::map<std::string, std::size_t> material_indices;
    bool has_texture{false};
};

/**
 * @brief Resolves only material resources reachable from model triangles.
 * @param model Immutable imported model.
 * @param textureSource Fail-closed texture decoder.
 * @param cancelToken Cooperative cancellation token.
 * @return Closed appearance or a stable PM-SLICER error.
 */
[[nodiscard]] ApiResult<ResolvedViewAppearance> ResolveViewAppearance(
    const SceneModel& model,
    const ISceneViewTextureSource& textureSource,
    const ICancelToken& cancelToken) noexcept;

/**
 * @brief Resolves the appearance material used by one triangle.
 * @param model Source model.
 * @param appearance Closed appearance.
 * @param triangleIndex Triangle index.
 * @return Resolved material index or a stable binding error.
 */
[[nodiscard]] ApiResult<std::size_t> ResolveTriangleMaterialIndex(
    const SceneModel& model,
    const ResolvedViewAppearance& appearance,
    std::size_t triangleIndex) noexcept;

}  // namespace slicer_core::api::viewdata_detail
