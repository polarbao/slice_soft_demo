#include "CliDiagnosticJson.h"

#include <algorithm>
#include <string>
#include <vector>

namespace slicer_cli
{
namespace
{

slicer_core::Json StringsToJsonArray(const std::vector<std::string>& values)
{
    slicer_core::Json::Array array;
    array.reserve(values.size());
    for (const std::string& value : values)
    {
        array.emplace_back(value);
    }
    return slicer_core::Json{array};
}


void AppendUnique(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end())
    {
        values.push_back(value);
    }
}

std::vector<std::string> ReasonCodes(
    const slicer_core::ProductionAdmissionDecision& decision)
{
    std::vector<std::string> reasonCodes;
    for (const std::string& code : decision.blockerCodes)
    {
        AppendUnique(reasonCodes, code);
    }
    for (const std::string& code : decision.warningCodes)
    {
        AppendUnique(reasonCodes, code);
    }
    return reasonCodes;
}

}  // namespace

slicer_core::Json MemoryStatsToJson(const slicer_core::ProcessMemoryStats& memory)
{
    slicer_core::Json::Object json;
    json["available"] = memory.available;
    json["workingSetBytes"] = memory.working_set_bytes;
    json["peakWorkingSetBytes"] = memory.peak_working_set_bytes;
    return slicer_core::Json{json};
}

slicer_core::Json SliceRunProfileToJson(const slicer_core::SliceRunProfile& profile)
{
    slicer_core::Json::Object json;
    json["available"] = profile.available;
    json["profileLevel"] = profile.profile_level;
    json["configLoadMs"] = profile.config_load_ms;
    json["modelLoadMs"] = profile.model_load_ms;
    json["gridSetupMs"] = profile.grid_setup_ms;
    json["maskSamplingMs"] = profile.mask_sampling_ms;
    json["texturePrepareMs"] = profile.texture_prepare_ms;
    json["supportGenerationMs"] = profile.support_generation_ms;
    json["supportStatisticsScanCount"] =
        profile.support_statistics_scan_count;
    json["layerComputeMs"] = profile.layer_compute_ms;
    json["tiffWriteMs"] = profile.tiff_write_ms;
    json["previewWriteMs"] = profile.preview_write_ms;
    json["layerComposeMs"] = profile.layer_compose_ms;
    json["reportBuildMs"] = profile.report_build_ms;
    json["reportWriteMs"] = profile.report_write_ms;
    json["packagePublishMs"] = profile.package_publish_ms;
    json["sliceProcessingMs"] = profile.slice_processing_ms;
    json["outputWriteMs"] = profile.output_write_ms;
    json["totalMs"] = profile.total_ms;
    json["notes"] = StringsToJsonArray(std::vector<std::string>{
        "Diagnostic-only coarse profile; not part of the RGBWSV production package protocol.",
        "Core-only benchmark disables TIFF, preview, reports, and package publishing, but report JSON objects may still be built in memory."});
    return slicer_core::Json{json};
}

slicer_core::Json AdmissionDecisionToJson(
    const slicer_core::ProductionAdmissionDecision& decision,
    const slicer_core::AdmissionMode mode)
{
    const bool blocked =
        !decision.blockerCodes.empty() || decision.status == slicer_core::AdmissionStatus::FailFast;
    slicer_core::Json::Object json;
    json["mode"] = slicer_core::AdmissionModeName(mode);
    json["status"] = slicer_core::AdmissionStatusName(decision.status);
    json["allowed"] = decision.productionAllowed;
    json["blocked"] = blocked;
    json["warning"] = !decision.warningCodes.empty();
    json["productionAllowed"] = decision.productionAllowed;
    json["nonProduction"] = decision.nonProduction;
    json["reasonCodes"] = StringsToJsonArray(ReasonCodes(decision));
    json["blockerCodes"] = StringsToJsonArray(decision.blockerCodes);
    json["warningCodes"] = StringsToJsonArray(decision.warningCodes);
    json["suggestedActions"] = StringsToJsonArray(decision.suggestedActions);
    return slicer_core::Json{json};
}

}  // namespace slicer_cli
