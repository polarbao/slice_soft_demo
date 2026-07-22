#include "slicer_core/preflight/ModelPreflightService.h"

#include "slicer_core/config.h"
#include "slicer_core/geometry/MeshScaleTolerance.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshRepairPreflight.h"
#include "slicer_core/model.h"
#include "slicer_core/preflight/ModelPreflightCacheIdentity.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

constexpr const char* kAlgorithmVersion = "model_preflight_service.1";

struct ResourceEntry
{
    std::string key;
    std::filesystem::path path;
};

struct ResourceSnapshot
{
    std::string hash;
    std::uint64_t missingCount{0U};
};

std::string ReadBinaryFile(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        throw std::runtime_error("failed to read file: " + path.string());
    }
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

std::string HashFile(const std::filesystem::path& path)
{
    return ComputeMeshRepairSha256(ReadBinaryFile(path));
}

void AppendField(std::string& payload, const std::string& name, const std::string& value)
{
    payload.append(name);
    payload.push_back(':');
    payload.append(std::to_string(value.size()));
    payload.push_back(':');
    payload.append(value);
    payload.push_back('\n');
}

std::string Number(const double value)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return stream.str();
}

template <std::size_t Size>
void AppendArray(
    std::string& payload,
    const std::string& name,
    const std::array<double, Size>& values)
{
    for (std::size_t index{0U}; index < values.size(); ++index)
    {
        AppendField(payload, name + std::to_string(index), Number(values.at(index)));
    }
}

std::filesystem::path ConfigDirectory(const std::filesystem::path& configPath)
{
    if (configPath.parent_path().empty())
    {
        return std::filesystem::current_path();
    }
    return std::filesystem::absolute(configPath.parent_path()).lexically_normal();
}

std::filesystem::path ResolveModelPath(
    const SliceConfig& config,
    const std::filesystem::path& configDirectory)
{
    if (config.input.model_path.is_absolute())
    {
        return config.input.model_path.lexically_normal();
    }
    return (configDirectory / config.input.model_path).lexically_normal();
}

std::vector<ResourceEntry> CollectResources(const ModelReport& scene)
{
    std::map<std::string, std::filesystem::path> resources;
    for (const std::string& library : scene.material_libraries)
    {
        resources.emplace(
            "mtl:" + std::filesystem::path{library}.generic_string(),
            (scene.model_path.parent_path() / library).lexically_normal());
    }
    for (const MaterialInfo& material : scene.material_infos)
    {
        if (!material.has_texture)
        {
            continue;
        }
        resources.emplace(
            "texture:" + material.texture_source + ":" + material.name,
            material.diffuse_texture_path.lexically_normal());
    }

    std::vector<ResourceEntry> result;
    result.reserve(resources.size());
    for (const auto& [key, path] : resources)
    {
        result.push_back({key, path});
    }
    return result;
}

ResourceSnapshot CaptureResources(const std::vector<ResourceEntry>& resources)
{
    std::string payload{"slicesoft.model_preflight.resources.1\n"};
    ResourceSnapshot snapshot;
    for (const ResourceEntry& resource : resources)
    {
        AppendField(payload, "key", resource.key);
        if (!std::filesystem::exists(resource.path))
        {
            AppendField(payload, "state", "missing");
            ++snapshot.missingCount;
            continue;
        }
        AppendField(payload, "state", "present");
        AppendField(payload, "content", HashFile(resource.path));
    }
    snapshot.hash = ComputeMeshRepairSha256(payload);
    return snapshot;
}

std::uint64_t CountNonFiniteObjVertices(const std::string& content)
{
    std::istringstream input{content};
    std::uint64_t count{0U};
    std::string line;
    while (std::getline(input, line))
    {
        std::istringstream tokens{line};
        std::string kind;
        std::string x;
        std::string y;
        std::string z;
        if (!(tokens >> kind >> x >> y >> z) || kind != "v")
        {
            continue;
        }
        try
        {
            if (!std::isfinite(std::stod(x))
                || !std::isfinite(std::stod(y))
                || !std::isfinite(std::stod(z)))
            {
                ++count;
            }
        }
        catch (const std::out_of_range&)
        {
            ++count;
        }
        catch (const std::invalid_argument&)
        {
            // Malformed numeric syntax remains an importer error.
        }
    }
    return count;
}

