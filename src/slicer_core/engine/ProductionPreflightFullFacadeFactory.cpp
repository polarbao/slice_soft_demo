#include "slicer_core/engine/ProductionPreflightFullFacadeFactory.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/config.h"
#include "slicer_core/model.h"
#include "slicer_core/preflight/SceneFullPreflightService.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <algorithm>
#include <exception>
#include <fstream>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

// 文件职责：把生产导入规则和完整场景预检服务组装为权威 PreflightFullFacade；
// 边界：场景/Profile 标识不一致时失败即拒绝，不进入几何审计。
namespace slicer_core::engine
{
namespace
{

constexpr const char* kCancelledCode{"PM-SLICER-CANCELLED-0070"};
constexpr const char* kInputCode{"PM-SLICER-INPUT-0001"};
constexpr const char* kInvalidInputCode{"PM-SLICER-INPUT-0002"};
constexpr const char* kInternalCode{"PM-SLICER-INTERNAL-0099"};
constexpr const char* kLayoutCode{"PM-SLICER-LAYOUT-0022"};
constexpr const char* kProfileCode{"PM-SLICER-PROFILE-0030"};
constexpr const char* kProfileMismatchCode{"PM-SLICER-PROFILE-0031"};

api::ApiError MakeError(
    std::string code,
    std::string message,
    std::string detail = {})
{
    api::ApiError error;
    error.code = std::move(code);
    error.message = std::move(message);
    error.detail = std::move(detail);
    return error;
}

Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to open JSON document: " + path.generic_string());
    }
    return Json::parse(input);
}

std::optional<ModelPreflightPipelineMode> ParseTargetMode(
    const std::string& value)
{
    if (value == "legacy")
    {
        return ModelPreflightPipelineMode::Legacy;
    }
    if (value == "global_surface_shell")
    {
        return ModelPreflightPipelineMode::GlobalSurfaceShell;
    }
    return std::nullopt;
}

api::PreflightIssue MapIssue(const SceneFullPreflightIssue& issue)
{
    api::PreflightIssue mapped;
    mapped.code = issue.code;
    mapped.severity = ModelPreflightIssueSeverityName(issue.severity);
    mapped.count = issue.count;
    mapped.detail = issue.detail;
    mapped.model_id = issue.modelid;
    mapped.instance_id = issue.instanceid;
    mapped.context = issue.context;
    return mapped;
}

api::PreflightTopology MapTopology(
    const MeshRepairDiagnosticsSummary& topology)
{
    api::PreflightTopology mapped;
    mapped.available = topology.available;
    mapped.strict_pass = topology.strictPass;
    mapped.boundary_edges = topology.boundaryEdges;
    mapped.non_manifold_edges = topology.nonManifoldEdges;
    mapped.duplicate_faces = topology.duplicateFaces;
    mapped.opposite_duplicate_faces = topology.oppositeDuplicateFaces;
    mapped.local_winding_issues = topology.localWindingIssues;
    mapped.degenerate_triangles = topology.degenerateTriangles;
    mapped.connected_components = topology.connectedComponents;
    mapped.self_intersection_pairs =
        topology.confirmedSelfIntersectionPairs;
    return mapped;
}

api::PreflightBounds MapBounds(const BoundingBox& bounds)
{
    api::PreflightBounds mapped;
    mapped.valid = true;
    mapped.min_mm = {bounds.min.x, bounds.min.y, bounds.min.z};
    mapped.max_mm = {bounds.max.x, bounds.max.y, bounds.max.z};
    return mapped;
}

void MergeTopology(
    api::PreflightTopology& aggregate,
    const api::PreflightTopology& item)
{
    if (!item.available)
    {
        return;
    }
    aggregate.strict_pass = aggregate.available
        ? aggregate.strict_pass && item.strict_pass
        : item.strict_pass;
    aggregate.available = true;
    aggregate.boundary_edges += item.boundary_edges;
    aggregate.non_manifold_edges += item.non_manifold_edges;
    aggregate.duplicate_faces += item.duplicate_faces;
    aggregate.opposite_duplicate_faces += item.opposite_duplicate_faces;
    aggregate.local_winding_issues += item.local_winding_issues;
    aggregate.degenerate_triangles += item.degenerate_triangles;
    aggregate.connected_components += item.connected_components;
    aggregate.self_intersection_pairs += item.self_intersection_pairs;
}

void MergeBounds(
    api::PreflightBounds& aggregate,
    const api::PreflightBounds& item)
{
    if (!item.valid)
    {
        return;
    }
    if (!aggregate.valid)
    {
        aggregate = item;
        return;
    }
    for (std::size_t axis{0U}; axis < 3U; ++axis)
    {
        aggregate.min_mm[axis] = std::min(
            aggregate.min_mm[axis], item.min_mm[axis]);
        aggregate.max_mm[axis] = std::max(
            aggregate.max_mm[axis], item.max_mm[axis]);
    }
}

