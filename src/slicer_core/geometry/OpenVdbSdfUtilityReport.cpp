#include "slicer_core/geometry/OpenVdbSdfUtilityReport.h"

#include "slicer_core/geometry/OpenVdbAdapter.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <initializer_list>
#include <sstream>
#include <string>

namespace slicer_core
{
namespace
{

Json MakeStringArray(const std::initializer_list<std::string>& values)
{
    Json::Array array;
    for (const std::string& value : values)
    {
        array.push_back(value);
    }
    return Json{array};
}

Json MakeStringArray(const std::vector<std::string>& values)
{
    Json::Array array;
    for (const std::string& value : values)
    {
        array.push_back(value);
    }
    return Json{array};
}

std::string MakeGeneratedAtUtc()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utcTime{};
#if defined(_WIN32)
    gmtime_s(&utcTime, &now);
#else
    gmtime_r(&now, &utcTime);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utcTime, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

Json MakeUtility(
    const bool available,
    const bool executed,
    const std::string& status,
    const std::string& source,
    const std::string& promoteDecision,
    const Json& metrics,
    const Json& blockers,
    const Json& warnings,
    const Json& notes)
{
    return Json::object({
        {"available", available},
        {"executed", executed},
        {"status", status},
        {"source", source},
        {"promoteDecision", promoteDecision},
        {"metrics", metrics},
        {"timingsMs", Json::object({})},
        {"blockers", blockers},
        {"warnings", warnings},
        {"notes", notes},
    });
}

Json MakeUnavailableUtility(const std::string& source, const std::string& blocker)
{
    return MakeUtility(
        false,
        false,
        "unavailable",
        source,
        "not_evaluated",
        Json::object({}),
        MakeStringArray({blocker}),
        Json::array({}),
        Json::array({}));
}

Json MakeIssueArray(
    const GeometryKernelResult* result,
    const bool openVdbAvailable,
    const std::string& unavailableReason)
{
    Json::Array issues;
    if (!openVdbAvailable)
    {
        issues.push_back(Json::object({
            {"severity", "warning"},
            {"code", unavailableReason},
            {"message", "OpenVDB utility is unavailable because USE_OPENVDB is off or runtime initialization failed."},
            {"path", "build.openVdbAvailable"},
        }));
    }

    if (result != nullptr)
    {
        for (const ValidationIssue& issue : result->issues)
        {
            issues.push_back(Json::object({
                {"severity", ValidationSeverityName(issue.severity)},
                {"code", issue.code},
                {"message", issue.message},
                {"path", "utilities.outerVarnishShell"},
            }));
        }
    }
    return Json{issues};
}

Json MakeOuterVarnishUtility(const GeometryKernelResult& result, const double requestedThicknessMm)
{
    std::vector<std::string> blockers;
    for (const ValidationIssue& issue : result.issues)
    {
        if (issue.severity == ValidationSeverity::Error)
        {
            blockers.push_back(issue.code);
        }
    }

    const bool executed = result.status.compiled_with_openvdb && result.status.runtime_available;
    const std::string status = result.ok ? "pass" : "fail";
    return MakeUtility(
        true,
        executed,
        status,
        "openvdb_sdf_shell",
        "promote",
        Json::object({
            {"voxelSizeMm", result.level_set.voxel_size_mm},
            {"requestedThicknessMm", requestedThicknessMm},
            {"classifiedThicknessMm", result.surface_shell.shell_thickness_mm},
            {"activeVoxels", result.level_set.active_voxels},
            {"memoryBytes", result.level_set.memory_bytes},
            {"insideVoxels", result.surface_shell.inside_voxels},
            {"candidateShellVoxels", result.surface_shell.shell_voxels},
            {"interiorVoxels", result.surface_shell.interior_voxels},
            {"outsideVoxels", result.surface_shell.outside_voxels},
            {"unclassifiedVoxels", result.surface_shell.unclassified_voxels},
            {"xyExpansionAllowed", true},
        }),
        MakeStringArray(blockers),
        MakeStringArray(result.surface_shell.warnings),
        MakeStringArray({"Candidate shell metrics do not write or replace the production V channel."}));
}

Json MakeClearanceUtility(const GeometryKernelResult& result)
{
    return MakeUtility(
        false,
        false,
        "not_evaluated",
        "openvdb_sdf_distance",
        "keep_experimental",
        Json::object({
            {"sharedLevelSetGenerated", result.level_set.generated},
            {"sharedLevelSetActiveVoxels", result.level_set.active_voxels},
            {"sharedLevelSetMemoryBytes", result.level_set.memory_bytes},
            {"voxelSizeMm", result.level_set.voxel_size_mm},
            {"minDistanceMm", Json{nullptr}},
            {"maxDistanceMm", Json{nullptr}},
            {"nearSurfaceVoxelCount", Json{nullptr}},
            {"thinRegionCount", Json{nullptr}},
        }),
        MakeStringArray({"clearance_utility_not_implemented"}),
        Json::array({}),
        MakeStringArray({"Shared level-set evidence is not a production clearance acceptance result."}));
}

Json MakeTopologyUtility(const MeshTopologyReport* topologyReport, const std::string& admissionMode)
{
    if (topologyReport == nullptr)
    {
        return MakeUtility(
            false,
            false,
            "not_evaluated",
            "mesh_diagnostics_plus_openvdb_admission",
            "promote",
            Json::object({}),
            MakeStringArray({"topology_report_missing"}),
            Json::array({}),
            Json::array({}));
    }

    std::vector<std::string> blockers;
    if (topologyReport->boundary_edges > 0)
    {
        blockers.push_back("boundary_edges");
    }
    if (topologyReport->non_manifold_edges > 0)
    {
        blockers.push_back("non_manifold_edges");
    }
    if (!topologyReport->errors.empty())
    {
        blockers.push_back("topology_errors");
    }
    const bool accepted = blockers.empty();

    return MakeUtility(
        true,
        true,
        accepted ? "pass" : "blocked",
        "mesh_diagnostics_plus_openvdb_admission",
        "promote",
        Json::object({
            {"sourceTriangles", static_cast<std::uint64_t>(topologyReport->source_triangles)},
            {"acceptedTriangles", static_cast<std::uint64_t>(topologyReport->accepted_triangles)},
            {"degenerateTriangles", static_cast<std::uint64_t>(topologyReport->degenerate_triangles)},
            {"uniqueVertices", static_cast<std::uint64_t>(topologyReport->unique_vertices)},
            {"boundaryEdges", static_cast<std::uint64_t>(topologyReport->boundary_edges)},
            {"nonManifoldEdges", static_cast<std::uint64_t>(topologyReport->non_manifold_edges)},
            {"signedVolumeMm3", topologyReport->signed_volume_mm3},
            {"orientationFlipped", topologyReport->orientation_flipped},
            {"admissionStatus", accepted ? "accepted" : "rejected"},
            {"admissionMode", admissionMode},
        }),
        MakeStringArray(blockers),
        MakeStringArray(topologyReport->warnings),
        MakeStringArray({"Promotion is limited to a diagnostic and admission-gate utility."}));
}

Json MakeMaterialClosureUtility()
{
    return MakeUtility(
        false,
        false,
        "not_evaluated",
        "semantic_mask_plus_sdf_assist",
        "keep_experimental",
        Json::object({
            {"gapPixelCount", Json{nullptr}},
            {"nearSurfaceGapVoxelCount", Json{nullptr}},
            {"modelSupportContactGapCount", Json{nullptr}},
            {"confidence", "unknown"},
        }),
        MakeStringArray({"material_closure_assist_not_implemented"}),
        MakeStringArray({"Production closure remains governed by RGBWSV semantic masks."}),
        Json::array({}));
}

}  // namespace

Json BuildOpenVdbSdfUtilityReport(const OpenVdbSdfUtilityReportInput& input)
{
    const GeometryKernelResult* result = input.geometry_result;
    const OpenVdbStatus status = result != nullptr ? result->status : GetOpenVdbStatus();
    const bool openVdbAvailable = status.compiled_with_openvdb && status.runtime_available;
    const std::string unavailableReason =
        status.compiled_with_openvdb ? "openvdb_runtime_error" : "use_openvdb_off";

    Json outerVarnish = MakeUnavailableUtility("openvdb_sdf_shell", unavailableReason);
    Json clearance = MakeUnavailableUtility("openvdb_sdf_distance", unavailableReason);
    Json topology = MakeUnavailableUtility("mesh_diagnostics_plus_openvdb_admission", unavailableReason);
    Json materialClosure = MakeUnavailableUtility("semantic_mask_plus_sdf_assist", unavailableReason);
    std::string openVdbRole{"unavailable"};
    std::string nextStep{"configure_or_run_openvdb_on_lane"};

    if (openVdbAvailable && result != nullptr)
    {
        outerVarnish = MakeOuterVarnishUtility(*result, input.requested_shell_thickness_mm);
        clearance = MakeClearanceUtility(*result);
        topology = MakeTopologyUtility(input.topology_report, input.admission_mode);
        materialClosure = MakeMaterialClosureUtility();
        openVdbRole = "sdf_utility_candidate";
        nextStep = "promote_outer_shell_and_topology_utility_design";
    }

    return Json::object({
        {"schema", "slicesoft.openvdb_sdf_utility.12b_r2.1"},
        {"generatedAt", MakeGeneratedAtUtc()},
        {"producer",
         Json::object({
             {"tool", "openvdb_sdf_utility_probe"},
             {"toolVersion", "12b-r2-prototype"},
             {"commandLine", input.command_line},
         })},
        {"build",
         Json::object({
             {"buildType", input.build_type},
             {"useOpenVdb", status.compiled_with_openvdb},
             {"openVdbAvailable", openVdbAvailable},
             {"openVdbVersion", openVdbAvailable ? Json{status.version} : Json{nullptr}},
             {"buildDir", input.build_dir},
             {"openVdbUnavailableReason", openVdbAvailable ? Json{nullptr} : Json{unavailableReason}},
         })},
        {"input",
         Json::object({
             {"modelPath", input.model_path.empty() ? Json{nullptr} : Json{input.model_path}},
             {"format", input.input_format.empty() ? Json{nullptr} : Json{input.input_format}},
             {"samePoseWithLegacy", Json{nullptr}},
             {"admissionMode", openVdbAvailable ? Json{input.admission_mode} : Json{"unavailable"}},
             {"meshDiagnosticsAvailable", openVdbAvailable && input.topology_report != nullptr},
         })},
        {"config",
         Json::object({
             {"voxelSizeMm", result != nullptr ? Json{result->level_set.voxel_size_mm} : Json{nullptr}},
             {"narrowBandVoxel", Json{nullptr}},
             {"requestedShellThicknessMm", input.requested_shell_thickness_mm},
         })},
        {"outputPolicy",
         Json::object({
             {"writesProductionPackage", false},
             {"writesProductionTiff", false},
             {"writesPreview", false},
             {"writesUtilityReport", true},
             {"modifiesLegacyOutput", false},
             {"protocolSchemaTouched", false},
         })},
        {"utilities",
         Json::object({
             {"outerVarnishShell", outerVarnish},
             {"clearanceDistance", clearance},
             {"topologyDiagnostic", topology},
             {"materialClosureAssist", materialClosure},
         })},
        {"decision",
         Json::object({
             {"openVdbRole", openVdbRole},
             {"productionReplacementAllowed", false},
             {"productionReplacementReason", "12B-R0 replacementPass=false and R2 is diagnostic-only"},
             {"recommendedNextStep", nextStep},
             {"capabilitySummary",
              Json::object({
                  {"outerVarnishShell", openVdbAvailable ? "promote" : "not_evaluated"},
                  {"clearanceDistance", openVdbAvailable ? "keep_experimental" : "not_evaluated"},
                  {"topologyDiagnostic", openVdbAvailable ? "promote" : "not_evaluated"},
                  {"materialClosureAssist", openVdbAvailable ? "keep_experimental" : "not_evaluated"},
              })},
         })},
        {"validation",
         Json::object({
             {"schemaValid", true},
             {"commands", Json::array({})},
             {"legacyGuard",
              Json::object({
                  {"ran", false},
                  {"reason", "utility_probe_does_not_run_or_modify_legacy_output"},
              })},
         })},
        {"issues", MakeIssueArray(result, openVdbAvailable, unavailableReason)},
    });
}

}  // namespace slicer_core
