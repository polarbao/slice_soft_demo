#include "slicer_worker/preflight/WorkerPreflightExecutor.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/engine/ProductionPreflightFullFacadeFactory.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace slicesoft::worker
{
namespace
{

constexpr const char* kCancelledCode{"PM-SLICER-CANCELLED-0070"};
constexpr const char* kContractCode{"PM-SLICER-CONTRACT-0060"};
constexpr const char* kLayoutCode{"PM-SLICER-LAYOUT-0022"};
constexpr const char* kOutputCode{"PM-SLICER-OUTPUT-0050"};
constexpr const char* kProfileCode{"PM-SLICER-PROFILE-0030"};
constexpr const char* kProfileMismatchCode{"PM-SLICER-PROFILE-0031"};

class WorkerPreflightInputError final : public std::runtime_error
{
public:
    WorkerPreflightInputError(std::string code, const std::string& message)
        : std::runtime_error(message),
          m_code(std::move(code))
    {
    }

    [[nodiscard]] const std::string& Code() const noexcept
    {
        return m_code;
    }

private:
    std::string m_code;
};

struct MaterializedPreflightInput
{
    std::filesystem::path scenepath;
    std::filesystem::path profilepath;
    std::string scenehash;
    std::string profilehash;
    std::uint64_t scenerevision{0U};
    std::string targetmode;
};

[[noreturn]] void Fail(const std::string& code, const std::string& message)
{
    throw WorkerPreflightInputError(code, message);
}

bool IsLowercaseSha256(const std::string& value)
{
    return value.size() == 71U
        && value.rfind("sha256:", 0U) == 0U
        && std::all_of(
            value.begin() + 7, value.end(),
            [](const char character)
            {
                return (character >= '0' && character <= '9')
                    || (character >= 'a' && character <= 'f');
            });
}

void CheckCancellation(
    const slicer_core::api::ICancelToken& cancelToken)
{
    if (cancelToken.IsCancelRequested())
    {
        Fail(kCancelledCode, "full preflight was cancelled");
    }
}

const slicer_core::Json& ReadObjectField(
    const slicer_core::Json& object,
    const std::string& field,
    const std::string& code)
{
    if (!object.contains(field) || !object.at(field).is_object())
    {
        Fail(code, field + " must be an object");
    }
    return object.at(field);
}

std::string ReadStringField(
    const slicer_core::Json& object,
    const std::string& field,
    const std::string& code)
{
    if (!object.contains(field) || !object.at(field).is_string())
    {
        Fail(code, field + " must be a string");
    }
    const std::string value = object.at(field).as_string();
    if (value.empty())
    {
        Fail(code, field + " must not be empty");
    }
    return value;
}

std::uint64_t ReadRevision(const slicer_core::Json& input)
{
    if (!input.contains("expectedSceneRevision")
        || !input.at("expectedSceneRevision").is_number())
    {
        Fail(kLayoutCode, "expectedSceneRevision must be a non-negative integer");
    }
    const double raw = input.at("expectedSceneRevision").as_double();
    const auto revision = static_cast<std::uint64_t>(raw);
    if (raw < 0.0 || static_cast<double>(revision) != raw)
    {
        Fail(kLayoutCode, "expectedSceneRevision must be a non-negative integer");
    }
    return revision;
}

void WriteJsonAtomically(
    const std::filesystem::path& path,
    const slicer_core::Json& document,
    const slicer_core::api::ICancelToken& cancelToken)
{
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::error_code error;
    std::filesystem::remove(temporary, error);
    error.clear();
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            Fail(kOutputCode, "failed to open preflight materialization staging file");
        }
        output << document.dump(2);
        output.flush();
        if (!output)
        {
            output.close();
            std::filesystem::remove(temporary, error);
            Fail(kOutputCode, "failed to write preflight materialization staging file");
        }
    }
    CheckCancellation(cancelToken);
    std::filesystem::rename(temporary, path, error);
    if (error)
    {
        std::filesystem::remove(temporary, error);
        Fail(kOutputCode, "failed to publish preflight materialization atomically");
    }
}

void CleanupMaterialization(
    const std::filesystem::path& scenePath,
    const std::filesystem::path& profilePath) noexcept
{
    std::error_code error;
    for (const std::filesystem::path& path : {scenePath, profilePath})
    {
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::remove(path.string() + ".tmp", error);
        error.clear();
    }
}

