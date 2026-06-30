#include "slicer_core/materials/texture_application/SurfaceShellTextureService.h"

#include <string>

namespace slicer_core
{
namespace
{

ValidationIssue MakeCountIssue(
    const std::string& code,
    const ValidationSeverity severity,
    const std::string& message,
    const int count)
{
    ValidationIssue issue = MakeValidationIssue(code, severity, message);
    issue.context = Json::object({{"count", count}});
    return issue;
}

SurfaceShellTexturePreviewInfo MakePreviewInfo(
    const OpenVdbSurfaceShellResult& shell,
    const SurfaceTextureTransferResult& transfer)
{
    SurfaceShellTexturePreviewInfo preview;
    preview.width = shell.width;
    preview.height = shell.height;
    preview.depth = shell.depth;
    preview.shell_voxels = shell.shell_voxels;
    preview.sampled_texture_voxels = transfer.stats.sampled_texture_voxels;
    preview.unique_color_count = transfer.stats.unique_color_count;
    return preview;
}

}  // namespace

std::vector<ValidationIssue> BuildSurfaceShellTextureIssues(const SurfaceTextureTransferResult& transfer)
{
    std::vector<ValidationIssue> issues;
    if (!transfer.error.empty())
    {
        issues.push_back(MakeValidationIssue(
            "SURFACE_TEXTURE_TRANSFER_FAILED",
            ValidationSeverity::Error,
            transfer.error));
    }
    if (transfer.stats.missing_texture_voxels > 0)
    {
        issues.push_back(MakeCountIssue(
            "TEXTURE_MISSING",
            ValidationSeverity::Warning,
            "one or more shell voxels fell back because source texture data is missing",
            transfer.stats.missing_texture_voxels));
    }
    if (transfer.stats.missing_uv_voxels > 0)
    {
        issues.push_back(MakeCountIssue(
            "TEXTURE_UV_MISSING",
            ValidationSeverity::Warning,
            "one or more shell voxels fell back because source UV data is missing",
            transfer.stats.missing_uv_voxels));
    }
    if (transfer.stats.uv_out_of_range_voxels > 0)
    {
        issues.push_back(MakeCountIssue(
            "TEXTURE_UV_OUT_OF_RANGE",
            ValidationSeverity::Warning,
            "one or more shell voxels sampled UV coordinates outside the normalized range",
            transfer.stats.uv_out_of_range_voxels));
    }
    if (transfer.stats.repeated_sampled_voxels > 0)
    {
        issues.push_back(MakeCountIssue(
            "TEXTURE_REPEAT_SAMPLED",
            ValidationSeverity::Info,
            "one or more shell voxels used repeat addressing for out-of-range UV coordinates",
            transfer.stats.repeated_sampled_voxels));
    }
    if (transfer.stats.transfer_distance_exceeded_voxels > 0)
    {
        issues.push_back(MakeCountIssue(
            "TEXTURE_TRANSFER_DISTANCE_EXCEEDED",
            ValidationSeverity::Warning,
            "one or more shell voxels exceeded the texture transfer distance threshold",
            transfer.stats.transfer_distance_exceeded_voxels));
    }
    if (transfer.stats.query_failed_voxels > 0)
    {
        issues.push_back(MakeCountIssue(
            "SURFACE_TEXTURE_QUERY_FAILED",
            ValidationSeverity::Warning,
            "one or more shell voxels could not find a nearest source triangle",
            transfer.stats.query_failed_voxels));
    }
    return issues;
}

SurfaceShellTextureServiceResult SurfaceShellTextureService::ApplyTexture(
    const SurfaceShellTextureServiceRequest& request) const
{
    SurfaceShellTextureServiceResult result;
    if (request.adapted_mesh == nullptr || request.level_set == nullptr || request.shell == nullptr)
    {
        result.issues.push_back(MakeValidationIssue(
            "SURFACE_TEXTURE_INPUT_MISSING",
            ValidationSeverity::Error,
            "surface shell texture service requires adapted mesh, level set, and shell inputs"));
        return result;
    }

    result.transfer = TransferSurfaceTexture(
        *request.adapted_mesh,
        *request.level_set,
        *request.shell,
        request.transfer_options);
    result.preview_info = MakePreviewInfo(*request.shell, result.transfer);
    result.issues = BuildSurfaceShellTextureIssues(result.transfer);
    result.ok = result.transfer.error.empty();
    return result;
}

}  // namespace slicer_core
