#include "slicer_core/geometry/OpenVdbGeometryKernelService.h"

#include "slicer_core/geometry/OpenVdbAdapter.h"

#include <string>

namespace slicer_core
{
namespace
{

void AddIssue(
    std::vector<ValidationIssue>& issues,
    const std::string& code,
    const ValidationSeverity severity,
    const std::string& message)
{
    issues.push_back(MakeValidationIssue(code, severity, message));
}

}  // namespace

GeometryKernelResult OpenVdbGeometryKernelService::BuildSurfaceShell(const GeometryKernelRequest& request)
{
    GeometryKernelResult result;
    result.status = GetOpenVdbStatus();
    result.available = result.status.compiled_with_openvdb && result.status.runtime_available;

    if (request.mesh == nullptr)
    {
        AddIssue(
            result.issues,
            "GEOMETRY_KERNEL_MESH_MISSING",
            ValidationSeverity::Error,
            "geometry kernel request does not contain a mesh");
        return result;
    }

    if (!result.available)
    {
        AddIssue(
            result.issues,
            "OPENVDB_UNAVAILABLE",
            ValidationSeverity::Error,
            "OpenVDB geometry kernel service is unavailable because USE_OPENVDB is off or runtime initialization failed");
        return result;
    }

    result.level_set = BuildOpenVdbLevelSet(*request.mesh, request.level_set_options);
    result.status = result.level_set.status;
    result.available = result.level_set.available;
    if (!result.level_set.generated)
    {
        AddIssue(
            result.issues,
            "OPENVDB_LEVEL_SET_FAILED",
            ValidationSeverity::Error,
            result.level_set.error.empty() ? "OpenVDB level set generation failed" : result.level_set.error);
        return result;
    }

    result.surface_shell = ClassifyOpenVdbSurfaceShell(result.level_set, request.shell_options);
    if (!result.surface_shell.error.empty())
    {
        AddIssue(
            result.issues,
            "OPENVDB_SURFACE_SHELL_FAILED",
            ValidationSeverity::Error,
            result.surface_shell.error);
        return result;
    }

    result.ok = true;
    return result;
}

}  // namespace slicer_core
