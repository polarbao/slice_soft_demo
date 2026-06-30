#pragma once

#include "slicer_core/geometry/GeometryKernelService.h"

namespace slicer_core
{

/**
 * @brief Experimental OpenVDB-backed geometry-kernel service.
 */
class OpenVdbGeometryKernelService final : public GeometryKernelService
{
public:
    /**
     * @brief Build an OpenVDB level set and classify it into shell/interior masks.
     * @param request Mesh and OpenVDB options.
     * @return Result with stable issue codes when OpenVDB is unavailable or classification fails.
     */
    GeometryKernelResult BuildSurfaceShell(const GeometryKernelRequest& request) override;
};

}  // namespace slicer_core