MaterializedPreflightInput Materialize(
    const WorkerRequestEnvelope& request,
    const slicer_core::api::ICancelToken& cancelToken)
{
    if (request.Identity().Capability() != "geometry.preflight.full"
        || !request.HasInput())
    {
        Fail(kContractCode, "executor requires geometry.preflight.full input");
    }
    const slicer_core::Json& input = request.Input();
    const std::string mode = ReadStringField(input, "mode", kContractCode);
    if (mode != "full")
    {
        Fail(kContractCode, "geometry.preflight.full requires mode=full");
    }
    const slicer_core::Json& sceneDocument = ReadObjectField(
        input, "scene", kLayoutCode);
    const slicer_core::Json& profileDocument = ReadObjectField(
        input, "profile", kProfileCode);
    const std::string externalSceneHash = ReadStringField(
        input, "sceneHash", kLayoutCode);
    const std::string profileHash = ReadStringField(
        input, "profileHash", kProfileCode);
    const std::uint64_t revision = ReadRevision(input);
    const std::string targetMode = ReadStringField(
        input, "targetMode", kProfileCode);
    if (targetMode != "legacy" && targetMode != "global_surface_shell")
    {
        Fail(kProfileCode, "targetMode must be legacy or global_surface_shell");
    }
    if (!IsLowercaseSha256(externalSceneHash)
        || !IsLowercaseSha256(profileHash))
    {
        Fail(kContractCode, "sceneHash and profileHash must be lowercase sha256 identities");
    }

    const slicer_core::MultiModelSceneDecodeResult decoded =
        slicer_core::DeserializeMultiModelScene(sceneDocument);
    if (!decoded.IsValid())
    {
        Fail(kLayoutCode, "committed scene cannot be decoded");
    }
    const std::string sceneHash =
        slicer_core::ComputeMultiModelSceneHash(decoded.scene);
    if (externalSceneHash != "sha256:" + sceneHash
        || revision != decoded.scene.scenerevision)
    {
        Fail(kLayoutCode, "committed scene hash or revision is stale");
    }
    if (profileHash
        != slicer_core::api::ComputeProfileDocumentHash(profileDocument))
    {
        Fail(kProfileCode, "effective Profile hash is stale");
    }
    if (!profileDocument.contains("materialProcessProfile")
        || !profileDocument.at("materialProcessProfile").is_object()
        || profileDocument.at("materialProcessProfile").value(
               "name", std::string{}) != decoded.scene.resolvedprofileid)
    {
        Fail(kProfileMismatchCode, "scene and effective Profile identities do not match");
    }
    if (input.contains("buildVolume")
        && input.at("buildVolume").dump(0)
            != slicer_core::SerializeMultiModelScene(decoded.scene)
                   .at("buildVolume").dump(0))
    {
        Fail(kLayoutCode, "standalone buildVolume differs from committed scene");
    }

    CheckCancellation(cancelToken);
    MaterializedPreflightInput materialized;
    materialized.scenepath = request.Identity().JobDirectory()
        / "preflight.scene.json";
    materialized.profilepath = request.Identity().JobDirectory()
        / "preflight.profile.json";
    for (const std::filesystem::path& path :
         {materialized.scenepath, materialized.profilepath})
    {
        std::error_code error;
        if (std::filesystem::exists(path, error) || error)
        {
            Fail(kOutputCode, "job directory already contains preflight materialization");
        }
    }
    try
    {
        WriteJsonAtomically(
            materialized.scenepath,
            slicer_core::SerializeMultiModelScene(decoded.scene),
            cancelToken);
        WriteJsonAtomically(
            materialized.profilepath, profileDocument, cancelToken);
    }
    catch (...)
    {
        CleanupMaterialization(
            materialized.scenepath, materialized.profilepath);
        throw;
    }
    materialized.scenehash = sceneHash;
    materialized.profilehash = profileHash;
    materialized.scenerevision = revision;
    materialized.targetmode = targetMode;
    return materialized;
}

