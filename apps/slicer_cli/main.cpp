#include "slicer_core/config.h"
#include "slicer_core/diagnostics/ProductionAdmissionPolicy.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/model.h"
#include "slicer_core/pipeline/OpenVdbCandidatePipeline.h"
#include "slicer_core/pipeline/SlicePipeline.h"
#include "slicer_core/reports/ReportWriter.h"
#include "slicer_core/slicer.h"
#include "slicer_core/system/ProcessMemoryStats.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

struct CliOptions
{
    std::string config_path{"samples/configs/slice_config.json"};
    bool inspect_model{false};
    bool preview_only{false};
    bool show_help{false};
    bool experimental_openvdb_shell{false};
    bool openvdb_candidate_slice{false};
    bool benchmark_core_only{false};
    bool openvdb_capability_json{false};
    std::string engine{"legacy"};
    std::string admission_mode{"strict_closed"};
    bool no_production_rgbwsv{true};
    std::filesystem::path experimental_report_path{
        "output/ExperimentalOpenVdbShell/reports/experimental_openvdb_shell_report.json"};
};

void PrintUsage()
{
    std::cout
        << "Usage: slicer_cli --config <path-to-slice_config.json> [--inspect-model] [--preview-only]\n"
        << "       slicer_cli --config <path> --experimental-openvdb-shell "
        << "[--admission-mode strict_closed|warn_and_attempt|diagnostic_only] "
        << "[--no-production-rgbwsv] [--experimental-report <path>]\n"
        << "       slicer_cli --config <path> --openvdb-candidate-slice\n"
        << "       slicer_cli --config <path> --benchmark-core-only "
        << "--engine legacy|openvdb-candidate\n"
        << "       slicer_cli --openvdb-capability-json\n";
}

CliOptions ParseOptions(const int argc, char** argv)
{
    CliOptions options;
    for (int i{1}; i < argc; ++i)
    {
        const std::string arg{argv[i]};
        if (arg == "--help" || arg == "-h")
        {
            options.show_help = true;
            return options;
        }
        if (arg == "--config" && i + 1 < argc)
        {
            options.config_path = argv[++i];
            continue;
        }
        if (arg == "--inspect-model")
        {
            options.inspect_model = true;
            continue;
        }
        if (arg == "--preview-only")
        {
            options.preview_only = true;
            continue;
        }
        if (arg == "--experimental-openvdb-shell")
        {
            options.experimental_openvdb_shell = true;
            continue;
        }
        if (arg == "--openvdb-candidate-slice")
        {
            options.openvdb_candidate_slice = true;
            continue;
        }
        if (arg == "--benchmark-core-only")
        {
            options.benchmark_core_only = true;
            continue;
        }
        if (arg == "--openvdb-capability-json")
        {
            options.openvdb_capability_json = true;
            continue;
        }
        if (arg == "--engine" && i + 1 < argc)
        {
            options.engine = argv[++i];
            continue;
        }
        if (arg == "--admission-mode" && i + 1 < argc)
        {
            options.admission_mode = argv[++i];
            continue;
        }
        if (arg == "--no-production-rgbwsv")
        {
            options.no_production_rgbwsv = true;
            continue;
        }
        if (arg == "--experimental-report" && i + 1 < argc)
        {
            options.experimental_report_path = argv[++i];
            continue;
        }
    }
    return options;
}

slicer_core::AdmissionMode ParseAdmissionMode(const std::string& value)
{
    if (value == "strict_closed")
    {
        return slicer_core::AdmissionMode::StrictClosed;
    }
    if (value == "warn_and_attempt")
    {
        return slicer_core::AdmissionMode::WarnAndAttempt;
    }
    if (value == "diagnostic_only")
    {
        return slicer_core::AdmissionMode::DiagnosticOnly;
    }
    if (value == "repair_then_strict")
    {
        return slicer_core::AdmissionMode::RepairThenStrict;
    }
    throw std::runtime_error("unsupported admission mode: " + value);
}

void AppendUnique(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end())
    {
        values.push_back(value);
    }
}

slicer_core::Json StringsToJsonArray(const std::vector<std::string>& values)
{
    slicer_core::Json::Array array;
    for (const std::string& value : values)
    {
        array.push_back(value);
    }
    return slicer_core::Json{array};
}

std::vector<std::string> ReasonCodes(const slicer_core::ProductionAdmissionDecision& decision)
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

