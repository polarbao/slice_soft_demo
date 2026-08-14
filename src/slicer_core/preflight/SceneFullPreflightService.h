#pragma once

#include "slicer_core/layout/SceneCollisionService.h"
#include "slicer_core/preflight/TransformedModelPreflight.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief 权威场景预检使用的稳定模型解析失败分类。
 */
enum class SceneFullPreflightResolutionErrorCode
{
    None,
    ResourceMissing,
    ImportInvalid,
};

/**
 * @brief 调用方持有的资源解析器返回的不可变模型。
 */
struct SceneFullPreflightResolvedModel
{
    std::shared_ptr<const SceneModel> model;
    SceneFullPreflightResolutionErrorCode errorcode{
        SceneFullPreflightResolutionErrorCode::None};
    std::string detail;

    /**
     * @brief 报告解析是否返回可用的不可变几何。
     * @return 模型存在且无解析错误时返回 true。
     */
    bool IsValid() const;
};

using SceneFullPreflightModelResolver = std::function<
    SceneFullPreflightResolvedModel(const ModelSource&)>;

/**
 * @brief 权威预检产生的一条稳定场景或实例问题。
 */
struct SceneFullPreflightIssue
{
    std::string code;
    ModelPreflightIssueSeverity severity{
        ModelPreflightIssueSeverity::Info};
    std::uint64_t count{0U};
    std::string detail;
    std::string modelid;
    std::string instanceid;
    Json context{Json::object({})};
};

/**
 * @brief 一个场景实例的完整源证据和变换后证据。
 */
struct SceneFullPreflightInstanceResult
{
    std::string modelid;
    std::string instanceid;
    std::uint64_t transformrevision{0U};
    std::string transformhash;
    bool visible{true};
    bool skippedhidden{false};
    bool complete{false};
    bool blocked{true};
    bool outofbounds{false};
    ModelPreflightStatus sourcestatus{ModelPreflightStatus::NotRun};
    ModelPreflightStatus transformedstatus{ModelPreflightStatus::NotRun};
    ModeAdmissionResult legacyadmission;
    ModeAdmissionResult globaladmission{
        ModelPreflightPipelineMode::GlobalSurfaceShell,
        ModelPreflightAdmissionStatus::Blocked,
        {},
        {}};
    MeshRepairDiagnosticsSummary topology;
    BoundingBox bboxmm;
    std::vector<SceneFullPreflightIssue> issues;
};

/**
 * @brief 一次全场景权威预检运行的完整输入。
 */
struct SceneFullPreflightRequest
{
    const MultiModelScene* scene{nullptr};
    std::string scenehash;
    std::uint64_t expectedscenerevision{0U};
    ModelPreflightPipelineMode targetmode{
        ModelPreflightPipelineMode::Legacy};
    ModelPreflightOptions options;
    ModelPreflightAdmissionContext admissioncontext;
    SceneFullPreflightModelResolver modelresolver;
    std::function<bool()> cancellationrequested;
};

/**
 * @brief 不承担 Worker 或生产包职责的稳定全场景预检结果。
 */
struct SceneFullPreflightResult
{
    std::string sceneid;
    std::uint64_t scenerevision{0U};
    std::string scenehash;
    ModelPreflightPipelineMode targetmode{
        ModelPreflightPipelineMode::Legacy};
    bool authoritative{false};
    bool productionadmitted{false};
    bool cancelled{false};
    bool complete{false};
    std::size_t checkedmodelcount{0U};
    std::size_t checkedinstancecount{0U};
    std::size_t blockedinstancecount{0U};
    std::size_t skippedinstancecount{0U};
    std::vector<SceneFullPreflightInstanceResult> instances;
    std::vector<SceneFullPreflightIssue> sceneissues;
    std::vector<SceneCollisionPair> collisions;
    std::vector<std::string> outofboundsinstances;
};

/**
 * @brief 运行完整拓扑、变换后几何和碰撞预检。
 */
class SceneFullPreflightService final
{
public:
    /**
     * @brief 审计一个不可变已提交场景中的每个可见实例。
     * @param request 场景标识、模式、解析器、选项和取消状态。
     * @return 稳定的权威场景证据，或明确标记不完整的证据。
     */
    SceneFullPreflightResult Run(
        const SceneFullPreflightRequest& request);

    /**
     * @brief 移除可复用的变换后模型诊断缓存。
     */
    void ClearCache();

private:
    static SceneFullPreflightResolvedModel ResolveModel(
        const SceneFullPreflightRequest& request,
        const ModelSource& source);

    TransformedModelPreflightService m_transformedPreflight;
};

}  // namespace slicer_core
