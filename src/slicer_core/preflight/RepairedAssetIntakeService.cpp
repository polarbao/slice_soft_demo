#include "slicer_core/preflight/RepairedAssetIntakeService.h"

#include "slicer_core/config.h"
#include "slicer_core/diagnostics/ModelPreflightReport.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/model.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

constexpr const char* kManifestInvalid = "E_12E_INTAKE_MANIFEST_INVALID";
constexpr const char* kFamilyUnknown = "E_12E_INTAKE_FAMILY_UNKNOWN";
constexpr const char* kFamilyPathMismatch = "E_12E_INTAKE_FAMILY_PATH_MISMATCH";
constexpr const char* kCandidateKindInvalid = "E_12E_INTAKE_CANDIDATE_KIND_INVALID";
constexpr const char* kProvenanceMissing = "E_12E_INTAKE_PROVENANCE_MISSING";
constexpr const char* kOriginalHashMismatch = "E_12E_INTAKE_ORIGINAL_HASH_MISMATCH";
constexpr const char* kCandidateHashMismatch = "E_12E_INTAKE_CANDIDATE_HASH_MISMATCH";
constexpr const char* kCandidateIdentityInvalid = "E_12E_INTAKE_CANDIDATE_IDENTITY_INVALID";
constexpr const char* kResourceMissing = "E_12E_INTAKE_RESOURCE_MISSING";
constexpr const char* kPostStrictFailed = "E_12E_INTAKE_POST_STRICT_FAILED";
constexpr const char* kAttributeMismatch = "E_12E_INTAKE_ATTRIBUTE_MISMATCH";
constexpr const char* kCoordinateMismatch = "E_12E_INTAKE_COORDINATE_MISMATCH";
constexpr const char* kBoundsDeltaExceeded = "E_12E_INTAKE_BOUNDS_DELTA_EXCEEDED";
constexpr const char* kRepeatabilityFailed = "E_12E_INTAKE_REPEATABILITY_FAILED";
constexpr const char* kExecutionFailed = "E_12E_INTAKE_EXECUTION_FAILED";

struct LoadedAsset
{
    RepairedAssetSnapshot snapshot;
    ModelPreflightExecutionResult execution;
};

std::string ToLower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

std::string RequiredFamilyDirectory(const std::string& familyId)
{
    if (familyId == "required_aishen_family")
    {
        return "aishen_fudiao";
    }
    if (familyId == "required_meigui_family")
    {
        return "meigui_fudiao";
    }
    if (familyId == "required_titian_family")
    {
        return "titian_fudiao";
    }
    return {};
}

bool PathContainsDirectory(
    const std::filesystem::path& path,
    const std::string& directory)
{
    const std::string expected = ToLower(directory);
    for (const std::filesystem::path& part : path)
    {
        if (ToLower(part.string()) == expected)
        {
            return true;
        }
    }
    return false;
}

void AddIssue(
    RepairedAssetIntakeResult& result,
    const std::string& code,
    const std::string& message,
    const Json& context = Json::object({}))
{
    result.issues.push_back({code, message, context});
}

bool HasPreflightIssue(
    const ModelPreflightResult& result,
    const std::string& code)
{
    return std::any_of(
        result.issues.begin(),
        result.issues.end(),
        [&](const ModelPreflightIssue& issue)
        {
            return issue.code == code;
        });
}

std::filesystem::path ConfigDirectory(
    const std::filesystem::path& configPath)
{
    return configPath.parent_path().empty()
        ? std::filesystem::current_path()
        : std::filesystem::absolute(configPath.parent_path()).lexically_normal();
}

std::uint64_t FindRoot(
    std::vector<std::uint64_t>& parent,
    const std::uint64_t value)
{
    std::uint64_t current = value;
    while (parent.at(current) != current)
    {
        current = parent.at(current);
    }
    std::uint64_t compressed = value;
    while (parent.at(compressed) != compressed)
    {
        const std::uint64_t next = parent.at(compressed);
        parent.at(compressed) = current;
        compressed = next;
    }
    return current;
}