api::PreflightResult MapResult(
    const SceneFullPreflightResult& source)
{
    api::PreflightResult result;
    result.admitted = source.productionadmitted;
    result.authoritative = source.authoritative;
    result.complete = source.complete;
    result.cancelled = source.cancelled;
    result.scene_id = source.sceneid;
    result.scene_revision = source.scenerevision;
    result.scene_hash = source.scenehash;
    result.target_mode = ModelPreflightPipelineModeName(source.targetmode);
    result.checked_model_count = source.checkedmodelcount;
    result.checked_instance_count = source.checkedinstancecount;
    result.blocked_instance_count = source.blockedinstancecount;
    result.skipped_instance_count = source.skippedinstancecount;
    result.out_of_bounds_instances = source.outofboundsinstances;
    result.out_of_bounds = !source.outofboundsinstances.empty();

    std::set<std::string> issueCodes;
    for (const SceneFullPreflightIssue& issue : source.sceneissues)
    {
        result.issues.push_back(MapIssue(issue));
        issueCodes.insert(issue.code);
    }
    for (const SceneFullPreflightInstanceResult& sourceItem : source.instances)
    {
        api::PreflightInstanceResult item;
        item.model_id = sourceItem.modelid;
        item.instance_id = sourceItem.instanceid;
        item.transform_revision = sourceItem.transformrevision;
        item.transform_hash = sourceItem.transformhash;
        item.visible = sourceItem.visible;
        item.skipped_hidden = sourceItem.skippedhidden;
        item.complete = sourceItem.complete;
        item.blocked = sourceItem.blocked;
        item.out_of_bounds = sourceItem.outofbounds;
        item.source_status = ModelPreflightStatusName(sourceItem.sourcestatus);
        item.transformed_status =
            ModelPreflightStatusName(sourceItem.transformedstatus);
        item.legacy_admission = ModelPreflightAdmissionStatusName(
            sourceItem.legacyadmission.status);
        item.global_admission = ModelPreflightAdmissionStatusName(
            sourceItem.globaladmission.status);
        item.topology = MapTopology(sourceItem.topology);
        if (sourceItem.visible && sourceItem.complete)
        {
            item.bbox_mm = MapBounds(sourceItem.bboxmm);
        }
        MergeTopology(result.topology, item.topology);
        MergeBounds(result.bbox_mm, item.bbox_mm);
        for (const SceneFullPreflightIssue& issue : sourceItem.issues)
        {
            api::PreflightIssue mapped = MapIssue(issue);
            item.issues.push_back(mapped);
            result.issues.push_back(std::move(mapped));
            issueCodes.insert(issue.code);
        }
        result.instances.push_back(std::move(item));
    }
    for (const SceneCollisionPair& collision : source.collisions)
    {
        result.collisions.push_back({
            collision.firstinstanceid,
            collision.secondinstanceid});
    }
    result.issue_codes.assign(issueCodes.begin(), issueCodes.end());
    return result;
}

bool HasResourceFailure(const api::PreflightResult& result)
{
    return std::any_of(
        result.issues.begin(), result.issues.end(),
        [](const api::PreflightIssue& issue)
        {
            return issue.code == kInputCode
                || issue.code == kInvalidInputCode;
        });
}

