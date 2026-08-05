#pragma once

#include "slicer_core/api/scene/SceneFacadeService.h"
#include "slicer_core/api/viewdata/SceneViewResources.h"

#include <memory>

namespace slicer_core::api
{

/**
 * @brief Creates the Stage 14B-03A fail-closed textured ViewData provider.
 * @param models Immutable imported model repository.
 * @param textures Texture decoder/source used for declared texture assets.
 * @return Provider implementing the existing SceneFacade boundary.
 */
[[nodiscard]] ApiResult<
    std::shared_ptr<const ITexturedSceneViewDataProvider>>
CreateTexturedSceneViewDataProvider(
    std::shared_ptr<const ISceneViewModelRepository> models,
    std::shared_ptr<const ISceneViewTextureSource> textures) noexcept;

}  // namespace slicer_core::api