void UnionVertices(
    std::vector<std::uint64_t>& parent,
    const std::uint64_t left,
    const std::uint64_t right)
{
    const std::uint64_t leftRoot = FindRoot(parent, left);
    const std::uint64_t rightRoot = FindRoot(parent, right);
    if (leftRoot != rightRoot)
    {
        parent.at(rightRoot) = leftRoot;
    }
}

std::uint64_t CountConnectedComponents(const TriangleMeshData& mesh)
{
    if (mesh.triangles.empty())
    {
        return 0U;
    }
    std::vector<std::uint64_t> parent(mesh.vertices.size());
    for (std::size_t index{0U}; index < parent.size(); ++index)
    {
        parent.at(index) = static_cast<std::uint64_t>(index);
    }
    std::vector<bool> used(mesh.vertices.size(), false);
    for (const std::array<int, 3>& triangle : mesh.triangles)
    {
        const std::uint64_t first = static_cast<std::uint64_t>(triangle.at(0));
        const std::uint64_t second = static_cast<std::uint64_t>(triangle.at(1));
        const std::uint64_t third = static_cast<std::uint64_t>(triangle.at(2));
        used.at(first) = true;
        used.at(second) = true;
        used.at(third) = true;
        UnionVertices(parent, first, second);
        UnionVertices(parent, first, third);
    }
    std::set<std::uint64_t> roots;
    for (std::size_t index{0U}; index < used.size(); ++index)
    {
        if (used.at(index))
        {
            roots.insert(FindRoot(parent, static_cast<std::uint64_t>(index)));
        }
    }
    return static_cast<std::uint64_t>(roots.size());
}

std::uint64_t CountTextures(const ModelReport& scene)
{
    return static_cast<std::uint64_t>(std::count_if(
        scene.material_infos.begin(),
        scene.material_infos.end(),
        [](const MaterialInfo& material)
        {
            return material.has_texture;
        }));
}

std::uint64_t CountMissingTextures(const ModelReport& scene)
{
    const std::uint64_t objMissing = static_cast<std::uint64_t>(std::count_if(
        scene.material_infos.begin(),
        scene.material_infos.end(),
        [](const MaterialInfo& material)
        {
            return material.has_texture && !material.texture_exists;
        }));
    return objMissing
        + static_cast<std::uint64_t>(scene.three_mf.texture_missing_count);
}

std::string BuildAuditHash(const ModelPreflightExecutionResult& execution)
{
    std::string payload{"slicesoft.repaired_asset_intake.audit.1\n"};
    payload += execution.fastComplete ? "fast=1\n" : "fast=0\n";
    payload += execution.fullComplete ? "full=1\n" : "full=0\n";
    payload += BuildModelPreflightReport(execution.result).dump(0);
    if (execution.full_audit.available)
    {
        const MeshRepairDiagnosticsSummary& diagnostics =
            execution.full_audit.diagnostics;
        const MeshCompleteSelfIntersectionAnalysis& selfIntersection =
            execution.full_audit.self_intersection;
        payload += "boundary=" + std::to_string(diagnostics.boundaryEdges) + "\n";
        payload += "non_manifold="
            + std::to_string(diagnostics.nonManifoldEdges) + "\n";
        payload += "duplicate=" + std::to_string(diagnostics.duplicateFaces) + "\n";
        payload += "opposite_duplicate="
            + std::to_string(diagnostics.oppositeDuplicateFaces) + "\n";
        payload += "winding="
            + std::to_string(diagnostics.localWindingIssues) + "\n";
        payload += "degenerate="
            + std::to_string(diagnostics.degenerateTriangles) + "\n";
        payload += "candidate_pairs="
            + std::to_string(selfIntersection.candidatePairCount) + "\n";
        payload += "tested_pairs="
            + std::to_string(selfIntersection.testedPairCount) + "\n";
        payload += "confirmed_pairs="
            + std::to_string(selfIntersection.confirmedIntersectionPairs) + "\n";
        payload += "coplanar_pairs="
            + std::to_string(selfIntersection.coplanarOverlapPairs) + "\n";
        payload += "pair_hash="
            + selfIntersection.candidatePairHash.value_or(std::string{}) + "\n";
    }
    return ComputeMeshRepairSha256(payload);
}

