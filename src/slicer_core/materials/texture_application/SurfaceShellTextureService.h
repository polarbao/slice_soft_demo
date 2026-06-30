#pragma once

#include "slicer_core/diagnostics/ValidationIssue.h"
#include "slicer_core/materials/texture_application/SurfaceTextureTransfer.h"

#include <vector>

namespace slicer_core
{

/**
 * @brief Request for applying source texture/material color to an OpenVDB surface shell.
 */
struct SurfaceShellTextureServiceRequest
{
    const AdaptedTriangleMesh* adapted_mesh{nullptr};
    const OpenVdbLevelSetResult* level_set{nullptr};
    const OpenVdbSurfaceShellResult* shell{nullptr};
    SurfaceTextureTransferOptions transfer_options;
};

/**
 * @brief Preview-oriented summary of a surface shell texture transfer.
 */
struct SurfaceShellTexturePreviewInfo
{
    int width{0};
    int height{0};
    int depth{0};
    int shell_voxels{0};
    int sampled_texture_voxels{0};
    int unique_color_count{0};
};

/**
 * @brief Result of surface shell texture service execution.
 */
struct SurfaceShellTextureServiceResult
{
    bool ok{false};
    SurfaceTextureTransferResult transfer;
    SurfaceShellTexturePreviewInfo preview_info;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Build stable validation issues from texture transfer stats and errors.
 * @param transfer Texture transfer result.
 * @return Stable warning/error issues for reports.
 */
std::vector<ValidationIssue> BuildSurfaceShellTextureIssues(const SurfaceTextureTransferResult& transfer);

/**
 * @brief Service boundary for surface shell texture transfer.
 */
class SurfaceShellTextureService
{
public:
    /**
     * @brief Apply texture/material color to shell voxels.
     * @param request Adapted mesh, OpenVDB level set, shell masks, and transfer options.
     * @return Transfer result, preview summary, and stable issues.
     */
    SurfaceShellTextureServiceResult ApplyTexture(const SurfaceShellTextureServiceRequest& request) const;
};

}  // namespace slicer_core
