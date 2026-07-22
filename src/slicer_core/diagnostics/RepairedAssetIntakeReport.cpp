#include "slicer_core/diagnostics/RepairedAssetIntakeReport.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace slicer_core
{
namespace
{

Json ArrayToJson(const std::array<double, 3>& values)
{
    return Json::array({values.at(0U), values.at(1U), values.at(2U)});
}

Json BuildSnapshot(const RepairedAssetSnapshot& snapshot)
{
    return Json::object({
        {"sourcePath", snapshot.source_path},
        {"inputFormat", snapshot.input_format},
        {"sourceHash", snapshot.source_hash},
        {"resourceHash", snapshot.resource_hash},
        {"transformHash", snapshot.transform_hash},
        {"geometryHash", snapshot.geometry_hash},
        {"attributeHash", snapshot.attribute_hash},
        {"auditHash", snapshot.audit_hash},
        {"preflightStatus", snapshot.preflight_status},
        {"fullAuditComplete", snapshot.full_audit_complete},
        {"strictPass", snapshot.strict_pass},
        {"vertexCount", snapshot.vertex_count},
        {"triangleCount", snapshot.triangle_count},
        {"componentCount", snapshot.component_count},
        {"materialCount", snapshot.material_count},
        {"textureCount", snapshot.texture_count},
        {"missingTextureCount", snapshot.missing_texture_count},
        {"uvTriangleCount", snapshot.uv_triangle_count},
        {"bboxMinMm", ArrayToJson(snapshot.bbox_min_mm)},
        {"bboxMaxMm", ArrayToJson(snapshot.bbox_max_mm)},
        {"dimensionsMm", ArrayToJson(snapshot.dimensions_mm)},
        {"volumeMm3", snapshot.volume_mm3},
        {"coordinate",
         Json::object({
             {"unit", snapshot.configured_unit},
             {"selectedOrientation", snapshot.selected_orientation},
             {"transformHash", snapshot.transform_hash},
         })},
        {"topology",
         Json::object({
             {"boundaryEdges", snapshot.boundary_edge_count},
             {"nonManifoldEdges", snapshot.non_manifold_edge_count},
             {"duplicateFaces", snapshot.duplicate_face_count},
             {"oppositeDuplicateFaces", snapshot.opposite_duplicate_face_count},
             {"localWindingIssues", snapshot.local_winding_issue_count},
             {"degenerateTriangles", snapshot.degenerate_triangle_count},
         })},
        {"selfIntersection",
         Json::object({
             {"auditComplete", snapshot.full_audit_complete},
             {"candidatePairs", snapshot.self_intersection_candidate_pair_count},
             {"testedPairs", snapshot.self_intersection_tested_pair_count},
             {"confirmedPairs", snapshot.confirmed_self_intersection_pair_count},
             {"coplanarPairs", snapshot.coplanar_overlap_pair_count},
             {"candidatePairHash",
              snapshot.self_intersection_candidate_pair_hash},
             {"auditHash", snapshot.audit_hash},
         })},
        {"attributes",
         Json::object({
             {"materialCount", snapshot.material_count},
             {"textureCount", snapshot.texture_count},
             {"missingTextureCount", snapshot.missing_texture_count},
             {"uvTriangleCount", snapshot.uv_triangle_count},
             {"uvCoverage", snapshot.uv_coverage},
             {"attributeHash", snapshot.attribute_hash},
             {"resourceHash", snapshot.resource_hash},
         })},
    });
}

Json BuildDelta(const RepairedAssetDelta& delta)
{
    return Json::object({
        {"vertexCount", static_cast<double>(delta.vertex_count)},
        {"triangleCount", static_cast<double>(delta.triangle_count)},
        {"componentCount", static_cast<double>(delta.component_count)},
        {"materialCount", static_cast<double>(delta.material_count)},
        {"textureCount", static_cast<double>(delta.texture_count)},
        {"uvTriangleCount", static_cast<double>(delta.uv_triangle_count)},
        {"dimensionsMm", ArrayToJson(delta.dimensions_mm)},
        {"maxAbsDimensionDeltaMm", delta.max_abs_dimension_delta_mm},
        {"geometryChanged", delta.geometry_changed},
        {"attributesChanged", delta.attributes_changed},
        {"resourcesChanged", delta.resources_changed},
        {"transformChanged", delta.transform_changed},
        {"orientationChanged", delta.orientation_changed},
    });
}

Json BuildProvenance(const RepairedAssetProvenance& provenance)
{
    return Json::object({
        {"provider", provenance.provider},
        {"tool", provenance.tool},
        {"toolVersion", provenance.tool_version},
        {"operationSummary", provenance.operation_summary},
        {"timestamp", provenance.timestamp},
        {"operator", provenance.operator_id},
    });
}

Json BuildIssues(const std::vector<RepairedAssetIntakeIssue>& issues)
{
    Json::Array array;
    for (const RepairedAssetIntakeIssue& issue : issues)
    {
        array.push_back(Json::object({
            {"code", issue.code},
            {"message", issue.message},
            {"context", issue.context},
        }));
    }
    return Json{std::move(array)};
}

Json BuildReasonCodes(const std::vector<RepairedAssetIntakeIssue>& issues)
{
    Json::Array array;
    std::string previous;
    for (const RepairedAssetIntakeIssue& issue : issues)
    {
        if (issue.code != previous)
        {
            array.emplace_back(issue.code);
            previous = issue.code;
        }
    }
    return Json{std::move(array)};
}

}  // namespace

Json BuildRepairedAssetIntakeReport(
    const RepairedAssetIntakeResult& result)
{
    return Json::object({
        {"schema", "slicesoft.repaired_asset_intake.12e_08c_r4.1"},
        {"diagnosticOnly", true},
        {"productionOutputWritten", result.production_output_written},
        {"familyId", result.family_id},
        {"candidateId", result.candidate_id},
        {"candidateKind", RepairedAssetCandidateKindName(result.candidate_kind)},
        {"manifestAccepted", result.manifest_accepted},
        {"status", result.admitted ? "admitted" : "blocked"},
        {"admitted", result.admitted},
        {"requiredFamilyPassCount", result.required_family_pass_count},
        {"repeatability",
         Json::object({
             {"runCount", 2},
             {"hashMatch", result.repeatability_pass},
         })},
        {"approval",
         Json::object({
             {"maxDimensionDeltaMm", result.approval.max_dimension_delta_mm},
             {"allowAttributeChanges", result.approval.allow_attribute_changes},
             {"attributeChangeReason", result.approval.attribute_change_reason},
         })},
        {"provenance", BuildProvenance(result.provenance)},
        {"original", BuildSnapshot(result.original)},
        {"candidate", BuildSnapshot(result.candidate)},
        {"delta", BuildDelta(result.delta)},
        {"reasonCodes", BuildReasonCodes(result.issues)},
        {"issues", BuildIssues(result.issues)},
    });
}

}  // namespace slicer_core