LoadedAsset LoadAsset(
    const std::filesystem::path& configPath,
    const ModelPreflightOptions& options)
{
    ModelPreflightRequest preflightRequest;
    preflightRequest.configPath = configPath;
    preflightRequest.options = options;
    ModelPreflightService preflightService;
    LoadedAsset loaded;
    loaded.execution = preflightService.Run(preflightRequest);

    const SliceConfig config = load_slice_config(configPath);
    const ModelReport scene = load_model_report(
        config,
        ConfigDirectory(configPath));
    SceneModelTriangleMeshAdapterOptions adapterOptions;
    adapterOptions.position_epsilon_mm = options.positionEpsilonMm;
    adapterOptions.degenerate_area_epsilon_mm2 =
        options.degenerateAreaEpsilonMm2;
    const AdaptedTriangleMesh adapted = AdaptSceneModelToTriangleMesh(
        scene,
        adapterOptions);

    RepairedAssetSnapshot& snapshot = loaded.snapshot;
    snapshot.source_path = scene.model_path.generic_string();
    snapshot.input_format = scene.format;
    snapshot.source_hash = loaded.execution.result.identity.sourceHash;
    snapshot.resource_hash = loaded.execution.result.identity.resourceHash;
    snapshot.transform_hash = loaded.execution.result.identity.transformHash;
    snapshot.geometry_hash = ComputeMeshRepairGeometryHash(adapted.mesh);
    snapshot.attribute_hash = ComputeMeshRepairAttributeHash(adapted);
    snapshot.audit_hash = BuildAuditHash(loaded.execution);
    snapshot.preflight_status = ModelPreflightStatusName(
        loaded.execution.result.status);
    snapshot.configured_unit = config.transform.unit;
    snapshot.selected_orientation = scene.auto_orient.selected_orientation;
    snapshot.vertex_count = static_cast<std::uint64_t>(adapted.mesh.vertices.size());
    snapshot.triangle_count = static_cast<std::uint64_t>(adapted.mesh.triangles.size());
    snapshot.component_count = CountConnectedComponents(adapted.mesh);
    if (loaded.execution.full_audit.available)
    {
        const MeshRepairDiagnosticsSummary& diagnostics =
            loaded.execution.full_audit.diagnostics;
        const MeshCompleteSelfIntersectionAnalysis& selfIntersection =
            loaded.execution.full_audit.self_intersection;
        snapshot.boundary_edge_count = diagnostics.boundaryEdges;
        snapshot.non_manifold_edge_count = diagnostics.nonManifoldEdges;
        snapshot.duplicate_face_count = diagnostics.duplicateFaces;
        snapshot.opposite_duplicate_face_count =
            diagnostics.oppositeDuplicateFaces;
        snapshot.local_winding_issue_count = diagnostics.localWindingIssues;
        snapshot.degenerate_triangle_count = diagnostics.degenerateTriangles;
        snapshot.self_intersection_candidate_pair_count =
            selfIntersection.candidatePairCount;
        snapshot.self_intersection_tested_pair_count =
            selfIntersection.testedPairCount;
        snapshot.confirmed_self_intersection_pair_count =
            selfIntersection.confirmedIntersectionPairs;
        snapshot.coplanar_overlap_pair_count =
            selfIntersection.coplanarOverlapPairs;
        snapshot.self_intersection_candidate_pair_hash =
            selfIntersection.candidatePairHash.value_or(std::string{});
    }
    snapshot.material_count = static_cast<std::uint64_t>(scene.material_infos.size());
    snapshot.texture_count = CountTextures(scene);
    snapshot.missing_texture_count = CountMissingTextures(scene);
    snapshot.uv_triangle_count = static_cast<std::uint64_t>(std::count_if(
        adapted.triangle_attributes.begin(),
        adapted.triangle_attributes.end(),
        [](const SurfaceTriangleAttributes& attributes)
        {
            return attributes.has_uv;
        }));
    snapshot.uv_coverage = snapshot.triangle_count == 0U
        ? 0.0
        : static_cast<double>(snapshot.uv_triangle_count)
            / static_cast<double>(snapshot.triangle_count);
    snapshot.bbox_min_mm = {
        scene.bbox_mm.min.x,
        scene.bbox_mm.min.y,
        scene.bbox_mm.min.z};
    snapshot.bbox_max_mm = {
        scene.bbox_mm.max.x,
        scene.bbox_mm.max.y,
        scene.bbox_mm.max.z};
    for (std::size_t axis{0U}; axis < snapshot.dimensions_mm.size(); ++axis)
    {
        snapshot.dimensions_mm.at(axis) =
            snapshot.bbox_max_mm.at(axis) - snapshot.bbox_min_mm.at(axis);
    }
    snapshot.volume_mm3 = std::abs(adapted.topology.signed_volume_mm3);
    snapshot.full_audit_complete = loaded.execution.fullComplete
        && loaded.execution.full_audit.available
        && loaded.execution.full_audit.self_intersection.complete;
    snapshot.strict_pass = snapshot.full_audit_complete
        && loaded.execution.result.status == ModelPreflightStatus::Passed;
    return loaded;
}

