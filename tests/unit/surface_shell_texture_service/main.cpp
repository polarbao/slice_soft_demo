#include "slicer_core/materials/texture_application/SurfaceShellTextureService.h"

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

bool TransferStatsMapToStableIssues()
{
    slicer_core::SurfaceTextureTransferResult transfer;
    transfer.stats.missing_texture_voxels = 2;
    transfer.stats.missing_uv_voxels = 3;
    transfer.stats.uv_out_of_range_voxels = 4;
    transfer.stats.repeated_sampled_voxels = 5;
    transfer.stats.transfer_distance_exceeded_voxels = 6;
    transfer.stats.query_failed_voxels = 7;

    const std::vector<slicer_core::ValidationIssue> issues =
        slicer_core::BuildSurfaceShellTextureIssues(transfer);
    return ExpectTrue(ContainsIssue(issues, "TEXTURE_MISSING"), "missing texture issue")
        && ExpectTrue(ContainsIssue(issues, "TEXTURE_UV_MISSING"), "missing UV issue")
        && ExpectTrue(ContainsIssue(issues, "TEXTURE_UV_OUT_OF_RANGE"), "UV out-of-range issue")
        && ExpectTrue(ContainsIssue(issues, "TEXTURE_REPEAT_SAMPLED"), "repeat sampled issue")
        && ExpectTrue(ContainsIssue(issues, "TEXTURE_TRANSFER_DISTANCE_EXCEEDED"), "distance issue")
        && ExpectTrue(ContainsIssue(issues, "SURFACE_TEXTURE_QUERY_FAILED"), "query issue");
}

bool ServiceRejectsMissingInputs()
{
    const slicer_core::SurfaceShellTextureService service;
    const slicer_core::SurfaceShellTextureServiceResult result = service.ApplyTexture({});
    return ExpectTrue(!result.ok, "missing input service call fails")
        && ExpectTrue(ContainsIssue(result.issues, "SURFACE_TEXTURE_INPUT_MISSING"), "missing input issue");
}

bool ServiceMapsTransferError()
{
    slicer_core::AdaptedTriangleMesh adapted;
    slicer_core::OpenVdbLevelSetResult levelSet;
    slicer_core::OpenVdbSurfaceShellResult shell;
    shell.error = "synthetic shell error";

    slicer_core::SurfaceShellTextureServiceRequest request;
    request.adapted_mesh = &adapted;
    request.level_set = &levelSet;
    request.shell = &shell;

    const slicer_core::SurfaceShellTextureService service;
    const slicer_core::SurfaceShellTextureServiceResult result = service.ApplyTexture(request);
    return ExpectTrue(!result.ok, "transfer error service call fails")
        && ExpectTrue(
            ContainsIssue(result.issues, "SURFACE_TEXTURE_TRANSFER_FAILED"),
            "transfer error stable issue");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"transfer_stats_map_to_stable_issues", TransferStatsMapToStableIssues},
        {"service_rejects_missing_inputs", ServiceRejectsMissingInputs},
        {"service_maps_transfer_error", ServiceMapsTransferError},
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

    std::cout << "Surface shell texture service unit tests complete.\n";
    return 0;
}