slicer_core::Json BoundsToJson(
    const slicer_core::api::PreflightBounds& bounds)
{
    return slicer_core::Json::object({
        {"valid", bounds.valid},
        {"min", slicer_core::Json::array({
            bounds.min_mm[0], bounds.min_mm[1], bounds.min_mm[2]})},
        {"max", slicer_core::Json::array({
            bounds.max_mm[0], bounds.max_mm[1], bounds.max_mm[2]})},
    });
}

slicer_core::Json TopologyToJson(
    const slicer_core::api::PreflightTopology& topology)
{
    return slicer_core::Json::object({
        {"available", topology.available},
        {"strictPass", topology.strict_pass},
        {"boundaryEdges", topology.boundary_edges},
        {"nonManifoldEdges", topology.non_manifold_edges},
        {"duplicateFaces", topology.duplicate_faces},
        {"oppositeDuplicateFaces", topology.opposite_duplicate_faces},
        {"localWindingIssues", topology.local_winding_issues},
        {"degenerateTriangles", topology.degenerate_triangles},
        {"connectedComponents", topology.connected_components},
        {"selfIntersectionPairs", topology.self_intersection_pairs},
        {"isClosed", topology.available
            && topology.boundary_edges == 0U
            && topology.non_manifold_edges == 0U},
    });
}

slicer_core::Json IssueToJson(
    const slicer_core::api::PreflightIssue& issue)
{
    slicer_core::Json::Object value{
        {"code", issue.code},
        {"severity", issue.severity},
        {"count", issue.count},
        {"detail", issue.detail},
        {"context", issue.context}};
    if (!issue.model_id.empty())
    {
        value.emplace("modelId", issue.model_id);
    }
    if (!issue.instance_id.empty())
    {
        value.emplace("instanceId", issue.instance_id);
    }
    return slicer_core::Json(std::move(value));
}

slicer_core::Json IssuesToJson(
    const std::vector<slicer_core::api::PreflightIssue>& issues)
{
    slicer_core::Json::Array values;
    values.reserve(issues.size());
    for (const slicer_core::api::PreflightIssue& issue : issues)
    {
        values.push_back(IssueToJson(issue));
    }
    return slicer_core::Json(std::move(values));
}

slicer_core::Json ResultToJson(
    const slicer_core::api::PreflightResult& result)
{
    slicer_core::Json::Array instances;
    instances.reserve(result.instances.size());
    for (const slicer_core::api::PreflightInstanceResult& item : result.instances)
    {
        instances.push_back(slicer_core::Json::object({
            {"modelId", item.model_id},
            {"instanceId", item.instance_id},
            {"transformRevision", item.transform_revision},
            {"transformHash", item.transform_hash},
            {"visible", item.visible},
            {"skippedHidden", item.skipped_hidden},
            {"complete", item.complete},
            {"blocked", item.blocked},
            {"outOfBounds", item.out_of_bounds},
            {"sourceStatus", item.source_status},
            {"transformedStatus", item.transformed_status},
            {"legacyAdmission", item.legacy_admission},
            {"globalAdmission", item.global_admission},
            {"topology", TopologyToJson(item.topology)},
            {"bboxMm", BoundsToJson(item.bbox_mm)},
            {"issues", IssuesToJson(item.issues)},
        }));
    }
    slicer_core::Json::Array collisions;
    for (const slicer_core::api::PreflightCollision& item : result.collisions)
    {
        collisions.push_back(slicer_core::Json::object({
            {"firstInstanceId", item.first_instance_id},
            {"secondInstanceId", item.second_instance_id},
        }));
    }
    slicer_core::Json::Array outOfBounds;
    for (const std::string& instanceId : result.out_of_bounds_instances)
    {
        outOfBounds.emplace_back(instanceId);
    }
    return slicer_core::Json::object({
        {"admission", result.admitted ? "passed" : "blocked"},
        {"issues", IssuesToJson(result.issues)},
        {"topology", TopologyToJson(result.topology)},
        {"bboxMm", BoundsToJson(result.bbox_mm)},
        {"outOfBounds", result.out_of_bounds},
        {"sceneId", result.scene_id},
        {"sceneRevision", result.scene_revision},
        {"sceneHash", "sha256:" + result.scene_hash},
        {"targetMode", result.target_mode},
        {"authoritative", result.authoritative},
        {"complete", result.complete},
        {"cancelled", result.cancelled},
        {"checkedModelCount", static_cast<std::uint64_t>(result.checked_model_count)},
        {"checkedInstanceCount", static_cast<std::uint64_t>(result.checked_instance_count)},
        {"blockedInstanceCount", static_cast<std::uint64_t>(result.blocked_instance_count)},
        {"skippedInstanceCount", static_cast<std::uint64_t>(result.skipped_instance_count)},
        {"instances", slicer_core::Json(std::move(instances))},
        {"collisions", slicer_core::Json(std::move(collisions))},
        {"outOfBoundsInstances", slicer_core::Json(std::move(outOfBounds))},
    });
}

}  // namespace

