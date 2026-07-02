#include "slicer_core/config.h"
#include "slicer_core/diagnostics/ProductionAdmissionPolicy.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"

#include <exception>
#include <filesystem>
#include <fstream>
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

bool HasIssueCode(
    const std::vector<slicer_core::ValidationIssue>& issues,
    const std::string& code,
    const slicer_core::ValidationSeverity severity)
{
    for (const slicer_core::ValidationIssue& issue : issues)
    {
        if (issue.code == code && issue.severity == severity)
        {
            return true;
        }
    }
    return false;
}

std::filesystem::path WriteConfig(const std::string& name, const std::string& body)
{
    const std::filesystem::path directory = std::filesystem::path{"output"} / "ExperimentalConfigUnit";
    std::filesystem::create_directories(directory);
    const std::filesystem::path path = directory / name;
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error("failed to write config fixture: " + path.string());
    }
    output << body;
    return path;
}

std::string MinimalConfigBody(const std::string& experimentalBlock)
{
    return std::string{
        "{\n"
        "  \"input\": {\n"
        "    \"modelPath\": \"samples/models/sample.stl\",\n"
        "    \"format\": \"auto\"\n"
        "  }\n"}
        + experimentalBlock
        + "\n}\n";
}

bool OldConfigDefaultsOpenVdbDisabled()
{
    const std::filesystem::path path = WriteConfig("legacy_defaults.json", MinimalConfigBody(""));
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(path);
    return ExpectTrue(!config.experimental.openvdb_pipeline.enabled, "legacy config keeps OpenVDB disabled")
        && ExpectTrue(config.experimental.openvdb_pipeline.engine == "legacy", "legacy config engine default")
        && ExpectTrue(
               config.experimental.openvdb_pipeline.admission_mode == "strict_closed",
               "legacy config admission default")
        && ExpectTrue(
               !config.experimental.openvdb_pipeline.write_production_rgbwsv,
               "legacy config writeProductionRgbwsv default");
}

bool EmptyExperimentalDefaults()
{
    const std::filesystem::path path = WriteConfig(
        "empty_experimental.json",
        MinimalConfigBody(
            ",\n"
            "  \"experimental\": {\n"
            "    \"openvdbPipeline\": {}\n"
            "  }\n"));
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(path);
    return ExpectTrue(!config.experimental.openvdb_pipeline.enabled, "empty experimental disabled")
        && ExpectTrue(config.experimental.openvdb_pipeline.engine == "legacy", "empty experimental engine")
        && ExpectTrue(
               config.experimental.openvdb_pipeline.admission_mode == "strict_closed",
               "empty experimental admission")
        && ExpectTrue(!config.experimental.openvdb_pipeline.write_production_rgbwsv, "empty experimental write");
}

bool SurfaceShellFromSdfRequiresOpenVdb()
{
    const std::filesystem::path path = WriteConfig(
        "surface_shell_without_openvdb.json",
        MinimalConfigBody(
            ",\n"
            "  \"texture\": {\n"
            "    \"enabled\": true,\n"
            "    \"applyMode\": \"surface_shell_from_sdf\"\n"
            "  }\n"));
    try
    {
        (void)slicer_core::load_slice_config(path);
    }
    catch (const std::runtime_error& error)
    {
        return ExpectTrue(
            std::string{error.what()}.find("surface_shell_from_sdf requires") != std::string::npos,
            "surface_shell_from_sdf without OpenVDB is rejected");
    }
    return ExpectTrue(false, "surface_shell_from_sdf without OpenVDB must throw");
}

bool SurfaceShellFromSdfAcceptedWithOpenVdbGate()
{
    const std::filesystem::path path = WriteConfig(
        "surface_shell_with_openvdb.json",
        MinimalConfigBody(
            ",\n"
            "  \"texture\": {\n"
            "    \"enabled\": true,\n"
            "    \"applyMode\": \"surface_shell_from_sdf\"\n"
            "  },\n"
            "  \"experimental\": {\n"
            "    \"openvdbPipeline\": {\n"
            "      \"enabled\": true,\n"
            "      \"engine\": \"openvdb\"\n"
            "    }\n"
            "  }\n"));
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(path);
    return ExpectTrue(config.texture.apply_mode == "surface_shell_from_sdf", "surface shell texture mode accepted")
        && ExpectTrue(config.experimental.openvdb_pipeline.enabled, "surface shell OpenVDB enabled")
        && ExpectTrue(config.experimental.openvdb_pipeline.engine == "openvdb", "surface shell OpenVDB engine");
}

