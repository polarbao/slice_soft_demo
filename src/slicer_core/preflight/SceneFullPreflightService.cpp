#include "slicer_core/preflight/SceneFullPreflightService.h"

#include "slicer_core/scene/SceneViewGeometry.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>

// 文件职责：聚合源拓扑、变换后几何、构建体积和碰撞的权威预检证据；
// 边界：资源解析失败、取消或证据不完整时必须明确阻断生产准入。
namespace slicer_core
{
namespace
{

constexpr const char* kStaleCode = "PM-SLICER-LAYOUT-0022";
constexpr const char* kInputMissingCode = "PM-SLICER-INPUT-0001";
constexpr const char* kInputInvalidCode = "PM-SLICER-INPUT-0002";
constexpr const char* kProfileCode = "PM-SLICER-PROFILE-0030";
constexpr const char* kCancelledCode = "PM-SLICER-CANCELLED-0070";
constexpr const char* kInternalCode = "PM-SLICER-INTERNAL-0099";

bool IsCancelled(const SceneFullPreflightRequest& request)
{
    return request.cancellationrequested
        && request.cancellationrequested();
}

SceneFullPreflightIssue MakeIssue(
    const std::string& code,
    const ModelPreflightIssueSeverity severity,
    const std::string& detail,
    const std::string& modelId = {},
    const std::string& instanceId = {},
    const Json& context = Json::object({}),
    const std::uint64_t count = 1U)
{
    return {code, severity, count, detail, modelId, instanceId, context};
}

void SortIssues(std::vector<SceneFullPreflightIssue>& issues)
{
    std::sort(
        issues.begin(),
        issues.end(),
        [](const SceneFullPreflightIssue& left,
           const SceneFullPreflightIssue& right)
        {
            return std::tie(
                       left.modelid,
                       left.instanceid,
                       left.code,
                       left.detail)
                < std::tie(
                       right.modelid,
                       right.instanceid,
                       right.code,
                       right.detail);
        });
}

std::string MapSceneValidationCode(const SceneValidationErrorCode code)
{
    if (code == SceneValidationErrorCode::SceneRevisionStale)
    {
        return kStaleCode;
    }
    if (code == SceneValidationErrorCode::ResourceScopeEscape
        || code == SceneValidationErrorCode::ResourceScopeMissing)
    {
        return kInputMissingCode;
    }
    if (code == SceneValidationErrorCode::SceneProfileMismatch)
    {
        return kProfileCode;
    }
    if (code == SceneValidationErrorCode::EffectiveConfigCancelled)
    {
        return kCancelledCode;
    }
    return kInputInvalidCode;
}

bool IsModeAdmitted(
    const SceneFullPreflightInstanceResult& instance,
    const ModelPreflightPipelineMode mode)
{
    const ModeAdmissionResult& admission =
        mode == ModelPreflightPipelineMode::Legacy
        ? instance.legacyadmission
        : instance.globaladmission;
    return admission.status != ModelPreflightAdmissionStatus::Blocked;
}

void AppendModelIssues(
    SceneFullPreflightInstanceResult& target,
    const ModelPreflightResult& source,
    const std::string& phase)
{
    for (const ModelPreflightIssue& issue : source.issues)
    {
        target.issues.push_back(MakeIssue(
            issue.code,
            issue.severity,
            phase,
            target.modelid,
            target.instanceid,
            Json::object({{"phase", phase}, {"evidence", issue.context}}),
            issue.count));
    }
}

bool HasIncompleteAudit(const TransformedModelPreflightExecution& execution)
{
    return !execution.IsValid()
        || !execution.source.full_audit.available
        || !execution.transformed.full_audit.available
        || !execution.source.full_audit.self_intersection.complete
        || !execution.transformed.full_audit.self_intersection.complete;
}

SceneViewAdmissionStatus ToViewAdmission(const bool admitted)
{
    return admitted
        ? SceneViewAdmissionStatus::Admitted
        : SceneViewAdmissionStatus::Blocked;
}

std::vector<const SceneModelInstance*> OrderInstances(
    const MultiModelScene& scene)
{
    std::vector<const SceneModelInstance*> ordered;
    ordered.reserve(scene.instances.size());
    for (const SceneModelInstance& instance : scene.instances)
    {
        ordered.push_back(&instance);
    }
    std::sort(
        ordered.begin(), ordered.end(),
        [](const SceneModelInstance* left, const SceneModelInstance* right)
        {
            return std::tie(left->instance.modelid, left->instance.instanceid)
                < std::tie(right->instance.modelid, right->instance.instanceid);
        });
    return ordered;
}

std::map<std::string, const ModelSource*> IndexSources(
    const MultiModelScene& scene)
{
    std::map<std::string, const ModelSource*> sources;
    for (const ModelSource& source : scene.models)
    {
        sources.emplace(source.modelid, &source);
    }
    return sources;
}

}  // namespace

bool SceneFullPreflightResolvedModel::IsValid() const
{
    return model != nullptr
        && errorcode == SceneFullPreflightResolutionErrorCode::None;
}

SceneFullPreflightResult SceneFullPreflightService::Run(
    const SceneFullPreflightRequest& request)
{
    SceneFullPreflightResult result;
    result.targetmode = request.targetmode;
    if (request.scene == nullptr)
    {
        result.sceneissues.push_back(MakeIssue(
            kInputInvalidCode,
            ModelPreflightIssueSeverity::Error,
            "committed scene is missing"));
        return result;
    }

    const MultiModelScene& scene = *request.scene;
    result.sceneid = scene.sceneid;
    result.scenerevision = scene.scenerevision;
    result.scenehash = ComputeMultiModelSceneHash(scene);
    const std::vector<const SceneModelInstance*> instances =
        OrderInstances(scene);
    for (const SceneModelInstance* instance : instances)
    {
        SceneFullPreflightInstanceResult item;
        item.modelid = instance->instance.modelid;
        item.instanceid = instance->instance.instanceid;
        item.transformrevision = instance->instance.transformrevision;
        item.visible = instance->instance.visible;
        item.skippedhidden = !item.visible;
        item.complete = item.skippedhidden;
        item.blocked = item.visible;
        result.skippedinstancecount += item.skippedhidden ? 1U : 0U;
        result.instances.push_back(std::move(item));
    }

    if (IsCancelled(request))
    {
        result.cancelled = true;
        result.sceneissues.push_back(MakeIssue(
            kCancelledCode,
            ModelPreflightIssueSeverity::Error,
            "scene preflight was cancelled before validation"));
        return result;
    }
    if (request.scenehash != result.scenehash
        || request.expectedscenerevision != result.scenerevision)
    {
        result.sceneissues.push_back(MakeIssue(
            kStaleCode,
            ModelPreflightIssueSeverity::Error,
            "scene hash or revision is stale"));
        return result;
    }
    if (!request.modelresolver)
    {
        result.sceneissues.push_back(MakeIssue(
            kInputInvalidCode,
            ModelPreflightIssueSeverity::Error,
            "model resolver is missing"));
        return result;
    }

    const SceneValidationResult validation = ValidateMultiModelScene(
        scene, SceneValidationPurpose::Production);
    for (const SceneValidationError& error : validation.errors)
    {
        result.sceneissues.push_back(MakeIssue(
            MapSceneValidationCode(error.code),
            ModelPreflightIssueSeverity::Error,
            error.message,
            error.modelid,
            error.instanceid,
            Json::object({
                {"field", error.field},
                {"sceneCode", std::string(
                     SceneValidationErrorCodeName(error.code))},
            })));
    }
    if (!validation.IsValid())
    {
        SortIssues(result.sceneissues);
        return result;
    }

    const std::map<std::string, const ModelSource*> sources =
        IndexSources(scene);
    std::set<std::string> visibleModelIds;
    for (const SceneModelInstance* instance : instances)
    {
        if (instance->instance.visible)
        {
            visibleModelIds.insert(instance->instance.modelid);
        }
    }
    std::map<std::string, SceneFullPreflightResolvedModel> models;
    for (const std::string& modelId : visibleModelIds)
    {
        if (IsCancelled(request))
        {
            result.cancelled = true;
            break;
        }
        SceneFullPreflightResolvedModel resolved = ResolveModel(
            request, *sources.at(modelId));
        result.checkedmodelcount += resolved.IsValid() ? 1U : 0U;
        models.emplace(modelId, std::move(resolved));
    }

    SceneCollisionRequest collisionRequest;
    collisionRequest.sceneid = scene.sceneid;
    collisionRequest.currentscenerevision = scene.scenerevision;
    collisionRequest.expectedscenerevision = request.expectedscenerevision;
    collisionRequest.purpose = SceneValidationPurpose::Production;
    collisionRequest.buildvolume = scene.buildvolume;

    for (std::size_t index = 0U; index < instances.size(); ++index)
    {
        const SceneModelInstance& sceneInstance = *instances[index];
        SceneFullPreflightInstanceResult& item = result.instances[index];
        SceneCollisionItem collisionItem;
        collisionItem.instance = sceneInstance.instance;
        if (!item.visible)
        {
            collisionRequest.items.push_back(std::move(collisionItem));
            continue;
        }
        if (result.cancelled || IsCancelled(request))
        {
            result.cancelled = true;
            item.issues.push_back(MakeIssue(
                kCancelledCode,
                ModelPreflightIssueSeverity::Error,
                "scene preflight was cancelled",
                item.modelid,
                item.instanceid));
        }
        else
        {
            const auto resolved = models.find(item.modelid);
            if (resolved == models.end() || !resolved->second.IsValid())
            {
                const bool missing = resolved == models.end()
                    || resolved->second.errorcode
                        == SceneFullPreflightResolutionErrorCode::ResourceMissing;
                const std::string detail = resolved == models.end()
                    ? "visible instance model was not resolved"
                    : resolved->second.detail;
                item.issues.push_back(MakeIssue(
                    missing ? kInputMissingCode : kInputInvalidCode,
                    ModelPreflightIssueSeverity::Error,
                    detail,
                    item.modelid,
                    item.instanceid));
            }
            else
            {
                TransformedModelPreflightRequest preflight;
                preflight.source = resolved->second.model.get();
                preflight.instance = sceneInstance.instance;
                preflight.options = request.options;
                preflight.admissioncontext = request.admissioncontext;
                preflight.sourcehash = sources.at(item.modelid)->sourcehash;
                preflight.resourcehash = sources.at(item.modelid)->resourcehash;
                preflight.sceneid = scene.sceneid;
                preflight.scenerevision = scene.scenerevision;
                preflight.expectedscenerevision = request.expectedscenerevision;
                preflight.expectedtransformrevision = item.transformrevision;
                preflight.generation = item.transformrevision;
                preflight.cancellationrequested = request.cancellationrequested;
                const TransformedModelPreflightExecution execution =
                    m_transformedPreflight.Run(preflight);
                result.cancelled = result.cancelled || execution.cancelled;
                item.transformhash = execution.transformhash;
                item.sourcestatus = execution.source.result.status;
                item.transformedstatus = execution.transformed.result.status;
                item.legacyadmission = execution.transformed.result.legacyAdmission;
                item.globaladmission = execution.transformed.result.globalAdmission;
                if (execution.transformed.full_audit.available)
                {
                    item.topology = execution.transformed.full_audit.diagnostics;
                }
                AppendModelIssues(item, execution.source.result, "source");
                AppendModelIssues(
                    item, execution.transformed.result, "transformed");
                item.complete = !HasIncompleteAudit(execution);
                item.blocked = !item.complete
                    || !IsModeAdmitted(item, request.targetmode);

                SceneViewGeometryRequest geometryRequest;
                geometryRequest.sceneid = scene.sceneid;
                geometryRequest.scenerevision = scene.scenerevision;
                geometryRequest.expectedscenerevision =
                    request.expectedscenerevision;
                geometryRequest.expectedtransformrevision =
                    item.transformrevision;
                geometryRequest.instance = sceneInstance.instance;
                geometryRequest.admissionstatus = ToViewAdmission(!item.blocked);
                geometryRequest.buildsurfacepreview = false;
                SceneViewGeometryResult geometry = BuildSceneViewGeometry(
                    *resolved->second.model, geometryRequest);
                if (!geometry.IsValid())
                {
                    item.complete = false;
                    item.blocked = true;
                    item.issues.push_back(MakeIssue(
                        kInternalCode,
                        ModelPreflightIssueSeverity::Error,
                        geometry.error->message,
                        item.modelid,
                        item.instanceid,
                        Json::object({{"field", geometry.error->field}})));
                }
                else
                {
                    result.checkedinstancecount += item.complete ? 1U : 0U;
                    item.bboxmm = geometry.geometry.effectivebboxmm;
                    collisionItem.instance.effectivebboxmm = item.bboxmm;
                    collisionItem.geometry = std::move(geometry.geometry);
                }
            }
        }
        collisionItem.admissionstatus = item.blocked
            ? SceneInstanceAdmissionStatus::Blocked
            : SceneInstanceAdmissionStatus::Admitted;
        collisionRequest.items.push_back(std::move(collisionItem));
        SortIssues(item.issues);
    }

    bool checksComplete = !result.cancelled;
    for (const SceneFullPreflightInstanceResult& item : result.instances)
    {
        checksComplete = checksComplete && (!item.visible || item.complete);
    }
    if (checksComplete)
    {
        const SceneCollisionResult collision =
            EvaluateSceneCollisionAdmission(collisionRequest);
        result.collisions = collision.collisionpairs;
        for (const SceneCollisionInstanceResult& collisionItem :
             collision.instances)
        {
            const auto found = std::find_if(
                result.instances.begin(),
                result.instances.end(),
                [&collisionItem](const SceneFullPreflightInstanceResult& item)
                {
                    return item.instanceid == collisionItem.instanceid;
                });
            if (found == result.instances.end())
            {
                checksComplete = false;
                continue;
            }
            found->outofbounds = found->visible
                && collisionItem.boundsvalid
                && !collisionItem.inbounds;
            if (found->outofbounds)
            {
                result.outofboundsinstances.push_back(found->instanceid);
            }
            found->blocked = found->blocked || !collisionItem.errors.empty();
            for (const SceneCollisionError& error : collisionItem.errors)
            {
                found->issues.push_back(MakeIssue(
                    std::string(SceneCollisionErrorCodeName(error.code)),
                    ModelPreflightIssueSeverity::Error,
                    error.message,
                    found->modelid,
                    found->instanceid,
                    Json::object({
                        {"field", error.field},
                        {"otherInstanceId", error.otherinstanceid},
                    })));
            }
            SortIssues(found->issues);
        }
        for (const SceneCollisionError& error : collision.errors)
        {
            if (error.instanceid.empty())
            {
                result.sceneissues.push_back(MakeIssue(
                    std::string(SceneCollisionErrorCodeName(error.code)),
                    ModelPreflightIssueSeverity::Error,
                    error.message,
                    error.modelid,
                    error.instanceid,
                    Json::object({{"field", error.field}})));
            }
        }
    }

    for (const SceneFullPreflightInstanceResult& item : result.instances)
    {
        result.blockedinstancecount +=
            item.visible && item.blocked ? 1U : 0U;
    }
    std::sort(
        result.collisions.begin(), result.collisions.end(),
        [](const SceneCollisionPair& left, const SceneCollisionPair& right)
        {
            return std::tie(left.firstinstanceid, left.secondinstanceid)
                < std::tie(right.firstinstanceid, right.secondinstanceid);
        });
    std::sort(
        result.outofboundsinstances.begin(),
        result.outofboundsinstances.end());
    SortIssues(result.sceneissues);
    result.complete = checksComplete;
    result.authoritative = result.complete && !result.cancelled;
    result.productionadmitted = result.authoritative
        && result.blockedinstancecount == 0U;
    return result;
}

void SceneFullPreflightService::ClearCache()
{
    m_transformedPreflight.ClearCache();
}

}  // namespace slicer_core
