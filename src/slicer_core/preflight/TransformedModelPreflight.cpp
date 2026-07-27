#include "slicer_core/preflight/TransformedModelPreflight.h"

#include "slicer_core/geometry/MeshScaleTolerance.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/TransformedModelAdapter.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshRepairPreflight.h"
#include "slicer_core/preflight/ModelPreflightCacheIdentity.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr const char* kAlgorithmVersion =
    "transformed_model_preflight.13a04.1";

bool IsCancelled(const TransformedModelPreflightRequest& request)
{
    return request.cancellationrequested
        && request.cancellationrequested();
}

std::string Number(const double value)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(
        std::numeric_limits<double>::max_digits10)
           << value;
    return stream.str();
}

void AppendField(
    std::string& payload,
    const std::string& name,
    const std::string& value)
{
    payload.append(name);
    payload.push_back(':');
    payload.append(std::to_string(value.size()));
    payload.push_back(':');
    payload.append(value);
    payload.push_back('\n');
}

std::string BuildOptionsHash(const ModelPreflightOptions& options)
{
    std::string payload{
        "slicesoft.transformed_model_preflight.options.1\n"};
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
        std::to_string(
            options.maxCompleteSelfIntersectionCandidatePairs));
    AppendField(
        payload,
        "positionEpsilonMm",
        Number(options.positionEpsilonMm));
    AppendField(
        payload,
        "degenerateAreaEpsilonMm2",
        Number(options.degenerateAreaEpsilonMm2));
    return ComputeMeshRepairSha256(payload);
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
    issue.recommendationKey =
        "preflight." + code + ".recommendation";
    issue.context = context;
    return issue;
}

ModelPreflightIssueSeverity ConvertSeverity(
    const ValidationSeverity severity)
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
    const auto severityRank = [](
                                  const ModelPreflightIssueSeverity severity)
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
        [&](const ModelPreflightIssue& left,
            const ModelPreflightIssue& right)
        {
            return key(left) < key(right);
        });

    std::vector<ModelPreflightIssue> merged;
    for (const ModelPreflightIssue& issue : issues)
    {
        if (!merged.empty() && key(merged.back()) == key(issue))
        {
            merged.back().count += issue.count;
        }
        else
        {
            merged.push_back(issue);
        }
    }
    issues = std::move(merged);
}

void SetLifecycleResult(
    ModelPreflightExecutionResult& execution,
    const ModelPreflightStatus status,
    const ModelPreflightErrorCode code)
{
    execution.result.status = status;
    execution.result.issues.push_back(MakeIssue(
        ModelPreflightErrorCodeName(code),
        "lifecycle",
        ModelPreflightIssueSeverity::Error,
        1U));
}

ModelPreflightExecutionResult LifecycleStage(
    const TransformedModelPreflightRequest& request,
    const ModelPreflightStatus status,
    const ModelPreflightErrorCode code,
    const bool cancelled,
    const bool stale)
{
    ModelPreflightExecutionResult execution;
    execution.generation = request.generation;
    execution.cancelled = cancelled;
    execution.stale = stale;
    SetLifecycleResult(execution, status, code);
    execution.result = EvaluateModelPreflightAdmissions(
        execution.result,
        request.admissioncontext);
    return execution;
}

SceneModel BuildDiagnosticModel(
    const SceneModel& source,
    const TransformedModelGeometry& geometry)
{
    SceneModel transformed = source;
    transformed.triangles = geometry.triangles;
    transformed.triangle_textures = geometry.triangletextures;
    transformed.bbox_mm = geometry.bboxmm;
    transformed.triangle_count = transformed.triangles.size();
    return transformed;
}

}  // namespace

bool TransformedModelPreflightExecution::IsValid() const
{
    return !cancelled
        && !stale
        && source.fastComplete
        && source.fullComplete
        && transformed.fastComplete
        && transformed.fullComplete;
}

