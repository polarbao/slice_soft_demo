#include "slicer_core/config.h"
#include "slicer_core/diagnostics/ProductionAdmissionPolicy.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/model.h"
#include "slicer_core/reports/ReportWriter.h"
#include "slicer_core/slicer.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
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
        << "[--no-production-rgbwsv] [--experimental-report <path>]\n";
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
    return slicer_core::Json::object({
        {"mode", slicer_core::AdmissionModeName(mode)},
        {"status", slicer_core::AdmissionStatusName(decision.status)},
        {"productionAllowed", decision.productionAllowed},
        {"nonProduction", decision.nonProduction},
        {"blockerCodes", StringsToJsonArray(decision.blockerCodes)},
        {"warningCodes", StringsToJsonArray(decision.warningCodes)},
        {"suggestedActions", StringsToJsonArray(decision.suggestedActions)},
    });
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
    const slicer_core::Json report = slicer_core::Json::object({
        {"schema", "p0.experimental_openvdb_shell_cli_report.1"},
        {"configPath", options.config_path},
        {"experimentalOpenvdbShell", true},
        {"legacyPathExecuted", false},
        {"productionPackageWritten", false},
        {"noProductionRgbwsv", options.no_production_rgbwsv},
        {"writeProductionRgbwsv", false},
        {"openvdb",
         slicer_core::Json::object({
             {"enabled", status.compiled_with_openvdb},
             {"available", status.runtime_available},
             {"version", status.version},
             {"gridName", status.grid_name},
             {"gridClass", status.grid_class},
         })},
        {"diagnostics", slicer_core::ValidationIssuesToJson(issues)},
        {"productionAdmission", AdmissionDecisionToJson(decision, mode)},
    });

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
        if (options.inspect_model)
        {
            return InspectModel(options.config_path);
        }
        if (options.experimental_openvdb_shell)
        {
            return RunExperimentalOpenVdbShellDiagnostic(options);
        }

        slicer_core::SliceRunOptions run_options;
        run_options.write_tiff_layers = !options.preview_only;
        const slicer_core::SliceRunResult result = slicer_core::run_slicer(options.config_path, run_options);
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
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "slicer_cli error: " << error.what() << '\n';
        return 1;
    }
}
