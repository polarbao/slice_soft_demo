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

/** @brief Thread-safe model repository that accepts models after scene creation. */
class MutableSceneViewModelRepository final
    : public slicer_core::api::ISceneViewModelRepository
{
public:
    /**
     * @brief Registers or validates one immutable model resource.
     * @param modelId Numeric model identity returned by model.import.
     * @param model Immutable model resource.
     * @return Success or an identity/resource conflict.
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
 * @brief Builds a production empty-scene seed from host-owned context.
 * @param sceneContext DTO v1.6 sceneContext object.
 * @param sceneId Numeric facade identity.
 * @return Validated seed or a stable input error.
 */
[[nodiscard]] slicer_core::api::ApiResult<
    slicer_core::api::SceneFacadeSeed> BuildImplicitSceneSeed(
    const slicer_core::Json& sceneContext,
    slicer_core::api::SceneId sceneId) noexcept;

/**
 * @brief Maps an imported model resource to one scene registration.
 * @param resource Model retained by ModelCapabilityAdapter.
 * @return Registration suitable for SceneFacadeService::RegisterModel.
 */
[[nodiscard]] slicer_core::api::ApiResult<
    slicer_core::api::SceneFacadeModelRegistration> BuildModelRegistration(
    const ImportedModelResource& resource) noexcept;

}  // namespace slicesoft::module::scene_lifecycle