std::int64_t SignedDelta(
    const std::uint64_t candidate,
    const std::uint64_t original)
{
    return static_cast<std::int64_t>(candidate)
        - static_cast<std::int64_t>(original);
}

RepairedAssetDelta BuildDelta(
    const RepairedAssetSnapshot& original,
    const RepairedAssetSnapshot& candidate)
{
    RepairedAssetDelta delta;
    delta.vertex_count = SignedDelta(candidate.vertex_count, original.vertex_count);
    delta.triangle_count = SignedDelta(candidate.triangle_count, original.triangle_count);
    delta.component_count = SignedDelta(candidate.component_count, original.component_count);
    delta.material_count = SignedDelta(candidate.material_count, original.material_count);
    delta.texture_count = SignedDelta(candidate.texture_count, original.texture_count);
    delta.uv_triangle_count = SignedDelta(
        candidate.uv_triangle_count,
        original.uv_triangle_count);
    for (std::size_t axis{0U}; axis < delta.dimensions_mm.size(); ++axis)
    {
        delta.dimensions_mm.at(axis) =
            candidate.dimensions_mm.at(axis) - original.dimensions_mm.at(axis);
        delta.max_abs_dimension_delta_mm = std::max(
            delta.max_abs_dimension_delta_mm,
            std::abs(delta.dimensions_mm.at(axis)));
    }
    delta.geometry_changed = original.geometry_hash != candidate.geometry_hash;
    delta.attributes_changed = original.attribute_hash != candidate.attribute_hash;
    delta.resources_changed = original.resource_hash != candidate.resource_hash;
    delta.transform_changed = original.transform_hash != candidate.transform_hash;
    delta.orientation_changed =
        original.selected_orientation != candidate.selected_orientation;
    return delta;
}

bool ProvenanceComplete(const RepairedAssetProvenance& provenance)
{
    return !provenance.provider.empty()
        && !provenance.tool.empty()
        && !provenance.tool_version.empty()
        && !provenance.operation_summary.empty()
        && !provenance.timestamp.empty()
        && !provenance.operator_id.empty();
}

