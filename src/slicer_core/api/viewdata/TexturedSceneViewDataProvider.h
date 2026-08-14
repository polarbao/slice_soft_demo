#pragma once

#include "slicer_core/api/scene/SceneFacadeService.h"
#include "slicer_core/api/viewdata/SceneViewResources.h"

#include <memory>

namespace slicer_core::api
{

/**
 * @brief 创建 Stage 14B-03A 失败即拒绝的纹理 ViewData 提供者。
 * @param models 不可变的已导入模型仓库。
 * @param textures 已声明纹理资源使用的纹理解码器/来源。
 * @return 实现现有 SceneFacade 边界的提供者。
 */
[[nodiscard]] ApiResult<
    std::shared_ptr<const ITexturedSceneViewDataProvider>>
CreateTexturedSceneViewDataProvider(
    std::shared_ptr<const ISceneViewModelRepository> models,
    std::shared_ptr<const ISceneViewTextureSource> textures) noexcept;

}  // namespace slicer_core::api
