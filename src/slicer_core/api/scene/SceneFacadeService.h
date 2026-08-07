#pragma once

#include "slicer_core/api/SceneFacade.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneModel.h"

#include <map>
#include <memory>
#include <string>

namespace slicer_core::api
{

/**
 * @brief Immutable imported model registration available to addInstance.
 */
struct SceneFacadeModelRegistration
{
    /** @brief Numeric model identity returned by model.import. */
    ModelId api_model_id{0};
    /** @brief Stable scene-local model identity. */
    std::string scene_model_id;
    /** @brief Source and resource hashes retained by the scene. */
    ModelSource source;
    /** @brief Resource boundary used to resolve adjacent model assets. */
    ResourceScope scope;
    /** @brief Immutable geometry and appearance resource. */
    std::shared_ptr<const SceneModel> model;
};

/**
 * @brief Provider boundary implemented by Stage 14B-03A for contract-v1.2 views.
 */
class ITexturedSceneViewDataProvider
{
public:
    /** @brief Destroys the provider through its interface. */
    virtual ~ITexturedSceneViewDataProvider() = default;

    /**
     * @brief Builds fail-closed textured view data for one authoritative snapshot.
     * @param request Requested view mode, revision, instances, LOD, and budget.
     * @param snapshot Authoritative committed scene snapshot.
     * @param cancelToken Cooperative cancellation token.
     * @return ViewData v1.2 payload or a stable PM-SLICER error.
     */
    [[nodiscard]] virtual ApiResult<SceneViewData> GetViewData(
        const SceneViewDataRequest& request,
        const SceneSnapshot& snapshot,
        const ICancelToken& cancelToken) const noexcept = 0;
};

/**
 * @brief Dependencies and authoritative state used to create one SceneFacade.
 */
struct SceneFacadeSeed
{
    /** @brief Numeric API identity owned by the facade instance. */
    SceneId scene_id{0};
    /** @brief Existing authoritative scene representation. */
    MultiModelScene scene;
    /** @brief Immutable source geometry keyed by scene model identity. */
    std::map<std::string, std::shared_ptr<const SceneModel>> models_by_id;
    /** @brief Numeric API model identities keyed by scene model identity. */
    std::map<std::string, ModelId> api_model_ids;
    /** @brief Imported models that may be added after facade creation. */
    std::map<ModelId, SceneFacadeModelRegistration> registered_models;
    /** @brief Existing scene/layout admission purpose to reuse. */
    SceneValidationPurpose validation_purpose{SceneValidationPurpose::Draft};
    /** @brief Existing collision contact tolerance in millimetres. */
    double contact_epsilon_mm{0.0};
};

/**
 * @brief Qt-free authoritative scene service for the three-lane Commit contract.
 */
class SceneFacadeService final : public SceneFacade
{
public:
    /**
     * @brief Creates a validated facade without mutating the supplied scene.
     * @param seed Scene state, source models, identities, and validation purpose.
     * @param viewDataProvider Optional Stage 14B-03A textured ViewData provider.
     * @return Ready facade or a stable PM-SLICER error.
     */
    [[nodiscard]] static ApiResult<std::shared_ptr<SceneFacadeService>> Create(
        SceneFacadeSeed seed,
        std::shared_ptr<const ITexturedSceneViewDataProvider> viewDataProvider = {}) noexcept;

    /** @brief Destroys the facade and its authoritative scene state. */
    ~SceneFacadeService() override;

    /**
     * @brief Registers one imported model for a future addInstance operation.
     * @param registration Stable source, scope, geometry, and API identity.
     * @return Success or a fail-closed identity/resource conflict.
     */
    [[nodiscard]] ApiResult<void> RegisterModel(
        SceneFacadeModelRegistration registration) noexcept;

    /** @copydoc SceneFacade::ApplyOperation */
    [[nodiscard]] ApiResult<SceneCommitResult> ApplyOperation(
        const SceneOperationRequest& request,
        const ICancelToken& cancelToken) noexcept override;

    /** @copydoc SceneFacade::GetSnapshot */
    [[nodiscard]] ApiResult<SceneSnapshot> GetSnapshot(
        SceneId sceneId) const noexcept override;

    /** @copydoc SceneFacade::GetViewData */
    [[nodiscard]] ApiResult<SceneViewData> GetViewData(
        const SceneViewDataRequest& request,
        const ICancelToken& cancelToken) const noexcept override;

    /** @copydoc SceneFacade::CheckCollision */
    [[nodiscard]] ApiResult<CollisionReport> CheckCollision(
        const SceneSnapshot& snapshot,
        const ICancelToken& cancelToken) const noexcept override;

private:
    class Impl;

    explicit SceneFacadeService(std::unique_ptr<Impl> implementation) noexcept;

    std::unique_ptr<Impl> m_implementation;
};

}  // namespace slicer_core::api