bool SnapshotsRepeat(
    const RepairedAssetSnapshot& first,
    const RepairedAssetSnapshot& second)
{
    return first.source_hash == second.source_hash
        && first.resource_hash == second.resource_hash
        && first.geometry_hash == second.geometry_hash
        && first.attribute_hash == second.attribute_hash
        && first.transform_hash == second.transform_hash
        && first.audit_hash == second.audit_hash
        && first.selected_orientation == second.selected_orientation
        && first.preflight_status == second.preflight_status
        && first.full_audit_complete == second.full_audit_complete
        && first.strict_pass == second.strict_pass;
}

void SortIssues(RepairedAssetIntakeResult& result)
{
    std::sort(
        result.issues.begin(),
        result.issues.end(),
        [](const RepairedAssetIntakeIssue& left,
           const RepairedAssetIntakeIssue& right)
        {
            return std::pair{left.code, left.message}
                < std::pair{right.code, right.message};
        });
    result.issues.erase(
        std::unique(
            result.issues.begin(),
            result.issues.end(),
            [](const RepairedAssetIntakeIssue& left,
               const RepairedAssetIntakeIssue& right)
            {
                return left.code == right.code
                    && left.message == right.message
                    && left.context.dump(0) == right.context.dump(0);
            }),
        result.issues.end());
}

}  // namespace

