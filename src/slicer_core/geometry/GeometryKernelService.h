#pragma once

#include "slicer_core/diagnostics/ValidationIssue.h"
#include "slicer_core/geometry/OpenVdbLevelSetBuilder.h"
#include "slicer_core/geometry/OpenVdbSurfaceShell.h"
#include "slicer_core/geometry/TriangleMeshData.h"

#include <vector>

namespace slicer_core
{

/**
 * @brief Request for an experimental geometry-kernel surface shell build.
 */
struct GeometryKernelRequest
{
    const TriangleMeshData* mesh{nullptr};
    OpenVdbLevelSetOptions level_set_options;
    OpenVdbSurfaceShellOptions shell_options;
};

/**
 * @brief Result returned by an experimental geometry-kernel service.
 */
struct GeometryKernelResult
{
    bool ok{false};
    bool available{false};
    OpenVdbStatus status;
    OpenVdbLevelSetResult level_set;
    OpenVdbSurfaceShellResult surface_shell;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Abstract service boundary for experimental geometry-kernel implementations.
 */
class GeometryKernelService
{
public:
    virtual ~GeometryKernelService() = default;

    /**
     * @brief Build a surface shell from a triangle mesh.
     * @param request Mesh and geometry-kernel options.
     * @return Build result with diagnostics; never throws for unavailable optional dependencies.
     */
    virtual GeometryKernelResult BuildSurfaceShell(const GeometryKernelRequest& request) = 0;
};

}  // namespace slicer_core