void ForceExperimentalDiagnosticOnly(
    slicer_core::ProductionAdmissionDecision& decision,
    const std::string& reasonCode)
{
    if (decision.productionAllowed)
    {
        decision.status = slicer_core::AdmissionStatus::NonProductionOnly;
        decision.productionAllowed = false;
        decision.nonProduction = true;
        AppendUnique(decision.warningCodes, reasonCode);
        decision.suggestedActions.push_back("Experimental CLI diagnostics do not write production RGBWSV packages.");
    }
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

slicer_core::Json ConfigSnapshotToJson(const slicer_core::SliceConfig& config)
{
    slicer_core::Json::Object output;
    output["packageDir"] = config.output.package_dir.string();
    output["dpiX"] = config.output.dpi_x;
    output["dpiY"] = config.output.dpi_y;
    output["layerThicknessMm"] = config.output.layer_thickness_mm;
    output["channelOrder"] = StringsToJsonArray(config.output.channel_order);
    output["bitDepth"] = config.output.bit_depth;
    output["storageMode"] = config.output.storage_mode;

    slicer_core::Json::Object openvdbPipeline;
    openvdbPipeline["enabled"] = config.experimental.openvdb_pipeline.enabled;
    openvdbPipeline["engine"] = config.experimental.openvdb_pipeline.engine;
    openvdbPipeline["admissionMode"] = config.experimental.openvdb_pipeline.admission_mode;
    openvdbPipeline["failurePolicy"] = config.experimental.openvdb_pipeline.failure_policy;
    openvdbPipeline["allowNonProductionOutput"] =
        config.experimental.openvdb_pipeline.allow_non_production_output;
    openvdbPipeline["writeProductionRgbwsv"] = config.experimental.openvdb_pipeline.write_production_rgbwsv;

    slicer_core::Json::Object experimental;
    experimental["openvdbPipeline"] = slicer_core::Json{openvdbPipeline};

    slicer_core::Json::Object root;
    root["slicingMode"] = config.slicing_mode;
    root["output"] = slicer_core::Json{output};
    root["experimental"] = slicer_core::Json{experimental};
    return slicer_core::Json{root};
}

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

std::string FormatMilliseconds(const double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

void PrintSliceProgress(const slicer_core::SliceRunProgress& progress)
{
    std::cout
        << "SLICE_PROGRESS"
        << " phase=" << progress.phase
        << " current=" << progress.current
        << " total=" << progress.total
        << " percent=" << progress.percent
        << " elapsedMs=" << FormatMilliseconds(progress.elapsed_ms)
        << '\n';
    std::cout.flush();
}

void PrintSliceTiming(
    const std::string& engine,
    const slicer_core::SliceRunProfile& profile,
    const slicer_core::ProcessMemoryStats& memory)
{
    std::cout
        << "SLICE_TIMING"
        << " engine=" << engine
        << " profileLevel=" << profile.profile_level
        << " configLoadMs=" << FormatMilliseconds(profile.config_load_ms)
        << " modelLoadMs=" << FormatMilliseconds(profile.model_load_ms)
        << " sliceProcessingMs=" << FormatMilliseconds(profile.slice_processing_ms)
        << " layerComputeMs=" << FormatMilliseconds(profile.layer_compute_ms)
        << " tiffWriteMs=" << FormatMilliseconds(profile.tiff_write_ms)
        << " previewWriteMs=" << FormatMilliseconds(profile.preview_write_ms)
        << " reportBuildMs=" << FormatMilliseconds(profile.report_build_ms)
        << " reportWriteMs=" << FormatMilliseconds(profile.report_write_ms)
        << " packagePublishMs=" << FormatMilliseconds(profile.package_publish_ms)
        << " outputWriteMs=" << FormatMilliseconds(profile.output_write_ms)
        << " totalMs=" << FormatMilliseconds(profile.total_ms)
        << " memoryAvailable=" << (memory.available ? 1 : 0)
        << " workingSetBytes=" << memory.working_set_bytes
        << " peakWorkingSetBytes=" << memory.peak_working_set_bytes
        << '\n';
    std::cout.flush();
}

slicer_core::Json DiagnosticSummaryToJson(
    const std::vector<slicer_core::ValidationIssue>& issues,
    const slicer_core::ProductionAdmissionDecision& decision)
{
    slicer_core::Json::Object json;
    json["issueCount"] = static_cast<int>(issues.size());
    json["blockerCount"] = static_cast<int>(decision.blockerCodes.size());
    json["warningCount"] = static_cast<int>(decision.warningCodes.size());
    return slicer_core::Json{json};
}

slicer_core::Json OutputContractToJson(
    const slicer_core::SliceConfig& config,
    const std::vector<slicer_core::ValidationIssue>& issues,
    const slicer_core::ProductionAdmissionDecision& decision)
{
    slicer_core::Json::Object resolution;
    resolution["dpiX"] = config.output.dpi_x;
    resolution["dpiY"] = config.output.dpi_y;
    resolution["layerThicknessMm"] = config.output.layer_thickness_mm;

    slicer_core::Json::Object perLayerStats;
    perLayerStats["available"] = false;
    perLayerStats["reason"] = "production package not written by experimental CLI diagnostic path";

    slicer_core::Json::Object textureFidelity;
    textureFidelity["available"] = false;
    textureFidelity["reason"] = "texture transfer is not executed by experimental CLI diagnostic path";
    textureFidelity["fallbackCodes"] = StringsToJsonArray(std::vector<std::string>{});

    slicer_core::Json::Object root;
    root["packageSchema"] = "p0.rgbwsv.2";
    root["channelOrder"] = StringsToJsonArray(config.output.channel_order);
    root["bitDepth"] = config.output.bit_depth;
    root["polarity"] = "black_is_print";
    root["printValue"] = 0;
    root["emptyValue"] = 255;
    root["layerCount"] = nullptr;
    root["resolution"] = slicer_core::Json{resolution};
    root["perLayerStats"] = slicer_core::Json{perLayerStats};
    root["textureFidelity"] = slicer_core::Json{textureFidelity};
    root["fallbackCodes"] = StringsToJsonArray(std::vector<std::string>{});
    root["diagnosticSummary"] = DiagnosticSummaryToJson(issues, decision);
    return slicer_core::Json{root};
}

double HeightMm(const slicer_core::BoundingBox& bbox)
{
    return bbox.max.z - bbox.min.z;
}

void PrintBbox(const char* label, const slicer_core::BoundingBox& bbox)
{
    std::cout << "  " << label << ": [" << bbox.min.x << ", " << bbox.min.y << ", " << bbox.min.z << "] - ["
              << bbox.max.x << ", " << bbox.max.y << ", " << bbox.max.z << "] height=" << HeightMm(bbox)
              << "mm\n";
}

int InspectModel(const std::filesystem::path& config_path)
{
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(config_path);
    const std::filesystem::path config_dir =
        config_path.parent_path().empty() ? std::filesystem::current_path() : config_path.parent_path();
    const slicer_core::ModelReport report = slicer_core::load_model_report(config, config_dir);
    std::cout << "slicer_cli: model inspection\n";
    std::cout << "  modelPath: " << report.model_path.string() << '\n';
    std::cout << "  format: " << report.format << '\n';
    std::cout << "  vertices: " << report.vertex_count << '\n';
    std::cout << "  autoOrient.enabled: " << (report.auto_orient.enabled ? "true" : "false") << '\n';
    std::cout << "  autoOrient.applied: " << (report.auto_orient.applied ? "true" : "false") << '\n';
    std::cout << "  autoOrient.maxHeightMm: " << report.auto_orient.max_height_mm << '\n';
    std::cout << "  autoOrient.selectedOrientation: " << report.auto_orient.selected_orientation << '\n';
    PrintBbox("originalBboxMm", report.auto_orient.original_bbox_mm);
    PrintBbox("orientedBboxMm", report.bbox_mm);
    return 0;
}

int PrintOpenVdbCapabilityJson()
{
    slicer_core::OpenVdbStatus status = slicer_core::GetOpenVdbStatus();
    std::string reason;
    if (status.compiled_with_openvdb)
    {
        try
        {
            const slicer_core::OpenVdbSmokeResult smoke =
                slicer_core::RunOpenVdbSmokeCase();
            status = smoke.status;
            status.runtime_available = smoke.executed
                && smoke.status.runtime_available;
            if (!status.runtime_available)
            {
                reason = smoke.warnings.empty()
                    ? "OpenVDB runtime smoke did not execute"
                    : smoke.warnings.front();
            }
        }
        catch (const std::exception& error)
        {
            status.runtime_available = false;
            reason = error.what();
        }
    }
    else
    {
        reason = status.warnings.empty()
            ? "slicer_cli was built with USE_OPENVDB=OFF"
            : status.warnings.front();
    }

    slicer_core::Json::Object capability;
    capability["schema"] = "slicesoft.openvdb_capability.12e_r4.1";
    capability["compiledWithOpenVdb"] = status.compiled_with_openvdb;
    capability["runtimeAvailable"] = status.runtime_available;
    capability["version"] = status.version;
    capability["reason"] = reason;
    std::cout << slicer_core::Json{capability}.dump(2) << '\n';
    return status.runtime_available ? 0 : 2;
}

std::string BuildTypeName()
{
#ifdef NDEBUG
    return "Release";
#else
    return "Debug";
#endif
}

double MillisecondsSince(const std::chrono::steady_clock::time_point& start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

int RunCoreBenchmark(const CliOptions& options)
{
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(options.config_path);
    const auto start = std::chrono::steady_clock::now();

    int widthPx{0};
    int heightPx{0};
    int layerCount{0};
    int modelPixels{0};
    int supportPixels{0};
    int shellPixels{0};
    bool nonProduction{false};
    bool productionAllowed{true};
    slicer_core::Json profileJson = slicer_core::Json::object({
        {"available", false},
        {"profileLevel", "unavailable"},
        {"reason", "legacy SliceRunProfile is only emitted by the legacy engine path"},
        {"notes", StringsToJsonArray(std::vector<std::string>{})},
    });

    if (options.engine == "legacy")
    {
        slicer_core::SliceRunOptions runOptions;
        runOptions.write_tiff_layers = false;
        runOptions.write_preview_files = false;
        runOptions.write_reports = false;
        const slicer_core::SliceRunResult result = slicer_core::run_slicer(options.config_path, runOptions);
        widthPx = result.width_px;
        heightPx = result.height_px;
        layerCount = result.layer_count;
        modelPixels = result.model_pixel_count;
        supportPixels = result.support_pixel_count;
        profileJson = SliceRunProfileToJson(result.profile);
    }
    else if (options.engine == "openvdb-candidate")
    {
        slicer_core::OpenVdbCandidatePipelineOptions pipelineOptions;
        pipelineOptions.write_tiff_layers = false;
        pipelineOptions.write_preview_files = false;
        pipelineOptions.write_reports = false;
        pipelineOptions.publish_package = false;
        const slicer_core::OpenVdbCandidatePipelineResult result =
            slicer_core::RunOpenVdbCandidatePipeline(options.config_path, pipelineOptions);
        widthPx = result.width_px;
        heightPx = result.height_px;
        layerCount = result.layer_count;
        modelPixels = result.model_pixels;
        supportPixels = result.support_pixels;
        shellPixels = result.shell_pixels;
        nonProduction = result.non_production;
        productionAllowed = !result.non_production;
    }
    else
    {
        throw std::runtime_error("--engine must be legacy or openvdb-candidate");
    }

    const double elapsedMs = MillisecondsSince(start);
    const slicer_core::ProcessMemoryStats memory = slicer_core::CaptureProcessMemoryStats();
    const bool outputSemanticsComparable = options.engine == "legacy" ? true : productionAllowed && supportPixels > 0;

    slicer_core::Json::Object report;
    report["schema"] = "p0.openvdb_legacy_core_benchmark.1";
    report["engine"] = options.engine;
    report["buildType"] = BuildTypeName();
    report["configPath"] = options.config_path;
    report["modelPath"] = config.input.model_path.string();
    report["outputPolicy"] = slicer_core::Json::object({
        {"writeTiff", false},
        {"writePreview", false},
        {"writeReports", "benchmark_stdout_only"},
        {"publishPackage", false},
    });
    report["grid"] = slicer_core::Json::object({
        {"widthPx", widthPx},
        {"heightPx", heightPx},
        {"layerCount", layerCount},
    });
    report["stats"] = slicer_core::Json::object({
        {"modelPixels", modelPixels},
        {"supportPixels", supportPixels},
        {"shellPixels", shellPixels},
    });
    report["productionAdmission"] = slicer_core::Json::object({
        {"productionAllowed", productionAllowed},
        {"nonProduction", nonProduction},
        {"status", nonProduction ? "non_production_only" : "allowed"},
    });
    report["timingsMs"] = slicer_core::Json::object({
        {"coreCompute", elapsedMs},
        {"endToEnd", elapsedMs},
        {"note", "core-only mode disables TIFF, preview, reports, and package publishing"},
    });
    report["memory"] = MemoryStatsToJson(memory);
    report["profile"] = profileJson;
    report["replacementGate"] = slicer_core::Json::object({
        {"performanceComparable", false},
        {"outputSemanticsComparable", outputSemanticsComparable},
        {"replacementPass", false},
        {"reason", outputSemanticsComparable ? "single-engine benchmark; compare pair externally"
                                             : "output semantics are not comparable"},
    });

    std::cout << slicer_core::Json{report}.dump(2) << '\n';
    return 0;
}

int RunExperimentalOpenVdbShellDiagnostic(const CliOptions& options)
{
    slicer_core::SliceConfig config = slicer_core::load_slice_config(options.config_path);
    config.experimental.openvdb_pipeline.enabled = true;
    config.experimental.openvdb_pipeline.engine = "openvdb";
    config.experimental.openvdb_pipeline.admission_mode = options.admission_mode;
    config.experimental.openvdb_pipeline.write_production_rgbwsv = false;
    slicer_core::validate_slice_config(config);

    std::vector<slicer_core::ValidationIssue> issues =
        slicer_core::BuildExperimentalOpenVdbPipelineDiagnostics(config);
    issues.push_back(slicer_core::MakeValidationIssue(
        "EXPERIMENTAL_CLI_DIAGNOSTIC_ONLY",
        slicer_core::ValidationSeverity::Warning,
        "experimental OpenVDB CLI path emits diagnostics only and does not write production RGBWSV packages"));

    const slicer_core::AdmissionMode mode = ParseAdmissionMode(options.admission_mode);
    slicer_core::ProductionAdmissionDecision decision =
        slicer_core::EvaluateProductionAdmission(issues, mode);
    ForceExperimentalDiagnosticOnly(decision, "EXPERIMENTAL_CLI_DIAGNOSTIC_ONLY");

    const slicer_core::OpenVdbStatus status = slicer_core::GetOpenVdbStatus();
    const slicer_core::ProcessMemoryStats memory = slicer_core::CaptureProcessMemoryStats();
    const slicer_core::Json diagnosticSummary = DiagnosticSummaryToJson(issues, decision);

    slicer_core::Json::Object inputJson;
    inputJson["configPath"] = options.config_path;
    inputJson["modelPath"] = config.input.model_path.string();
    inputJson["format"] = config.input.format;
    inputJson["outputPackageDir"] = config.output.package_dir.string();

    slicer_core::Json::Object openvdbJson;
    openvdbJson["enabled"] = status.compiled_with_openvdb;
    openvdbJson["available"] = status.runtime_available;
    openvdbJson["version"] = status.version;
    openvdbJson["gridName"] = status.grid_name;
    openvdbJson["gridClass"] = status.grid_class;

    slicer_core::Json::Object surfaceShellJson;
    surfaceShellJson["requested"] = true;
    surfaceShellJson["generated"] = false;
    surfaceShellJson["status"] = "not_executed_cli_diagnostic_only";
    surfaceShellJson["reasonCodes"] =
        StringsToJsonArray(std::vector<std::string>{"EXPERIMENTAL_CLI_DIAGNOSTIC_ONLY"});

    slicer_core::Json::Object textureTransferJson;
    textureTransferJson["executed"] = false;
    textureTransferJson["status"] = "not_executed_cli_diagnostic_only";
    textureTransferJson["fallbackCodes"] = StringsToJsonArray(std::vector<std::string>{});

    slicer_core::Json::Object materialComposerJson;
    materialComposerJson["executed"] = false;
    materialComposerJson["status"] = "not_executed_cli_diagnostic_only";

    slicer_core::Json::Object legacyPathJson;
    legacyPathJson["executed"] = false;
    legacyPathJson["productionPackageWritten"] = false;

    slicer_core::Json::Object timingJson;
    timingJson["available"] = false;
    timingJson["reason"] = "experimental CLI report schema currently records no wall-clock timings";

    slicer_core::Json::Object reportJson;
    reportJson["schema"] = "p0.experimental_openvdb_shell_cli_report.1";
    reportJson["configPath"] = options.config_path;
    reportJson["input"] = slicer_core::Json{inputJson};
    reportJson["configSnapshot"] = ConfigSnapshotToJson(config);
    reportJson["experimentalOpenvdbShell"] = true;
    reportJson["legacyPathExecuted"] = false;
    reportJson["productionPackageWritten"] = false;
    reportJson["noProductionRgbwsv"] = options.no_production_rgbwsv;
    reportJson["writeProductionRgbwsv"] = false;
    reportJson["openvdb"] = slicer_core::Json{openvdbJson};
    reportJson["surfaceShell"] = slicer_core::Json{surfaceShellJson};
    reportJson["diagnostics"] = slicer_core::ValidationIssuesToJson(issues);
    reportJson["productionAdmission"] = AdmissionDecisionToJson(decision, mode);
    reportJson["textureTransfer"] = slicer_core::Json{textureTransferJson};
    reportJson["materialComposer"] = slicer_core::Json{materialComposerJson};
    reportJson["outputContract"] = OutputContractToJson(config, issues, decision);
    reportJson["legacyPath"] = slicer_core::Json{legacyPathJson};
    reportJson["timing"] = slicer_core::Json{timingJson};
    reportJson["memory"] = MemoryStatsToJson(memory);
    reportJson["stats"] = diagnosticSummary;
    const slicer_core::Json report{reportJson};

    const std::filesystem::path reportDirectory = options.experimental_report_path.parent_path();
    if (!reportDirectory.empty())
    {
        std::filesystem::create_directories(reportDirectory);
    }
    slicer_core::WriteReportJsonFile(options.experimental_report_path, report);

    std::cout << "slicer_cli: experimental OpenVDB diagnostic\n";
    std::cout << "  report: " << options.experimental_report_path.string() << '\n';
    std::cout << "  productionPackageWritten: false\n";
    std::cout << "  productionAllowed: " << (decision.productionAllowed ? "true" : "false") << '\n';
    std::cout << "  status: " << slicer_core::AdmissionStatusName(decision.status) << '\n';
    return 0;
}

int RunOpenVdbCandidateSlice(const CliOptions& options)
{
    if (options.experimental_openvdb_shell)
    {
        throw std::runtime_error(
            "--openvdb-candidate-slice cannot be combined with --experimental-openvdb-shell");
    }

    slicer_core::OpenVdbCandidatePipelineOptions pipelineOptions;
    pipelineOptions.progress_callback = PrintSliceProgress;
    const slicer_core::OpenVdbCandidatePipelineResult result =
        slicer_core::RunOpenVdbCandidatePipeline(options.config_path, pipelineOptions);
    std::cout << "slicer_cli: generated "
              << (result.non_production ? "non-production " : "")
              << "OpenVDB candidate package\n";
    std::cout << "  packageDir: " << result.package_dir.string() << '\n';
    std::cout << "  grid: " << result.width_px << " x " << result.height_px << " x " << result.layer_count
              << '\n';
    std::cout << "  modelPixels: " << result.model_pixels << '\n';
    std::cout << "  supportPixels: " << result.support_pixels << '\n';
    std::cout << "  shellPixels: " << result.shell_pixels << '\n';
    std::cout << "  nonProduction: " << (result.non_production ? "true" : "false") << '\n';
    PrintSliceTiming(
        "openvdb-candidate",
        result.profile,
        slicer_core::CaptureProcessMemoryStats());
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        const CliOptions options = ParseOptions(argc, argv);
        if (options.show_help)
        {
            PrintUsage();
            return 0;
        }
        if (options.openvdb_capability_json)
        {
            return PrintOpenVdbCapabilityJson();
        }
        if (options.inspect_model)
        {
            return InspectModel(options.config_path);
        }
        if (options.benchmark_core_only)
        {
            return RunCoreBenchmark(options);
        }
        if (options.openvdb_candidate_slice)
        {
            return RunOpenVdbCandidateSlice(options);
        }
        if (options.experimental_openvdb_shell)
        {
            return RunExperimentalOpenVdbShellDiagnostic(options);
        }

        slicer_core::SliceRunOptions run_options;
        run_options.write_tiff_layers = !options.preview_only;
        run_options.progress_callback = PrintSliceProgress;
        const slicer_core::SliceRunResult result =
            slicer_core::RunSlicePipeline(options.config_path, run_options);
        std::cout << "slicer_cli: generated package\n";
        if (options.preview_only)
        {
            std::cout << "  mode: preview-only, TIFF layers were not written\n";
        }
        std::cout << "  packageDir: " << result.package_dir.string() << '\n';
        std::cout << "  grid: " << result.width_px << " x " << result.height_px << " x " << result.layer_count
                  << '\n';
        std::cout << "  modelPixels: " << result.model_pixel_count << '\n';
        std::cout << "  supportPixels: " << result.support_pixel_count << '\n';
        PrintSliceTiming(
            result.effective_pipeline_mode,
            result.profile,
            slicer_core::CaptureProcessMemoryStats());
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "slicer_cli error: " << error.what() << '\n';
        return 1;
    }
}