std::string BuildTransformHash(const SliceConfig& config, const ModelReport& scene)
{
    std::string payload{"slicesoft.model_preflight.transform.1\n"};
    AppendField(payload, "unit", config.transform.unit);
    AppendArray(payload, "scale", config.transform.scale);
    AppendArray(payload, "rotation", config.transform.rotation_deg);
    AppendArray(payload, "translation", config.transform.translation_mm);
    AppendField(payload, "autoOrientEnabled", config.auto_orient.enabled ? "true" : "false");
    AppendField(payload, "autoOrientMaxHeight", Number(config.auto_orient.max_height_mm));
    AppendField(payload, "autoOrientStrategy", config.auto_orient.strategy);
    AppendField(payload, "selectedOrientation", scene.auto_orient.selected_orientation);
    AppendField(payload, "orientationApplied", scene.auto_orient.applied ? "true" : "false");
    AppendField(payload, "finalMinX", Number(scene.bbox_mm.min.x));
    AppendField(payload, "finalMinY", Number(scene.bbox_mm.min.y));
    AppendField(payload, "finalMinZ", Number(scene.bbox_mm.min.z));
    AppendField(payload, "finalMaxX", Number(scene.bbox_mm.max.x));
    AppendField(payload, "finalMaxY", Number(scene.bbox_mm.max.y));
    AppendField(payload, "finalMaxZ", Number(scene.bbox_mm.max.z));
    return ComputeMeshRepairSha256(payload);
}

std::string BuildOptionsHash(
    const ModelPreflightOptions& options,
    const std::string& missingTexturePolicy)
{
    std::string payload{"slicesoft.model_preflight.options.1\n"};
    AppendField(payload, "voxelMm", Number(options.voxelMm));
    AppendField(
        payload,
        "maxSelfIntersectionPairs",
        std::to_string(options.maxSelfIntersectionPairs));
    AppendField(
        payload,
        "maxTrianglePairChecks",
        std::to_string(options.maxTrianglePairChecks));
    AppendField(
        payload,
        "maxCompleteSelfIntersectionCandidatePairs",
        std::to_string(options.maxCompleteSelfIntersectionCandidatePairs));
    AppendField(payload, "positionEpsilonMm", Number(options.positionEpsilonMm));
    AppendField(
        payload,
        "degenerateAreaEpsilonMm2",
        Number(options.degenerateAreaEpsilonMm2));
    AppendField(payload, "missingTexturePolicy", missingTexturePolicy);
    return ComputeMeshRepairSha256(payload);
}

void SetDeferredAdmissions(ModelPreflightResult& result)
{
    const std::string notRun = ModelPreflightErrorCodeName(
        ModelPreflightErrorCode::NotRun);
    result.legacyAdmission.mode = ModelPreflightPipelineMode::Legacy;
    result.legacyAdmission.status = ModelPreflightAdmissionStatus::Blocked;
    result.legacyAdmission.blockerCodes = {notRun};
    result.legacyAdmission.warningCodes.clear();
    result.globalAdmission.mode = ModelPreflightPipelineMode::GlobalSurfaceShell;
    result.globalAdmission.status = ModelPreflightAdmissionStatus::Blocked;
    result.globalAdmission.blockerCodes = {notRun};
    result.globalAdmission.warningCodes.clear();
}

ModelPreflightIssue MakeIssue(
    const std::string& code,
    const std::string& category,
    const ModelPreflightIssueSeverity severity,
    const std::uint64_t count,
    const Json& context = Json::object({}))
{
    ModelPreflightIssue issue;
    issue.code = code;
    issue.category = category;
    issue.severity = severity;
    issue.count = count;
    issue.summaryKey = "preflight." + code + ".summary";
    issue.recommendationKey = "preflight." + code + ".recommendation";
    issue.context = context;
    return issue;
}

ModelPreflightIssueSeverity ConvertSeverity(const ValidationSeverity severity)
{
    switch (severity)
    {
    case ValidationSeverity::Info:
        return ModelPreflightIssueSeverity::Info;
    case ValidationSeverity::Warning:
        return ModelPreflightIssueSeverity::Warning;
    case ValidationSeverity::Error:
        return ModelPreflightIssueSeverity::Error;
    }
    return ModelPreflightIssueSeverity::Info;
}