WorkerPreflightExecutor::WorkerPreflightExecutor(
    std::unique_ptr<slicer_core::api::PreflightFullFacade> facade)
    : m_facade(std::move(facade))
{
    if (m_facade == nullptr)
    {
        throw std::invalid_argument("Worker preflight executor requires a facade");
    }
}

WorkerCapabilityExecutionResult WorkerPreflightExecutor::Execute(
    const WorkerRequestEnvelope& request,
    const slicer_core::api::ICancelToken& cancelToken)
{
    std::optional<MaterializedPreflightInput> materialized;
    try
    {
        materialized = Materialize(request, cancelToken);
        slicer_core::api::PreflightRequest facadeRequest;
        facadeRequest.scene_config_path = materialized->scenepath;
        facadeRequest.profile_config_path = materialized->profilepath;
        facadeRequest.scene_hash = materialized->scenehash;
        facadeRequest.profile_hash = materialized->profilehash;
        facadeRequest.expected_scene_revision = materialized->scenerevision;
        facadeRequest.target_mode = materialized->targetmode;
        facadeRequest.authoritative = true;
        const slicer_core::api::ApiResult<slicer_core::api::PreflightResult>
            response = m_facade->RunFull(facadeRequest, cancelToken);
        if (!response.IsOk())
        {
            const slicer_core::api::ApiError* error = response.Error();
            const std::string code = error != nullptr
                ? error->code
                : "PM-SLICER-INTERNAL-0099";
            if (code == kCancelledCode)
            {
                CleanupMaterialization(
                    materialized->scenepath, materialized->profilepath);
                return WorkerCapabilityExecutionResult::Failure(
                    code,
                    error != nullptr ? error->message : "full preflight was cancelled",
                    error != nullptr && !error->detail.empty()
                        ? std::optional<std::string>(error->detail)
                        : std::nullopt,
                    WorkerResultCleanup{true, false});
            }
            return WorkerCapabilityExecutionResult::Failure(
                code,
                error != nullptr
                    ? error->message
                    : "full preflight failed without an error",
                error != nullptr && !error->detail.empty()
                    ? std::optional<std::string>(error->detail)
                    : std::nullopt);
        }
        if (response.Value() == nullptr)
        {
            return WorkerCapabilityExecutionResult::Failure(
                "PM-SLICER-INTERNAL-0099",
                "full preflight returned no result");
        }
        return WorkerCapabilityExecutionResult::Success(
            ResultToJson(*response.Value()));
    }
    catch (const WorkerPreflightInputError& error)
    {
        if (materialized.has_value())
        {
            CleanupMaterialization(
                materialized->scenepath, materialized->profilepath);
        }
        if (error.Code() == kCancelledCode)
        {
            return WorkerCapabilityExecutionResult::Failure(
                error.Code(), error.what(), std::nullopt,
                WorkerResultCleanup{true, false});
        }
        return WorkerCapabilityExecutionResult::Failure(
            error.Code(), error.what());
    }
    catch (const std::exception& error)
    {
        if (materialized.has_value())
        {
            CleanupMaterialization(
                materialized->scenepath, materialized->profilepath);
        }
        return WorkerCapabilityExecutionResult::Failure(
            "PM-SLICER-INTERNAL-0099",
            "unexpected full preflight executor failure",
            std::string(error.what()));
    }
}

std::unique_ptr<IWorkerCapabilityExecutor>
CreateProductionWorkerPreflightExecutor()
{
    return std::make_unique<WorkerPreflightExecutor>(
        slicer_core::engine::CreateProductionPreflightFullFacade());
}

}  // namespace slicesoft::worker