bool EnabledOpenVdbOffReportsUnavailable()
{
    const std::filesystem::path path = WriteConfig(
        "enabled_openvdb.json",
        MinimalConfigBody(
            ",\n"
            "  \"experimental\": {\n"
            "    \"openvdbPipeline\": {\n"
            "      \"enabled\": true,\n"
            "      \"engine\": \"openvdb\"\n"
            "    }\n"
            "  }\n"));
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(path);
    const std::vector<slicer_core::ValidationIssue> issues =
        slicer_core::BuildExperimentalOpenVdbPipelineDiagnostics(config);
    bool foundUnavailable{false};
    for (const slicer_core::ValidationIssue& issue : issues)
    {
        if (issue.code == "OPENVDB_UNAVAILABLE")
        {
            foundUnavailable = true;
        }
    }
    const slicer_core::OpenVdbStatus status = slicer_core::GetOpenVdbStatus();
    if (status.compiled_with_openvdb && status.runtime_available)
    {
        return ExpectTrue(!foundUnavailable, "OpenVDB ON build should not report unavailable");
    }
    return ExpectTrue(foundUnavailable, "OpenVDB OFF build reports OPENVDB_UNAVAILABLE");
}

bool WriteProductionRgbwsvRemainsAdmissionGated()
{
    const std::filesystem::path path = WriteConfig(
        "write_production_rgbwsv.json",
        MinimalConfigBody(
            ",\n"
            "  \"experimental\": {\n"
            "    \"openvdbPipeline\": {\n"
            "      \"enabled\": true,\n"
            "      \"engine\": \"openvdb\",\n"
            "      \"writeProductionRgbwsv\": true\n"
            "    }\n"
            "  }\n"));
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(path);
    const std::vector<slicer_core::ValidationIssue> diagnostics =
        slicer_core::BuildExperimentalOpenVdbPipelineDiagnostics(config);
    bool foundGateWarning{false};
    for (const slicer_core::ValidationIssue& issue : diagnostics)
    {
        if (issue.code == "EXPERIMENTAL_RGBWSV_REQUIRES_ADMISSION")
        {
            foundGateWarning = true;
        }
    }

    const std::vector<slicer_core::ValidationIssue> blockers{
        slicer_core::MakeValidationIssue(
            "MESH_NON_MANIFOLD_EDGES",
            slicer_core::ValidationSeverity::Error,
            "test blocker"),
    };
    const slicer_core::ProductionAdmissionDecision decision =
        slicer_core::EvaluateProductionAdmission(blockers, slicer_core::AdmissionMode::StrictClosed);
    return ExpectTrue(foundGateWarning, "writeProductionRgbwsv emits admission gate warning")
        && ExpectTrue(!decision.productionAllowed, "admission policy still blocks production output");
}

bool WriteProductionRgbwsvRequiresStrictAdmission()
{
    const std::vector<std::string> nonStrictModes{
        "diagnostic_only",
        "warn_and_attempt",
        "repair_then_strict",
    };
    for (const std::string& mode : nonStrictModes)
    {
        const std::filesystem::path path = WriteConfig(
            "write_production_" + mode + ".json",
            MinimalConfigBody(
                ",\n"
                "  \"experimental\": {\n"
                "    \"openvdbPipeline\": {\n"
                "      \"enabled\": true,\n"
                "      \"engine\": \"openvdb\",\n"
                "      \"admissionMode\": \"" + mode + "\",\n"
                "      \"writeProductionRgbwsv\": true\n"
                "    }\n"
                "  }\n"));
        const slicer_core::SliceConfig config = slicer_core::load_slice_config(path);
        const std::vector<slicer_core::ValidationIssue> diagnostics =
            slicer_core::BuildExperimentalOpenVdbPipelineDiagnostics(config);
        if (!ExpectTrue(
                HasIssueCode(
                    diagnostics,
                    "EXPERIMENTAL_RGBWSV_REQUIRES_STRICT_ADMISSION",
                    slicer_core::ValidationSeverity::Error),
                "writeProductionRgbwsv rejects non-strict admission mode " + mode))
        {
            return false;
        }
    }
    return true;
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"old_config_defaults_openvdb_disabled", OldConfigDefaultsOpenVdbDisabled},
        {"empty_experimental_defaults", EmptyExperimentalDefaults},
        {"surface_shell_from_sdf_requires_openvdb", SurfaceShellFromSdfRequiresOpenVdb},
        {"surface_shell_from_sdf_accepted_with_openvdb_gate", SurfaceShellFromSdfAcceptedWithOpenVdbGate},
        {"enabled_openvdb_off_reports_unavailable", EnabledOpenVdbOffReportsUnavailable},
        {"write_production_rgbwsv_remains_admission_gated", WriteProductionRgbwsvRemainsAdmissionGated},
        {"write_production_rgbwsv_requires_strict_admission", WriteProductionRgbwsvRequiresStrictAdmission},
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

    std::cout << "Experimental config unit tests complete.\n";
    return 0;
}