std::uint64_t IssueCount(
    const std::string& code,
    const MeshRepairDiagnosticsSummary& diagnostics)
{
    if (code == "MESH_BOUNDARY_EDGES")
    {
        return diagnostics.boundaryEdges;
    }
    if (code == "MESH_NON_MANIFOLD_EDGES")
    {
        return diagnostics.nonManifoldEdges;
    }
    if (code == "MESH_DEGENERATE_TRIANGLES")
    {
        return diagnostics.degenerateTriangles;
    }
    if (code == "MESH_SELF_INTERSECTION_CONFIRMED")
    {
        return diagnostics.confirmedSelfIntersectionPairs;
    }
    return 1U;
}

void SortAndMergeIssues(std::vector<ModelPreflightIssue>& issues)
{
    const auto severityRank = [](const ModelPreflightIssueSeverity severity)
    {
        switch (severity)
        {
        case ModelPreflightIssueSeverity::Error:
            return 0;
        case ModelPreflightIssueSeverity::Warning:
            return 1;
        case ModelPreflightIssueSeverity::Info:
            return 2;
        }
        return 2;
    };
    const auto key = [&](const ModelPreflightIssue& issue)
    {
        return std::tuple{
            severityRank(issue.severity),
            issue.code,
            issue.category,
            issue.context.dump(0)};
    };
    std::sort(
        issues.begin(),
        issues.end(),
        [&](const ModelPreflightIssue& left, const ModelPreflightIssue& right)
        {
            return key(left) < key(right);
        });

    std::vector<ModelPreflightIssue> merged;
    for (const ModelPreflightIssue& issue : issues)
    {
        if (!merged.empty()
            && key(merged.back()) == key(issue))
        {
            merged.back().count += issue.count;
            continue;
        }
        merged.push_back(issue);
    }
    issues = std::move(merged);
}

bool IsFinite(const Vec3& point)
{
    return std::isfinite(point.x)
        && std::isfinite(point.y)
        && std::isfinite(point.z);
}

std::uint64_t CountNonFiniteVertices(const ModelReport& scene)
{
    std::uint64_t count{0U};
    for (const Triangle& triangle : scene.triangles)
    {
        count += IsFinite(triangle.a) ? 0U : 1U;
        count += IsFinite(triangle.b) ? 0U : 1U;
        count += IsFinite(triangle.c) ? 0U : 1U;
    }
    return count;
}

bool IsCancellationRequested(const ModelPreflightRequest& request)
{
    return request.cancellationRequested
        && request.cancellationRequested();
}

ModelPreflightExecutionResult LifecycleResult(
    const ModelPreflightRequest& request,
    const ModelPreflightStatus status,
    const ModelPreflightErrorCode code,
    const bool cancelled,
    const bool stale)
{
    ModelPreflightExecutionResult execution;
    execution.generation = request.generation;
    execution.cancelled = cancelled;
    execution.stale = stale;
    execution.result.status = status;
    SetDeferredAdmissions(execution.result);
    execution.result.issues.push_back(MakeIssue(
        ModelPreflightErrorCodeName(code),
        "lifecycle",
        ModelPreflightIssueSeverity::Error,
        1U));
    return execution;
}

}  // namespace