RepairedAssetIntakeResult RepairedAssetIntakeService::Run(
    const RepairedAssetIntakeRequest& request) const
{
    RepairedAssetIntakeResult result;
    result.family_id = request.family_id;
    result.candidate_id = request.candidate_id;
    result.candidate_kind = request.candidate_kind;
    result.provenance = request.provenance;
    result.approval = request.approval;

    const std::string familyDirectory = RequiredFamilyDirectory(
        request.family_id);
    if (familyDirectory.empty())
    {
        AddIssue(result, kFamilyUnknown, "required family is not registered");
    }
    if (request.candidate_id.empty()
        || request.original_config_path.empty()
        || request.candidate_config_path.empty()
        || request.expected_original_source_hash.empty()
        || request.expected_candidate_source_hash.empty())
    {
        AddIssue(result, kManifestInvalid, "required manifest field is empty");
    }
    if (request.candidate_kind == RepairedAssetCandidateKind::Unknown)
    {
        AddIssue(result, kCandidateKindInvalid, "candidate kind is unknown");
    }
    if (!(request.approval.max_dimension_delta_mm >= 0.0)
        || !std::isfinite(request.approval.max_dimension_delta_mm))
    {
        AddIssue(result, kManifestInvalid, "dimension delta limit is invalid");
    }
    if (request.approval.allow_attribute_changes
        && request.approval.attribute_change_reason.empty())
    {
        AddIssue(
            result,
            kAttributeMismatch,
            "attribute changes require an approval reason");
    }
    if ((request.candidate_kind == RepairedAssetCandidateKind::ExternalRepaired
         || request.candidate_kind
             == RepairedAssetCandidateKind::IndependentlyRebuilt)
        && !ProvenanceComplete(request.provenance))
    {
        AddIssue(result, kProvenanceMissing, "changed candidate provenance is incomplete");
    }
    if (!result.issues.empty())
    {
        SortIssues(result);
        return result;
    }

    try
    {
        const LoadedAsset original = LoadAsset(
            request.original_config_path,
            request.preflight_options);
        const LoadedAsset candidateFirst = LoadAsset(
            request.candidate_config_path,
            request.preflight_options);
        const LoadedAsset candidateSecond = LoadAsset(
            request.candidate_config_path,
            request.preflight_options);
        result.original = original.snapshot;
        result.candidate = candidateFirst.snapshot;
        result.delta = BuildDelta(result.original, result.candidate);

        if (!PathContainsDirectory(
                std::filesystem::path{result.original.source_path},
                familyDirectory))
        {
            AddIssue(
                result,
                kFamilyPathMismatch,
                "original asset does not belong to the required family",
                Json::object({{"expectedDirectory", familyDirectory}}));
        }
        if (result.original.source_hash != request.expected_original_source_hash)
        {
            AddIssue(result, kOriginalHashMismatch, "original source hash does not match manifest");
        }
        if (result.candidate.source_hash != request.expected_candidate_source_hash)
        {
            AddIssue(result, kCandidateHashMismatch, "candidate source hash does not match manifest");
        }
        if (request.candidate_kind == RepairedAssetCandidateKind::StrictPassOriginal)
        {
            if (result.original.source_hash != result.candidate.source_hash
                || !PathContainsDirectory(
                    std::filesystem::path{result.candidate.source_path},
                    familyDirectory))
            {
                AddIssue(
                    result,
                    kCandidateIdentityInvalid,
                    "strict original candidate must retain the same family source identity");
            }
        }
        else if (result.original.source_hash == result.candidate.source_hash)
        {
            AddIssue(
                result,
                kCandidateIdentityInvalid,
                "repaired or rebuilt candidate must have a distinct source hash");
        }

        result.manifest_accepted = result.issues.empty();
        if (result.candidate.missing_texture_count > 0U
            || HasPreflightIssue(
                candidateFirst.execution.result,
                ModelPreflightErrorCodeName(
                    ModelPreflightErrorCode::ResourceMissing)))
        {
            AddIssue(result, kResourceMissing, "candidate has missing texture resources");
        }
        if (!result.candidate.full_audit_complete
            || !result.candidate.strict_pass)
        {
            AddIssue(
                result,
                kPostStrictFailed,
                "candidate did not complete a strict-pass full audit",
                Json::object({
                    {"preflightStatus", result.candidate.preflight_status},
                }));
        }
        if (result.delta.max_abs_dimension_delta_mm
            > request.approval.max_dimension_delta_mm)
        {
            AddIssue(
                result,
                kBoundsDeltaExceeded,
                "candidate dimensions exceed the approved delta",
                Json::object({
                    {"actualMm", result.delta.max_abs_dimension_delta_mm},
                    {"limitMm", request.approval.max_dimension_delta_mm},
                }));
        }
        if (result.original.configured_unit != result.candidate.configured_unit
            || result.delta.orientation_changed)
        {
            AddIssue(
                result,
                kCoordinateMismatch,
                "candidate unit or selected orientation differs from the original",
                Json::object({
                    {"originalUnit", result.original.configured_unit},
                    {"candidateUnit", result.candidate.configured_unit},
                    {"originalOrientation", result.original.selected_orientation},
                    {"candidateOrientation", result.candidate.selected_orientation},
                }));
        }
        const bool attributeDifference = result.delta.attributes_changed
            || result.delta.resources_changed
            || result.delta.material_count != 0
            || result.delta.texture_count != 0
            || result.delta.uv_triangle_count != 0;
        if (attributeDifference
            && !request.approval.allow_attribute_changes)
        {
            AddIssue(
                result,
                kAttributeMismatch,
                "candidate attributes differ without explicit approval");
        }

        result.repeatability_pass = SnapshotsRepeat(
            candidateFirst.snapshot,
            candidateSecond.snapshot);
        if (!result.repeatability_pass)
        {
            AddIssue(
                result,
                kRepeatabilityFailed,
                "candidate preflight evidence is not repeatable");
        }
    }
    catch (const std::exception& error)
    {
        AddIssue(
            result,
            kExecutionFailed,
            "intake execution failed",
            Json::object({{"detail", error.what()}}));
    }

    SortIssues(result);
    result.admitted = result.manifest_accepted
        && result.repeatability_pass
        && result.issues.empty();
    result.required_family_pass_count = result.admitted ? 1U : 0U;
    return result;
}

}  // namespace slicer_core
