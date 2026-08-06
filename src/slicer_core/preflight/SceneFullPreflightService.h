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
 * @brief Stable model-resolution failures used by authoritative scene preflight.
 */
enum class SceneFullPreflightResolutionErrorCode
{
    None,
    ResourceMissing,
    ImportInvalid,
};

/**
 * @brief Immutable model returned by the caller-owned resource resolver.
 */
struct SceneFullPreflightResolvedModel
{
    std::shared_ptr<const SceneModel> model;
    SceneFullPreflightResolutionErrorCode errorcode{
        SceneFullPreflightResolutionErrorCode::None};
    std::string detail;

    /**
     * @brief Report whether resolution returned usable immutable geometry.
     * @return True when a model exists and no resolution error is present.
     */
    bool IsValid() const;
};

using SceneFullPreflightModelResolver = std::function<
    SceneFullPreflightResolvedModel(const ModelSource&)>;

/**
 * @brief One stable scene or instance issue from authoritative preflight.
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
 * @brief Full source and transformed evidence for one scene instance.
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
 * @brief Complete input for one scene-wide authoritative preflight run.
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
 * @brief Stable scene-wide result without Worker or package concerns.
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
 * @brief Run full topology, transformed geometry, and collision preflight.
 */
class SceneFullPreflightService final
{
public:
    /**
     * @brief Audit every visible instance in one immutable committed scene.
     * @param request Scene identity, mode, resolver, options, and cancellation.
     * @return Stable authoritative or explicitly incomplete scene evidence.
     */
    SceneFullPreflightResult Run(
        const SceneFullPreflightRequest& request);

    /**
     * @brief Remove reusable transformed-model diagnostics.
     */
    void ClearCache();

private:
    static SceneFullPreflightResolvedModel ResolveModel(
        const SceneFullPreflightRequest& request,
        const ModelSource& source);

    TransformedModelPreflightService m_transformedPreflight;
};

}  // namespace slicer_core
