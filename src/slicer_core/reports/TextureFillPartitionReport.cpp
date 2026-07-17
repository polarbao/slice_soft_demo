#include "slicer_core/reports/TextureFillPartitionReport.h"

#include "slicer_core/materials/texture_application/TextureFillPartitionAdmission.h"

namespace slicer_core
{

TextureFillPartitionReportData BuildTextureFillPartitionUnavailableReportData(const SliceConfig& config)
{
    TextureFillPartitionReportData report;
    report.enabled = IsGlobalTextureFillPartitionRequested(config);
    report.options.requestedWidthMm = config.texture.surface_shell.width_mm;
    report.options.widthStepMm = config.texture.surface_shell.width_step_mm;
    report.options.surfaceScope = config.texture.surface_shell.surface_scope;
    if (report.enabled)
    {
        report.issues.push_back(MakeValidationIssue(
            TextureFillPartitionErrorCodeName(
                TextureFillPartitionErrorCode::PartitionBackendUnavailable),
            ValidationSeverity::Error,
            "global 3D texture/fill partition backend is unavailable"));
    }
    return report;
}

Json BuildTextureFillPartitionReportSkeleton(const SliceConfig& config)
{
    const TextureFillPartitionReportData report =
        BuildTextureFillPartitionUnavailableReportData(config);
    return Json::object({
        {"schema", "slicesoft.texture_fill_partition.12e.1"},
        {"packageProtocol", "p0.rgbwsv.2"},
        {"enabled", report.enabled},
        {"strategy", report.strategy},
        {"availability", report.availability},
        {"status", report.status},
        {"productionAcceptance", report.productionAcceptance},
        {"geometryMode", config.texture.surface_shell.geometry_mode},
        {"surfaceScope", report.options.surfaceScope},
        {"backend", report.backend},
        {"backendRole", report.backendRole},
        {"width",
         Json::object({
             {"requestedWidthMm", report.options.requestedWidthMm},
             {"widthStepMm", report.options.widthStepMm},
             {"baseMinimumWidthMm", report.options.baseMinimumWidthMm},
             {"classificationResolutionMm", nullptr},
             {"effectiveMinimumWidthMm", nullptr},
             {"effectiveWidthMm", nullptr},
             {"maxInteriorDistanceMm", nullptr},
             {"allTextureThresholdMm", nullptr},
             {"allTexture", false},
             {"quantizationErrorMm", nullptr},
             {"clamped", false},
         })},
        {"partition", Json::object({
             {"modelVoxels", 0},
             {"textureSurfaceVoxels", 0},
             {"modelFillVoxels", 0},
             {"overlapTextureFillVoxels", 0},
             {"unassignedModelVoxels", 0},
             {"modelPixels", 0},
             {"textureSurfacePixels", 0},
             {"modelFillPixels", 0},
             {"overlapTextureFillPixels", 0},
             {"unassignedModelPixels", 0},
             {"textureCoverageRatio", 0.0},
             {"modelFillCoverageRatio", 0.0},
             {"thinRegionMergedVoxels", 0},
             {"medialAxisTieCount", 0},
             {"partitionPass", false},
         })},
        {"textureTransfer", Json::object({
             {"sampledTextureCount", 0},
             {"fallbackCount", 0},
             {"missingUvCount", 0},
             {"missingTextureCount", 0},
             {"uvOutOfRangeCount", 0},
             {"outsideColoredCount", 0},
             {"maxTransferDistanceMm", nullptr},
             {"medialAxisTieCount", 0},
         })},
        {"performance", Json::object({
             {"preflightMs", nullptr},
             {"occupancyMs", nullptr},
             {"distanceMs", nullptr},
             {"partitionMs", nullptr},
             {"textureTransferMs", nullptr},
             {"totalCoreMs", nullptr},
             {"peakMemoryBytes", nullptr},
         })},
        {"layers", Json::array({})},
        {"issues", ValidationIssuesToJson(report.issues)},
        {"configSnapshot", Json::object({
             {"textureEnabled", config.texture.enabled},
             {"textureApplyMode", config.texture.apply_mode},
             {"geometryMode", config.texture.surface_shell.geometry_mode},
             {"widthMm", config.texture.surface_shell.width_mm},
             {"widthStepMm", config.texture.surface_shell.width_step_mm},
             {"minimumWidthPolicy", config.texture.surface_shell.minimum_width_policy},
             {"surfaceScope", config.texture.surface_shell.surface_scope},
             {"fullTextureAtModelLimit", config.texture.surface_shell.full_texture_at_model_limit},
             {"modelFillEnabled", config.model_fill.enabled},
             {"modelFillMaterial", config.model_fill.material},
             {"modelFillScope", config.model_fill.scope},
             {"modelFillValue", static_cast<int>(config.model_fill.value)},
         })},
    });
}

}  // namespace slicer_core
