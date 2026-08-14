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
 * @brief 可供 addInstance 使用的不可变已导入模型注册项。
 */
struct SceneFacadeModelRegistration
{
    /** @brief model.import 返回的数值模型标识。 */
    ModelId api_model_id{0};
    /** @brief 稳定的场景内模型标识。 */
    std::string scene_model_id;
    /** @brief 场景保留的源哈希和资源哈希。 */
    ModelSource source;
    /** @brief 用于解析模型文件相邻附属资源的作用域。 */
    ResourceScope scope;
    /** @brief 不可变的几何和外观资源。 */
    std::shared_ptr<const SceneModel> model;
};

/**
 * @brief Stage 14B-03A 为 contract-v1.2 视图实现的提供者边界。
 */
class ITexturedSceneViewDataProvider
{
public:
    /** @brief 通过接口销毁提供者。 */
    virtual ~ITexturedSceneViewDataProvider() = default;

    /**
     * @brief 为一个权威快照构造失败即拒绝的纹理视图数据。
     * @param request 请求的视图模式、修订号、实例、LOD 和预算。
     * @param snapshot 权威已提交场景快照。
     * @param cancelToken 协作式取消令牌。
     * @return ViewData v1.2 载荷或稳定的 PM-SLICER 错误。
     */
    [[nodiscard]] virtual ApiResult<SceneViewData> GetViewData(
        const SceneViewDataRequest& request,
        const SceneSnapshot& snapshot,
        const ICancelToken& cancelToken) const noexcept = 0;
};

/**
 * @brief 创建 SceneFacade 所需的依赖和权威状态。
 */
struct SceneFacadeSeed
{
    /** @brief SceneFacade 实例持有的数值 API 标识。 */
    SceneId scene_id{0};
    /** @brief 现有权威场景表示。 */
    MultiModelScene scene;
    /** @brief 以场景模型标识为键的不可变源几何。 */
    std::map<std::string, std::shared_ptr<const SceneModel>> models_by_id;
    /** @brief 以场景模型标识为键的数值 API 模型标识。 */
    std::map<std::string, ModelId> api_model_ids;
    /** @brief SceneFacade 创建后仍可添加的已导入模型。 */
    std::map<ModelId, SceneFacadeModelRegistration> registered_models;
    /** @brief 要复用的现有场景/排版准入用途。 */
    SceneValidationPurpose validation_purpose{SceneValidationPurpose::Draft};
    /** @brief 以毫米表示的现有碰撞接触容差。 */
    double contact_epsilon_mm{0.0};
};

/**
 * @brief 面向三通道 Commit 合同的无 Qt 权威场景服务。
 */
class SceneFacadeService final : public SceneFacade
{
public:
    /**
     * @brief 在不修改所给场景的前提下创建已验证 SceneFacade。
     * @param seed 场景状态、源模型、标识和验证用途。
     * @param viewDataProvider 可选的 Stage 14B-03A 纹理 ViewData 提供者。
     * @return 就绪的 SceneFacade 或稳定的 PM-SLICER 错误。
     */
    [[nodiscard]] static ApiResult<std::shared_ptr<SceneFacadeService>> Create(
        SceneFacadeSeed seed,
        std::shared_ptr<const ITexturedSceneViewDataProvider> viewDataProvider = {}) noexcept;

    /** @brief 销毁 SceneFacade 及其权威场景状态。 */
    ~SceneFacadeService() override;

    /**
     * @brief 为后续 addInstance 操作注册一个已导入模型。
     * @param registration 稳定的源、作用域、几何和 API 标识。
     * @return 成功结果或失败即拒绝的标识/资源冲突。
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
