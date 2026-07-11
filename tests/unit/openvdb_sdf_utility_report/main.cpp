#include "slicer_core/geometry/MeshTopologyDiagnostics.h"
#include "slicer_core/geometry/OpenVdbGeometryKernelService.h"
#include "slicer_core/geometry/OpenVdbSdfUtilityReport.h"
#include "slicer_core/geometry/TriangleMeshData.h"

#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool ReportPreservesDiagnosticOnlyContract()
{
    slicer_core::TriangleMeshData mesh = slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 0.4);
    const slicer_core::MeshTopologyReport topology = slicer_core::AnalyzeMeshTopology(mesh);

    slicer_core::GeometryKernelRequest request;
    request.mesh = &mesh;
    request.level_set_options.voxel_size_mm = 0.05;
    request.shell_options.shell_thickness_mm = 0.10;

    slicer_core::OpenVdbGeometryKernelService service;
    const slicer_core::GeometryKernelResult result = service.BuildSurfaceShell(request);

    slicer_core::OpenVdbSdfUtilityReportInput input;
    input.build_type = "Debug";
    input.build_dir = "build";
    input.model_path = "generated://closed-box";
    input.input_format = "generated";
    input.admission_mode = "strict_closed";
    input.requested_shell_thickness_mm = request.shell_options.shell_thickness_mm;
    input.geometry_result = &result;
    input.topology_report = &topology;

    const slicer_core::Json report = slicer_core::BuildOpenVdbSdfUtilityReport(input);

    bool passed = true;
    passed = ExpectTrue(
                 report.at("schema").as_string() == "slicesoft.openvdb_sdf_utility.12b_r2.1",
                 "utility report schema")
        && passed;
    passed = ExpectTrue(
                 !report.at("outputPolicy").at("writesProductionPackage").as_bool(),
                 "utility report does not write production package")
        && passed;
    passed = ExpectTrue(
                 !report.at("outputPolicy").at("writesProductionTiff").as_bool(),
                 "utility report does not write production TIFF")
        && passed;
    passed = ExpectTrue(
                 !report.at("decision").at("productionReplacementAllowed").as_bool(),
                 "utility report forbids production replacement")
        && passed;

    const slicer_core::Json& utilities = report.at("utilities");
    passed = ExpectTrue(utilities.contains("outerVarnishShell"), "outer varnish utility present") && passed;
    passed = ExpectTrue(utilities.contains("clearanceDistance"), "clearance utility present") && passed;
    passed = ExpectTrue(utilities.contains("topologyDiagnostic"), "topology utility present") && passed;
    passed = ExpectTrue(utilities.contains("materialClosureAssist"), "closure utility present") && passed;

    const bool openVdbAvailable = report.at("build").at("openVdbAvailable").as_bool();
    if (openVdbAvailable)
    {
        passed = ExpectTrue(
                     utilities.at("outerVarnishShell").at("status").as_string() == "pass",
                     "OpenVDB ON outer varnish utility passes")
            && passed;
        passed = ExpectTrue(
                     utilities.at("outerVarnishShell").at("metrics").at("activeVoxels").as_int() > 0,
                     "OpenVDB ON report contains active voxel metrics")
            && passed;
        passed = ExpectTrue(
                     utilities.at("topologyDiagnostic").at("status").as_string() == "pass",
                     "OpenVDB ON topology utility passes for the closed fixture")
            && passed;
    }
    else
    {
        passed = ExpectTrue(
                     utilities.at("outerVarnishShell").at("status").as_string() == "unavailable",
                     "OpenVDB OFF outer varnish utility is unavailable")
            && passed;
        passed = ExpectTrue(
                     !utilities.at("outerVarnishShell").at("executed").as_bool(),
                     "OpenVDB OFF outer varnish utility is not executed")
            && passed;
    }

    return passed;
}

bool TopologyReportCountsSourceTriangles()
{
    const slicer_core::TriangleMeshData mesh = slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 0.4);
    const slicer_core::MeshTopologyReport topology = slicer_core::AnalyzeMeshTopology(mesh);
    return ExpectTrue(
        topology.source_triangles == mesh.triangles.size(),
        "topology report counts source triangles before admission");
}

bool RuntimeUnavailableIsNotReportedAsCompileTimeOff()
{
    slicer_core::GeometryKernelResult result;
    result.status.compiled_with_openvdb = true;
    result.status.runtime_available = false;

    slicer_core::OpenVdbSdfUtilityReportInput input;
    input.geometry_result = &result;
    const slicer_core::Json report = slicer_core::BuildOpenVdbSdfUtilityReport(input);

    return ExpectTrue(
        report.at("build").at("openVdbUnavailableReason").as_string() == "openvdb_runtime_error",
        "runtime unavailability is distinct from USE_OPENVDB OFF");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"report_preserves_diagnostic_only_contract", ReportPreservesDiagnosticOnlyContract},
        {"topology_report_counts_source_triangles", TopologyReportCountsSourceTriangles},
        {"runtime_unavailable_is_not_reported_as_compile_time_off", RuntimeUnavailableIsNotReportedAsCompileTimeOff},
    };

    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        try
        {
            if (!test.second())
            {
                return 1;
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << test.first << " exception=" << error.what() << '\n';
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }

    std::cout << "OpenVDB SDF utility report unit tests complete.\n";
    return 0;
}
