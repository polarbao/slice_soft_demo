#include "slicer_core/geometry/repair/MeshRepairPreflight.h"

#include "slicer_core/geometry/MeshTopologyDiagnostics.h"
#include "slicer_core/geometry/repair/MeshRepairEligibilityPolicy.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <string>

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
    const MeshRobustnessReport robustness = AnalyzeMeshRobustness(
        request.mesh->mesh,
        request.robustnessOptions);
    const MeshRepairEligibilityEvidence evidence = BuildEligibilityEvidence(*request.mesh);
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