TransformedModelPreflightExecution
TransformedModelPreflightService::Run(
    const TransformedModelPreflightRequest& request)
{
    TransformedModelPreflightExecution execution;
    execution.generation = request.generation;
    execution.sceneid = request.sceneid;
    execution.instanceid = request.instance.instanceid;
    execution.scenerevision = request.scenerevision;
    execution.transformrevision =
        request.instance.transformrevision;

    if (IsCancelled(request))
    {
        execution.cancelled = true;
        execution.source = LifecycleStage(
            request,
            ModelPreflightStatus::Cancelled,
            ModelPreflightErrorCode::Cancelled,
            true,
            false);
        execution.transformed = execution.source;
        return execution;
    }
    if (request.source == nullptr
        || request.sceneid.empty()
        || request.sourcehash.empty()
        || request.resourcehash.empty())
    {
        execution.source = LifecycleStage(
            request,
            ModelPreflightStatus::Blocked,
            ModelPreflightErrorCode::ImportInvalid,
            false,
            false);
        execution.transformed = execution.source;
        return execution;
    }
    if (request.expectedscenerevision != request.scenerevision
        || request.expectedtransformrevision
            != request.instance.transformrevision)
    {
        execution.stale = true;
        execution.source = LifecycleStage(
            request,
            ModelPreflightStatus::Stale,
            ModelPreflightErrorCode::Stale,
            false,
            true);
        execution.transformed = execution.source;
        return execution;
    }

    ModelInstance sourceInstance = request.instance;
    sourceInstance.transform = ModelTransform{};
    sourceInstance.transformrevision = 0U;
    execution.source = RunGeometry(request, sourceInstance);
    if (IsCancelled(request))
    {
        execution.cancelled = true;
        execution.transformed = LifecycleStage(
            request,
            ModelPreflightStatus::Cancelled,
            ModelPreflightErrorCode::Cancelled,
            true,
            false);
        return execution;
    }

    const ModelTransformHashResult transformHash =
        ComputeModelTransformHash(
            request.instance.transform,
            request.instance.sourcetransformidentity,
            request.instance.instanceid,
            request.instance.modelid);
    if (transformHash.IsValid())
    {
        execution.transformhash = transformHash.hash;
    }
    execution.transformed =
        RunGeometry(request, request.instance);
    execution.cancelled = execution.transformed.cancelled;
    execution.stale = execution.transformed.stale;
    return execution;
}

void TransformedModelPreflightService::ClearCache()
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache.clear();
}

