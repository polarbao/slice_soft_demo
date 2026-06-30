#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/geometry/OpenVdbGeometryKernelService.h"
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

bool ContainsIssue(const std::vector<slicer_core::ValidationIssue>& issues, const std::string& code)
{
    for (const slicer_core::ValidationIssue& issue : issues)
    {
        if (issue.code == code)
        {
            return true;
        }
    }
    return false;
}

bool OpenVdbUnavailableReportsStableIssue()
{
    slicer_core::OpenVdbGeometryKernelService service;
    slicer_core::GeometryKernelRequest request;
    request.mesh = nullptr;
    const slicer_core::GeometryKernelResult missingMeshResult = service.BuildSurfaceShell(request);

    const slicer_core::OpenVdbStatus status = slicer_core::GetOpenVdbStatus();
    slicer_core::TriangleMeshData mesh = slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 0.4);
    request.mesh = &mesh;
    const slicer_core::GeometryKernelResult result = service.BuildSurfaceShell(request);

    if (!status.compiled_with_openvdb || !status.runtime_available)
    {
        return ExpectTrue(!missingMeshResult.ok, "missing mesh request does not succeed")
            && ExpectTrue(
                   ContainsIssue(missingMeshResult.issues, "GEOMETRY_KERNEL_MESH_MISSING"),
                   "missing mesh reports stable issue")
            && ExpectTrue(!result.ok, "OpenVDB OFF result is not ok")
            && ExpectTrue(!result.available, "OpenVDB OFF result is unavailable")
            && ExpectTrue(ContainsIssue(result.issues, "OPENVDB_UNAVAILABLE"), "OpenVDB OFF stable issue");
    }

    return ExpectTrue(result.ok, "OpenVDB ON result succeeds")
        && ExpectTrue(result.available, "OpenVDB ON result available")
        && ExpectTrue(result.surface_shell.inside_voxels > 0, "OpenVDB ON shell has inside voxels");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"openvdb_unavailable_reports_stable_issue", OpenVdbUnavailableReportsStableIssue},
    };

    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        bool passed{false};
        try
        {
            passed = test.second();
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << test.first << " exception=" << error.what() << '\n';
            return 1;
        }
        if (!passed)
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }

    std::cout << "Geometry kernel service unit tests complete.\n";
    return 0;
}
