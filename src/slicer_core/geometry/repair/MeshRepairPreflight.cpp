#include "slicer_core/geometry/repair/MeshRepairPreflight.h"

#include "slicer_core/geometry/MeshTopologyDiagnostics.h"
#include "slicer_core/geometry/repair/MeshCompleteSelfIntersectionAnalyzer.h"
#include "slicer_core/geometry/repair/MeshRepairEligibilityPolicy.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshNonManifoldPatternClassifier.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace slicer_core
{
namespace
{

using Clock = std::chrono::steady_clock;
using FaceKey = std::array<int, 3>;

double ElapsedMilliseconds(const Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

FaceKey MakeFaceKey(const std::array<int, 3>& triangle)
{
    FaceKey key = triangle;
    std::sort(key.begin(), key.end());
    return key;
}

bool FindSameOrientationShift(
    const std::array<int, 3>& reference,
    const std::array<int, 3>& candidate,
    std::size_t& shift)
{
    for (std::size_t candidateShift{0U}; candidateShift < 3U; ++candidateShift)
    {
        bool matches{true};
        for (std::size_t corner{0U}; corner < 3U; ++corner)
        {
            if (candidate.at(corner) != reference.at((corner + candidateShift) % 3U))
            {
                matches = false;
                break;
            }
        }
        if (matches)
        {
            shift = candidateShift;
            return true;
        }
    }
    return false;
}

bool EqualUv(const TexCoord& left, const TexCoord& right)
{
    return left.u == right.u && left.v == right.v;
}

bool AttributesMatch(
    const SurfaceTriangleAttributes& reference,
    const SurfaceTriangleAttributes& candidate,
    const std::size_t candidateShift)
{
    if (reference.has_uv != candidate.has_uv
        || reference.material_name != candidate.material_name)
    {
        return false;
    }
    if (!reference.has_uv)
    {
        return true;
    }
    for (std::size_t corner{0U}; corner < 3U; ++corner)
    {
        if (!EqualUv(
                candidate.uv.at(corner),
                reference.uv.at((corner + candidateShift) % 3U)))
        {
            return false;
        }
    }
    return true;
}

MeshRepairEligibilityEvidence BuildEligibilityEvidence(
    const AdaptedTriangleMesh& mesh)
{
    MeshRepairEligibilityEvidence evidence;
    std::map<FaceKey, std::size_t> firstTriangleByFace;
    for (std::size_t index{0U}; index < mesh.mesh.triangles.size(); ++index)
    {
        const std::array<int, 3>& triangle = mesh.mesh.triangles.at(index);
        const FaceKey key = MakeFaceKey(triangle);
        const auto [found, inserted] = firstTriangleByFace.emplace(key, index);
        if (inserted)
        {
            continue;
        }

        const std::size_t referenceIndex = found->second;
        std::size_t shift{0U};
        if (!FindSameOrientationShift(
                mesh.mesh.triangles.at(referenceIndex),
                triangle,
                shift))
        {
            continue;
        }

        evidence.duplicateFaceAttributesEvaluated = true;
        if (!AttributesMatch(
                mesh.triangle_attributes.at(referenceIndex),
                mesh.triangle_attributes.at(index),
                shift))
        {
            ++evidence.duplicateFaceAttributeConflicts;
        }
    }
    return evidence;
}

void AddTopologyIssues(
    const MeshTopologyReport& topology,
    std::vector<ValidationIssue>& issues)
{
    if (topology.boundary_edges > 0U)
    {
        issues.push_back(MakeValidationIssue(
            "MESH_BOUNDARY_EDGES",
            ValidationSeverity::Error,
            "mesh has boundary edges"));
    }
    if (topology.non_manifold_edges > 0U)
    {
        issues.push_back(MakeValidationIssue(
            "MESH_NON_MANIFOLD_EDGES",
            ValidationSeverity::Error,
            "mesh has non-manifold edges"));
    }
    if (topology.degenerate_triangles > 0U)
    {
        issues.push_back(MakeValidationIssue(
            "MESH_DEGENERATE_TRIANGLES",
            ValidationSeverity::Error,
            "mesh contains rejected degenerate triangles"));
    }
}

MeshRepairDiagnosticsSummary BuildDiagnosticsSummary(
    const MeshTopologyReport& topology,
    const MeshRobustnessReport& robustness,
    const MeshRepairEligibility& eligibility)
{
    MeshRepairDiagnosticsSummary summary;
    summary.available = true;
    summary.strictPass = eligibility.status == MeshRepairStatus::StrictPassNoRepair;
    summary.boundaryEdges = topology.boundary_edges;
    summary.nonManifoldEdges = topology.non_manifold_edges;
    summary.duplicateFaces = robustness.duplicate_faces;
    summary.oppositeDuplicateFaces = robustness.opposite_duplicate_faces;
    summary.localWindingIssues = robustness.inconsistent_oriented_edges;
    summary.degenerateTriangles = topology.degenerate_triangles;
    summary.connectedComponents = robustness.connected_components;
    summary.confirmedSelfIntersectionPairs = std::max(
        robustness.confirmed_self_intersections,
        robustness.self_intersection_pairs);
    AddTopologyIssues(topology, summary.issues);
    summary.issues.insert(
        summary.issues.end(),
        robustness.issues.begin(),
        robustness.issues.end());
    return summary;
}

std::uint64_t CountTextureResources(const AdaptedTriangleMesh& mesh)
{
    std::set<std::string> paths;
    for (const MaterialInfo& material : mesh.material_infos)
    {
        if (material.has_texture)
        {
            paths.insert(material.diffuse_texture_path.generic_string());
        }
    }
    return static_cast<std::uint64_t>(paths.size());
}

void RemoveSelfIntersectionIssues(MeshRobustnessReport& robustness)
{
    robustness.issues.erase(
        std::remove_if(
            robustness.issues.begin(),
            robustness.issues.end(),
            [](const ValidationIssue& issue)
            {
                return issue.code == "MESH_SELF_INTERSECTION_SAMPLED"
                    || issue.code == "MESH_SELF_INTERSECTION_CONFIRMED"
                    || issue.code == "MESH_SELF_INTERSECTION_BUDGET_BLOCKED";
            }),
        robustness.issues.end());
    const auto removeMessage = [](std::vector<std::string>& messages)
    {
        messages.erase(
            std::remove_if(
                messages.begin(),
                messages.end(),
                [](const std::string& message)
                {
                    return message == "self-intersection checks were sampled"
                        || message == "mesh has confirmed self-intersections"
                        || message
                            == "complete self-intersection analysis was budget blocked";
                }),
            messages.end());
    };
    removeMessage(robustness.warnings);
    removeMessage(robustness.errors);
}

void ApplyCompleteSelfIntersectionAnalysis(
    const MeshCompleteSelfIntersectionAnalysis& analysis,
    MeshRobustnessReport& robustness)
{
    RemoveSelfIntersectionIssues(robustness);
    robustness.self_intersection_candidates = static_cast<std::size_t>(
        analysis.candidatePairCount);
    robustness.confirmed_self_intersections = static_cast<std::size_t>(
        analysis.confirmedIntersectionPairs);
    robustness.coplanar_overlap_pairs = static_cast<std::size_t>(
        analysis.coplanarOverlapPairs);
    robustness.touching_only_pairs = static_cast<std::size_t>(
        analysis.touchingOnlyPairs);
    robustness.self_intersection_false_positive_candidates =
        static_cast<std::size_t>(analysis.aabbOnlyPairs);
    robustness.self_intersection_pairs = static_cast<std::size_t>(
        analysis.confirmedIntersectionPairs + analysis.coplanarOverlapPairs);
    robustness.self_intersection_sampled = false;
    robustness.self_intersection_check_sampled = false;
    robustness.self_intersection_check_budget_blocked = !analysis.complete;

    if (!analysis.complete)
    {
        robustness.issues.push_back(MakeValidationIssue(
            "MESH_SELF_INTERSECTION_BUDGET_BLOCKED",
            ValidationSeverity::Warning,
            "complete self-intersection analysis was budget blocked"));
        robustness.warnings.push_back(
            "complete self-intersection analysis was budget blocked");
    }
    else if (robustness.self_intersection_pairs > 0U)
    {
        robustness.issues.push_back(MakeValidationIssue(
            "MESH_SELF_INTERSECTION_CONFIRMED",
            ValidationSeverity::Error,
            "mesh has confirmed self-intersections"));
        robustness.errors.push_back("mesh has confirmed self-intersections");
    }
}

void BuildAdmissionEvidence(MeshRepairResult& result)
{
    result.admission.mode = "repair_then_strict";
    result.admission.status = "non_production_only";
    result.admission.postRepairStrictPass = false;
    result.admission.productionAllowed = false;
    for (const MeshRepairEligibilityDecision& decision : result.eligibility.decisions)
    {
        result.admission.blockerCodes.push_back(decision.issueCode);
        if (!decision.suggestedAction.empty())
        {
            result.admission.suggestedActions.push_back(decision.suggestedAction);
        }
    }
}

}  // namespace

MeshRepairResult EvaluateMeshRepairPreflight(
    const MeshRepairPreflightRequest& request)
{
    if (request.mesh == nullptr)
    {
        throw MeshRepairError(
            MeshRepairErrorCode::InputInvalid,
            "mesh repair preflight requires an adapted mesh");
    }
    if (request.options.mode != "strict_closed")
    {
        throw MeshRepairError(
            MeshRepairErrorCode::InputInvalid,
            "mesh repair preflight only supports strict_closed mode");
    }
    if (request.mesh->triangle_attributes.size() != request.mesh->mesh.triangles.size())
    {
        throw MeshRepairError(
            MeshRepairErrorCode::AttributeMismatch,
            "mesh repair preflight requires one attribute record per triangle");
    }

    const Clock::time_point totalStart = Clock::now();
    MeshRepairResult result;
    result.mode = request.options.mode;
    result.repairEnabled = request.options.enabled;
    result.repairAttempted = false;
    result.productionOutputWritten = false;
    result.input = request.input;
    result.options = request.options;

    const Clock::time_point hashStart = Clock::now();
    result.hashes = ComputeMeshRepairPreHashes(*request.mesh, request.options);
    result.hashes.sourceHash = request.sourceHash;
    result.performance.hashMs = ElapsedMilliseconds(hashStart);

    const Clock::time_point diagnosticsStart = Clock::now();
    MeshTopologyReport topology = AnalyzeMeshTopology(request.mesh->mesh);
    topology.source_triangles = request.mesh->topology.source_triangles;
    topology.degenerate_triangles = request.mesh->topology.degenerate_triangles;
    MeshRobustnessReport robustness = AnalyzeMeshRobustness(
        request.mesh->mesh,
        request.robustnessOptions);
    if (request.options.analyzeCompleteSelfIntersections)
    {
        MeshCompleteSelfIntersectionOptions completeOptions;
        completeOptions.epsilonMm =
            request.robustnessOptions.tolerance.self_intersection_epsilon_mm;
        completeOptions.maxCandidatePairs =
            request.options.maxCompleteSelfIntersectionCandidatePairs;
        result.completeSelfIntersectionAnalysis =
            AnalyzeCompleteMeshSelfIntersections(
                request.mesh->mesh,
                completeOptions);
        ApplyCompleteSelfIntersectionAnalysis(
            result.completeSelfIntersectionAnalysis,
            robustness);
        result.performance.peakWorkingSetBytes =
            result.completeSelfIntersectionAnalysis.peakWorkingSetBytes;
    }
    MeshRepairEligibilityEvidence evidence = BuildEligibilityEvidence(*request.mesh);
    if (request.options.classifyNonManifoldPatterns)
    {
        result.nonManifoldAnalysis = ClassifyMeshNonManifoldPatterns(*request.mesh);
        if (result.nonManifoldAnalysis.nonManifoldEdgeCount > 0U)
        {
            evidence.nonManifoldClassification =
                result.nonManifoldAnalysis.allUniqueFanSplitsFeasible
                ? MeshRepairNonManifoldClassification::UniquelySeparable
                : MeshRepairNonManifoldClassification::Ambiguous;
        }
    }
    result.performance.diagnosticsMs = ElapsedMilliseconds(diagnosticsStart);

    const Clock::time_point eligibilityStart = Clock::now();
    result.eligibility = EvaluateMeshRepairEligibility(topology, robustness, evidence);
    result.performance.eligibilityMs = ElapsedMilliseconds(eligibilityStart);
    result.status = result.eligibility.status;
    result.preRepair = BuildDiagnosticsSummary(topology, robustness, result.eligibility);
    result.issues = result.preRepair.issues;

    result.input.vertexCount = static_cast<std::uint64_t>(request.mesh->mesh.vertices.size());
    result.input.triangleCount = static_cast<std::uint64_t>(request.mesh->mesh.triangles.size());
    result.input.componentCount = static_cast<std::uint64_t>(robustness.connected_components);
    result.input.materialCount = static_cast<std::uint64_t>(request.mesh->material_infos.size());
    result.input.textureResourceCount = CountTextureResources(*request.mesh);
    BuildAdmissionEvidence(result);
    result.performance.totalRepairCoreMs = ElapsedMilliseconds(totalStart);
    return result;
}

}  // namespace slicer_core