ModelPreflightExecutionResult
TransformedModelPreflightService::RunGeometry(
    const TransformedModelPreflightRequest& request,
    const ModelInstance& instance)
{
    ModelPreflightExecutionResult execution;
    execution.generation = request.generation;
    execution.fastComplete = true;
    if (IsCancelled(request))
    {
        return LifecycleStage(
            request,
            ModelPreflightStatus::Cancelled,
            ModelPreflightErrorCode::Cancelled,
            true,
            false);
    }

    const ModelTransformHashResult transformHash =
        ComputeModelTransformHash(
            instance.transform,
            instance.sourcetransformidentity,
            instance.instanceid,
            instance.modelid);
    if (!transformHash.IsValid())
    {
        execution.result.status = ModelPreflightStatus::Blocked;
        execution.result.issues.push_back(MakeIssue(
            ModelPreflightErrorCodeName(
                ModelPreflightErrorCode::ImportInvalid),
            "transform",
            ModelPreflightIssueSeverity::Error,
            1U,
            Json::object(
                {{"detail", transformHash.error->message}})));
        execution.result = EvaluateModelPreflightAdmissions(
            execution.result,
            request.admissioncontext);
        return execution;
    }

    execution.result.identity.sourceHash = request.sourcehash;
    execution.result.identity.resourceHash = request.resourcehash;
    execution.result.identity.transformHash = transformHash.hash;
    execution.result.identity.optionsHash =
        BuildOptionsHash(request.options);
    execution.result.identity.algorithmVersion = kAlgorithmVersion;
    execution.result.cacheKey = ComputeModelPreflightCacheKey(
        execution.result.identity);
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        const auto found = m_cache.find(execution.result.cacheKey);
        if (found != m_cache.end())
        {
            ModelPreflightExecutionResult cached = found->second;
            cached.generation = request.generation;
            cached.cacheHit = true;
            return cached;
        }
    }

    const std::uint64_t missingMaterialResources =
        static_cast<std::uint64_t>(std::count_if(
            request.source->material_infos.begin(),
            request.source->material_infos.end(),
            [](const MaterialInfo& material)
            {
                return material.has_texture
                    && !material.texture_exists;
            }));
    const std::uint64_t missingResources = std::max(
        missingMaterialResources,
        static_cast<std::uint64_t>(std::max(
            0,
            request.source->three_mf.texture_missing_count)));
    if (missingResources > 0U)
    {
        execution.result.issues.push_back(MakeIssue(
            ModelPreflightErrorCodeName(
                ModelPreflightErrorCode::ResourceMissing),
            "resource",
            ModelPreflightIssueSeverity::Error,
            missingResources));
    }

    try
    {
        const TransformedModelResult transformed =
            AdaptTransformedModel(*request.source, instance);
        if (!transformed.IsValid())
        {
            throw std::runtime_error(
                transformed.error->message);
        }
        const SceneModel diagnosticModel =
            BuildDiagnosticModel(
                *request.source,
                transformed.geometry);
        SceneModelTriangleMeshAdapterOptions adapterOptions;
        adapterOptions.position_epsilon_mm =
            request.options.positionEpsilonMm;
        adapterOptions.degenerate_area_epsilon_mm2 =
            request.options.degenerateAreaEpsilonMm2;
        const AdaptedTriangleMesh adapted =
            AdaptSceneModelToTriangleMesh(
                diagnosticModel,
                adapterOptions);

        MeshRepairPreflightRequest preflightRequest;
        preflightRequest.mesh = &adapted;
        preflightRequest.input.sourcePath =
            diagnosticModel.model_path.generic_string();
        preflightRequest.input.inputFormat = diagnosticModel.format;
        preflightRequest.options.mode = "strict_closed";
        preflightRequest.options.analyzeCompleteSelfIntersections = true;
        preflightRequest.options
            .maxCompleteSelfIntersectionCandidatePairs =
            request.options
                .maxCompleteSelfIntersectionCandidatePairs;
        preflightRequest.robustnessOptions.tolerance =
            MakeMeshScaleTolerance(
                adapted.mesh.bbox_mm,
                request.options.voxelMm);
        preflightRequest.robustnessOptions
            .max_self_intersection_pairs =
            request.options.maxSelfIntersectionPairs;
        preflightRequest.robustnessOptions
            .max_triangle_pair_checks =
            request.options.maxTrianglePairChecks;
        preflightRequest.sourceHash =
            execution.result.identity.sourceHash;

        const MeshRepairResult diagnostics =
            EvaluateMeshRepairPreflight(preflightRequest);
        execution.fullComplete = true;
        execution.full_audit.available = true;
        execution.full_audit.diagnostics = diagnostics.preRepair;
        execution.full_audit.self_intersection =
            diagnostics.completeSelfIntersectionAnalysis;
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
            execution.result.status =
                ModelPreflightStatus::Blocked;
            execution.result.issues.push_back(MakeIssue(
                ModelPreflightErrorCodeName(
                    ModelPreflightErrorCode::AuditIncomplete),
                "audit",
                ModelPreflightIssueSeverity::Error,
                1U,
                Json::object({
                    {"blockerCode",
                     diagnostics.completeSelfIntersectionAnalysis
                         .blockerCode},
                })));
        }
        else
        {
            execution.result.status =
                diagnostics.preRepair.strictPass
                    && execution.result.issues.empty()
                ? ModelPreflightStatus::Passed
                : ModelPreflightStatus::Warning;
        }
        if (missingResources > 0U)
        {
            execution.result.status =
                ModelPreflightStatus::Blocked;
        }
        SortAndMergeIssues(execution.result.issues);
    }
    catch (const std::exception& error)
    {
        execution.fullComplete = true;
        execution.result.status = ModelPreflightStatus::Blocked;
        execution.result.issues.push_back(MakeIssue(
            ModelPreflightErrorCodeName(
                ModelPreflightErrorCode::AuditIncomplete),
            "audit",
            ModelPreflightIssueSeverity::Error,
            1U,
            Json::object({{"detail", error.what()}})));
    }

    if (IsCancelled(request))
    {
        execution.cancelled = true;
        execution.result.status =
            ModelPreflightStatus::Cancelled;
        execution.result.issues.push_back(MakeIssue(
            ModelPreflightErrorCodeName(
                ModelPreflightErrorCode::Cancelled),
            "lifecycle",
            ModelPreflightIssueSeverity::Error,
            1U));
        SortAndMergeIssues(execution.result.issues);
    }
    execution.result = EvaluateModelPreflightAdmissions(
        execution.result,
        request.admissioncontext);
    if (!execution.cancelled
        && (execution.result.status
                == ModelPreflightStatus::Passed
            || execution.result.status
                == ModelPreflightStatus::Warning))
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_cache[execution.result.cacheKey] = execution;
    }
    return execution;
}

}  // namespace slicer_core