ModelPreflightExecutionResult ModelPreflightService::Run(
    const ModelPreflightRequest& request)
{
    if (IsCancellationRequested(request))
    {
        return LifecycleResult(
            request,
            ModelPreflightStatus::Cancelled,
            ModelPreflightErrorCode::Cancelled,
            true,
            false);
    }

    ModelPreflightExecutionResult execution;
    execution.generation = request.generation;
    SetDeferredAdmissions(execution.result);

    SliceConfig config;
    ModelReport scene;
    std::filesystem::path sourcePath;
    std::vector<ResourceEntry> resources;
    ResourceSnapshot initialResources;
    try
    {
        config = load_slice_config(request.configPath);
        const std::filesystem::path configDirectory = ConfigDirectory(request.configPath);
        sourcePath = ResolveModelPath(config, configDirectory);
        const std::string sourceContent = ReadBinaryFile(sourcePath);
        execution.result.identity.sourceHash = ComputeMeshRepairSha256(sourceContent);
        execution.result.identity.optionsHash = BuildOptionsHash(
            request.options,
            config.texture.missing_texture_policy);
        execution.result.identity.algorithmVersion = kAlgorithmVersion;
        const bool objInput = config.input.format == "obj"
            || (config.input.format == "auto" && sourcePath.extension() == ".obj");
        const std::uint64_t sourceNonFiniteCount = objInput
            ? CountNonFiniteObjVertices(sourceContent)
            : 0U;
        if (sourceNonFiniteCount > 0U)
        {
            execution.fastComplete = true;
            execution.result.status = ModelPreflightStatus::Blocked;
            execution.result.cacheKey = ComputeModelPreflightCacheKey(
                execution.result.identity);
            execution.result.issues.push_back(MakeIssue(
                ModelPreflightErrorCodeName(
                    ModelPreflightErrorCode::NonFiniteGeometry),
                "geometry",
                ModelPreflightIssueSeverity::Error,
                sourceNonFiniteCount));
            return execution;
        }
        scene = load_model_report(config, configDirectory);
        resources = CollectResources(scene);
        initialResources = CaptureResources(resources);
        execution.result.identity.resourceHash = initialResources.hash;
        execution.result.identity.transformHash = BuildTransformHash(config, scene);
        execution.result.cacheKey = ComputeModelPreflightCacheKey(
            execution.result.identity);
        execution.fastComplete = true;
    }
    catch (const std::exception& error)
    {
        execution.result.status = ModelPreflightStatus::Blocked;
        execution.result.issues.push_back(MakeIssue(
            ModelPreflightErrorCodeName(ModelPreflightErrorCode::ImportInvalid),
            "import",
            ModelPreflightIssueSeverity::Error,
            1U,
            Json::object({{"detail", error.what()}})));
        return execution;
    }

    const std::uint64_t nonFiniteCount = CountNonFiniteVertices(scene);
    if (nonFiniteCount > 0U)
    {
        execution.result.status = ModelPreflightStatus::Blocked;
        execution.result.issues.push_back(MakeIssue(
            ModelPreflightErrorCodeName(ModelPreflightErrorCode::NonFiniteGeometry),
            "geometry",
            ModelPreflightIssueSeverity::Error,
            nonFiniteCount));
        return execution;
    }
    const std::uint64_t missingResourceCount = initialResources.missingCount
        + static_cast<std::uint64_t>(scene.three_mf.texture_missing_count);
    if (missingResourceCount > 0U)
    {
        execution.result.issues.push_back(MakeIssue(
            ModelPreflightErrorCodeName(ModelPreflightErrorCode::ResourceMissing),
            "resource",
            config.texture.missing_texture_policy == "fail_fast"
                ? ModelPreflightIssueSeverity::Error
                : ModelPreflightIssueSeverity::Warning,
            missingResourceCount));
        if (config.texture.missing_texture_policy == "fail_fast")
        {
            execution.result.status = ModelPreflightStatus::Blocked;
            return execution;
        }
    }

    if (IsCancellationRequested(request))
    {
        execution.result.status = ModelPreflightStatus::Cancelled;
        execution.cancelled = true;
        execution.result.issues.push_back(MakeIssue(
            ModelPreflightErrorCodeName(ModelPreflightErrorCode::Cancelled),
            "lifecycle",
            ModelPreflightIssueSeverity::Error,
            1U));
        return execution;
    }

    {
        std::lock_guard<std::mutex> lock{m_cacheMutex};
        const auto found = m_cache.find(execution.result.cacheKey);
        if (found != m_cache.end())
        {
            execution.cacheHit = true;
            execution.fullComplete = true;
            execution.result = found->second;
            return execution;
        }
    }

    try
    {
        SceneModelTriangleMeshAdapterOptions adapterOptions;
        adapterOptions.position_epsilon_mm = request.options.positionEpsilonMm;
        adapterOptions.degenerate_area_epsilon_mm2 =
            request.options.degenerateAreaEpsilonMm2;
        const AdaptedTriangleMesh adapted = AdaptSceneModelToTriangleMesh(
            scene,
            adapterOptions);

        MeshRepairPreflightRequest preflightRequest;
        preflightRequest.mesh = &adapted;
        preflightRequest.input.sourcePath = scene.model_path.generic_string();
        preflightRequest.input.inputFormat = scene.format;
        preflightRequest.options.mode = "strict_closed";
        preflightRequest.options.analyzeCompleteSelfIntersections = true;
        preflightRequest.options.maxCompleteSelfIntersectionCandidatePairs =
            request.options.maxCompleteSelfIntersectionCandidatePairs;
        preflightRequest.robustnessOptions.tolerance = MakeMeshScaleTolerance(
            adapted.mesh.bbox_mm,
            request.options.voxelMm);
        preflightRequest.robustnessOptions.max_self_intersection_pairs =
            request.options.maxSelfIntersectionPairs;
        preflightRequest.robustnessOptions.max_triangle_pair_checks =
            request.options.maxTrianglePairChecks;
        preflightRequest.sourceHash = execution.result.identity.sourceHash;

        const MeshRepairResult diagnostics = EvaluateMeshRepairPreflight(
            preflightRequest);
        execution.fullComplete = true;
        for (const ValidationIssue& issue : diagnostics.issues)
        {
            execution.result.issues.push_back(MakeIssue(
                issue.code,
                "topology",
                ConvertSeverity(issue.severity),
                IssueCount(issue.code, diagnostics.preRepair),
                issue.context));
        }
        if (!diagnostics.completeSelfIntersectionAnalysis.complete)
        {
            execution.result.status = ModelPreflightStatus::Blocked;
            execution.result.issues.push_back(MakeIssue(
                ModelPreflightErrorCodeName(ModelPreflightErrorCode::AuditIncomplete),
                "audit",
                ModelPreflightIssueSeverity::Error,
                1U,
                Json::object({
                    {"blockerCode", diagnostics.completeSelfIntersectionAnalysis.blockerCode},
                })));
        }
        else
        {
            execution.result.status = diagnostics.preRepair.strictPass
                    && execution.result.issues.empty()
                ? ModelPreflightStatus::Passed
                : ModelPreflightStatus::Warning;
        }
        SortAndMergeIssues(execution.result.issues);
    }
    catch (const std::exception& error)
    {
        execution.fullComplete = true;
        execution.result.status = ModelPreflightStatus::Blocked;
        execution.result.issues.push_back(MakeIssue(
            ModelPreflightErrorCodeName(ModelPreflightErrorCode::AuditIncomplete),
            "audit",
            ModelPreflightIssueSeverity::Error,
            1U,
            Json::object({{"detail", error.what()}})));
        return execution;
    }

    if (IsCancellationRequested(request))
    {
        execution.result.status = ModelPreflightStatus::Cancelled;
        execution.cancelled = true;
        execution.result.issues.push_back(MakeIssue(
            ModelPreflightErrorCodeName(ModelPreflightErrorCode::Cancelled),
            "lifecycle",
            ModelPreflightIssueSeverity::Error,
            1U));
        SortAndMergeIssues(execution.result.issues);
        return execution;
    }

    try
    {
        const SliceConfig finalConfig = load_slice_config(request.configPath);
        const ResourceSnapshot finalResources = CaptureResources(resources);
        const std::string finalSourceHash = HashFile(sourcePath);
        const std::string finalTransformHash = BuildTransformHash(finalConfig, scene);
        if (finalSourceHash != execution.result.identity.sourceHash
            || finalResources.hash != execution.result.identity.resourceHash
            || finalTransformHash != execution.result.identity.transformHash
            || BuildOptionsHash(
                   request.options,
                   finalConfig.texture.missing_texture_policy)
                != execution.result.identity.optionsHash)
        {
            execution.result.status = ModelPreflightStatus::Stale;
            execution.stale = true;
            execution.result.issues.push_back(MakeIssue(
                ModelPreflightErrorCodeName(ModelPreflightErrorCode::Stale),
                "lifecycle",
                ModelPreflightIssueSeverity::Error,
                1U));
            SortAndMergeIssues(execution.result.issues);
            return execution;
        }
    }
    catch (const std::exception&)
    {
        execution.result.status = ModelPreflightStatus::Stale;
        execution.stale = true;
        execution.result.issues.push_back(MakeIssue(
            ModelPreflightErrorCodeName(ModelPreflightErrorCode::Stale),
            "lifecycle",
            ModelPreflightIssueSeverity::Error,
            1U));
        SortAndMergeIssues(execution.result.issues);
        return execution;
    }

    if (execution.result.status == ModelPreflightStatus::Passed
        || execution.result.status == ModelPreflightStatus::Warning)
    {
        std::lock_guard<std::mutex> lock{m_cacheMutex};
        m_cache[execution.result.cacheKey] = execution.result;
    }
    return execution;
}

void ModelPreflightService::ClearCache()
{
    std::lock_guard<std::mutex> lock{m_cacheMutex};
    m_cache.clear();
}

std::size_t ModelPreflightService::CacheSize() const
{
    std::lock_guard<std::mutex> lock{m_cacheMutex};
    return m_cache.size();
}

}  // namespace slicer_core