class ProductionPreflightFullFacade final : public api::PreflightFullFacade
{
public:
    api::ApiResult<api::PreflightResult> RunFull(
        const api::PreflightRequest& request,
        const api::ICancelToken& cancelToken) const noexcept override
    {
        try
        {
            if (!request.authoritative)
            {
                return api::ApiResult<api::PreflightResult>::Failure(
                    MakeError(
                        kInvalidInputCode,
                        "full preflight requires authoritative=true"));
            }
            if (request.scene_config_path.empty()
                || request.profile_config_path.empty()
                || request.scene_hash.empty()
                || request.profile_hash.empty()
                || request.target_mode.empty())
            {
                return api::ApiResult<api::PreflightResult>::Failure(
                    MakeError(
                        kProfileCode,
                        "full preflight request is missing required identity"));
            }
            if (cancelToken.IsCancelRequested())
            {
                return api::ApiResult<api::PreflightResult>::Failure(
                    MakeError(kCancelledCode, "full preflight was cancelled"));
            }
            const std::optional<ModelPreflightPipelineMode> targetMode =
                ParseTargetMode(request.target_mode);
            if (!targetMode.has_value())
            {
                return api::ApiResult<api::PreflightResult>::Failure(
                    MakeError(
                        kProfileCode,
                        "full preflight targetMode is unsupported",
                        request.target_mode));
            }

            const Json sceneDocument = ReadJson(request.scene_config_path);
            const MultiModelSceneDecodeResult decoded =
                DeserializeMultiModelScene(sceneDocument);
            if (!decoded.IsValid())
            {
                return api::ApiResult<api::PreflightResult>::Failure(
                    MakeError(kInvalidInputCode, "committed scene cannot be decoded"));
            }
            const std::string actualSceneHash =
                ComputeMultiModelSceneHash(decoded.scene);
            if (actualSceneHash != request.scene_hash
                || decoded.scene.scenerevision
                    != request.expected_scene_revision)
            {
                return api::ApiResult<api::PreflightResult>::Failure(
                    MakeError(kLayoutCode, "committed scene identity is stale"));
            }

            const Json profileDocument = ReadJson(request.profile_config_path);
            if (api::ComputeProfileDocumentHash(profileDocument)
                != request.profile_hash)
            {
                return api::ApiResult<api::PreflightResult>::Failure(
                    MakeError(kProfileCode, "effective Profile hash is stale"));
            }
            const SliceConfig profile = load_slice_config(
                request.profile_config_path);
            if (!profile.material_process_profile.enabled
                || profile.material_process_profile.name
                    != decoded.scene.resolvedprofileid)
            {
                return api::ApiResult<api::PreflightResult>::Failure(
                    MakeError(
                        kProfileMismatchCode,
                        "scene and effective Profile identities do not match"));
            }

            SceneFullPreflightRequest serviceRequest;
            serviceRequest.scene = &decoded.scene;
            serviceRequest.scenehash = actualSceneHash;
            serviceRequest.expectedscenerevision =
                request.expected_scene_revision;
            serviceRequest.targetmode = *targetMode;
            serviceRequest.admissioncontext.global_backend_available = true;
            serviceRequest.cancellationrequested = [&cancelToken]()
            {
                return cancelToken.IsCancelRequested();
            };
            serviceRequest.modelresolver = [
                profile,
                profileDirectory = request.profile_config_path.parent_path()](
                    const ModelSource& source)
            {
                try
                {
                    SliceConfig modelConfig = profile;
                    modelConfig.input.model_path = source.sourcepath;
                    modelConfig.input.format = source.format;
                    validate_slice_config(modelConfig);
                    SceneModel model = load_model_report(
                        modelConfig, profileDirectory);
                    return SceneFullPreflightResolvedModel{
                        std::make_shared<const SceneModel>(std::move(model)),
                        SceneFullPreflightResolutionErrorCode::None,
                        {}};
                }
                catch (const std::exception& error)
                {
                    return SceneFullPreflightResolvedModel{
                        {},
                        SceneFullPreflightResolutionErrorCode::ImportInvalid,
                        error.what()};
                }
            };

            SceneFullPreflightService service;
            api::PreflightResult result = MapResult(service.Run(serviceRequest));
            if (result.cancelled || cancelToken.IsCancelRequested())
            {
                return api::ApiResult<api::PreflightResult>::Failure(
                    MakeError(kCancelledCode, "full preflight was cancelled"));
            }
            if (!result.authoritative && HasResourceFailure(result))
            {
                const api::PreflightIssue& issue = *std::find_if(
                    result.issues.begin(), result.issues.end(),
                    [](const api::PreflightIssue& item)
                    {
                        return item.code == kInputCode
                            || item.code == kInvalidInputCode;
                    });
                return api::ApiResult<api::PreflightResult>::Failure(
                    MakeError(issue.code, "full preflight resource resolution failed", issue.detail));
            }
            if (ComputeMultiModelSceneHash(decoded.scene) != request.scene_hash
                || api::ComputeProfileDocumentHash(ReadJson(
                       request.profile_config_path)) != request.profile_hash)
            {
                return api::ApiResult<api::PreflightResult>::Failure(
                    MakeError(kLayoutCode, "preflight input identity changed during execution"));
            }
            return api::ApiResult<api::PreflightResult>::Success(
                std::move(result));
        }
        catch (const std::bad_alloc&)
        {
            return api::ApiResult<api::PreflightResult>::Failure(
                MakeError(
                    "PM-SLICER-RESOURCE-0040",
                    "insufficient memory while running full preflight"));
        }
        catch (const std::exception& error)
        {
            return api::ApiResult<api::PreflightResult>::Failure(
                MakeError(
                    kInternalCode,
                    "unexpected full preflight failure",
                    error.what()));
        }
        catch (...)
        {
            return api::ApiResult<api::PreflightResult>::Failure(
                MakeError(kInternalCode, "unknown full preflight failure"));
        }
    }
};

}  // namespace

std::unique_ptr<api::PreflightFullFacade>
CreateProductionPreflightFullFacade()
{
    return std::make_unique<ProductionPreflightFullFacade>();
}

}  // namespace slicer_core::engine
