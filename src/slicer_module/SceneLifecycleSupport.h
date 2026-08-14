#pragma once

#include "slicer_core/api/scene/SceneFacadeService.h"
#include "slicer_core/api/viewdata/SceneViewResources.h"
#include "slicer_core/json_value.h"
#include "slicer_module/ModelCapabilityAdapter.h"

#include <map>
#include <memory>
#include <mutex>

namespace slicesoft::module::scene_lifecycle
{

/** @brief 场景创建后仍可接收模型的线程安全模型仓库。 */
class MutableSceneViewModelRepository final
    : public slicer_core::api::ISceneViewModelRepository
{
public:
    /**
     * @brief 注册或验证一个不可变模型资源。
     * @param modelId model.import 返回的数值模型标识。
     * @param model 不可变模型资源。
     * @return 成功结果或标识/资源冲突。
     */
    [[nodiscard]] slicer_core::api::ApiResult<void> Register(
        slicer_core::api::ModelId modelId,
        std::shared_ptr<const slicer_core::SceneModel> model) noexcept;

    /** @copydoc slicer_core::api::ISceneViewModelRepository::GetModel */
    [[nodiscard]] slicer_core::api::ApiResult<
        std::shared_ptr<const slicer_core::SceneModel>> GetModel(
        slicer_core::api::ModelId modelId) const noexcept override;

private:
    mutable std::mutex m_mutex;
    std::map<
        slicer_core::api::ModelId,
        std::shared_ptr<const slicer_core::SceneModel>> m_models;
};

/**
 * @brief 从宿主持有的上下文构造生产空场景种子。
 * @param sceneContext DTO v1.6 的 sceneContext 对象。
 * @param sceneId 数值 SceneFacade 标识。
 * @return 已验证种子或稳定输入错误。
 */
[[nodiscard]] slicer_core::api::ApiResult<
    slicer_core::api::SceneFacadeSeed> BuildImplicitSceneSeed(
    const slicer_core::Json& sceneContext,
    slicer_core::api::SceneId sceneId) noexcept;

/**
 * @brief 将已导入模型资源映射为一个场景注册项。
 * @param resource ModelCapabilityAdapter 保留的模型。
 * @return 可传给 SceneFacadeService::RegisterModel 的注册项。
 */
[[nodiscard]] slicer_core::api::ApiResult<
    slicer_core::api::SceneFacadeModelRegistration> BuildModelRegistration(
    const ImportedModelResource& resource) noexcept;

}  // namespace slicesoft::module::scene_lifecycle
