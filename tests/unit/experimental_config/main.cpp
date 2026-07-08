#include "slicer_core/config.h"
#include "slicer_core/diagnostics/ProductionAdmissionPolicy.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/slicer.h"

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
               "legacy config writeProductionRgbwsv default")
        && ExpectTrue(!config.model_fill.enabled, "legacy config keeps modelFill disabled")
        && ExpectTrue(!config.support.placement_explicit, "legacy config keeps support placement implicit")
        && ExpectTrue(!config.outer_varnish.enabled, "legacy config keeps outerVarnish disabled");
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

bool Stage12AConfigPlaceholdersParse()
{
    const std::filesystem::path path = WriteConfig(
        "stage_12a_placeholders.json",
        MinimalConfigBody(
            ",\n"
            "  \"modelFill\": {\n"
            "    \"enabled\": true,\n"
            "    \"material\": \"white\",\n"
            "    \"scope\": \"below_texture_surface\",\n"
            "    \"value\": 0,\n"
            "    \"emptyAllowedInProduction\": false,\n"
            "    \"legacyRgbFallback\": false\n"
            "  },\n"
            "  \"support\": {\n"
            "    \"enabled\": true,\n"
            "    \"mode\": \"bottom_projection\",\n"
            "    \"placement\": \"lower\",\n"
            "    \"internalVoid\": {\n"
            "      \"enabled\": true,\n"
            "      \"minAreaPx\": 16,\n"
            "      \"fillRule\": \"all_internal_voids\"\n"
            "    },\n"
            "    \"upper\": {\n"
            "      \"enabled\": false,\n"
            "      \"outside\": \"outer_varnish_shell\",\n"
            "      \"reason\": \"optional_detachable_surface_support\"\n"
            "    }\n"
            "  },\n"
            "  \"outerVarnish\": {\n"
            "    \"enabled\": false,\n"
            "    \"thicknessMm\": 0.0,\n"
            "    \"thicknessStepMm\": 0.01,\n"
            "    \"pixelPitchUm\": 42.3,\n"
            "    \"allowXYExpansion\": true,\n"
            "    \"conflictPolicy\": \"varnish_shell_wins\",\n"
            "    \"value\": 0\n"
            "  },\n"
            "  \"surfaceVarnish\": {\n"
            "    \"enabled\": true,\n"
            "    \"outerSurface\": true,\n"
            "    \"innerSurface\": true,\n"
            "    \"thicknessPx\": 1,\n"
            "    \"value\": 0,\n"
            "    \"source\": \"explicit\"\n"
            "  }\n"));
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(path);
    return ExpectTrue(config.model_fill.enabled, "12A modelFill enabled parses")
        && ExpectTrue(config.model_fill.material == "white", "12A modelFill material parses")
        && ExpectTrue(config.model_fill.scope == "below_texture_surface", "12A modelFill scope parses")
        && ExpectTrue(!config.model_fill.empty_allowed_in_production, "12A modelFill production empty default parses")
        && ExpectTrue(!config.model_fill.legacy_rgb_fallback, "12A modelFill legacy fallback parses")
        && ExpectTrue(config.support.placement == "lower", "12A support placement parses")
        && ExpectTrue(config.support.placement_explicit, "12A support placement explicit parses")
        && ExpectTrue(config.support.internal_void.enabled, "12A internal void support enabled parses")
        && ExpectTrue(config.support.internal_void.min_area_px == 16, "12A internal void min area parses")
        && ExpectTrue(
               config.support.internal_void.fill_rule == "all_internal_voids",
               "12A internal void fill rule parses")
        && ExpectTrue(!config.support.upper.enabled, "12A upper support enabled flag parses")
        && ExpectTrue(
               config.support.upper.outside == "outer_varnish_shell",
               "12A upper support outside boundary parses")
        && ExpectTrue(!config.outer_varnish.enabled, "12A outer varnish disabled parses")
        && ExpectTrue(config.outer_varnish.thickness_mm >= 0.0 && config.outer_varnish.thickness_mm < 0.001,
            "12A outer varnish thickness parses")
        && ExpectTrue(config.outer_varnish.thickness_step_mm > 0.009 && config.outer_varnish.thickness_step_mm < 0.011,
            "12A outer varnish step parses")
        && ExpectTrue(config.outer_varnish.pixel_pitch_um > 42.29 && config.outer_varnish.pixel_pitch_um < 42.31,
            "12A outer varnish pitch parses")
        && ExpectTrue(config.outer_varnish.allow_xy_expansion, "12A outer varnish expansion parses")
        && ExpectTrue(
               config.outer_varnish.conflict_policy == "varnish_shell_wins",
               "12A outer varnish conflict policy parses")
        && ExpectTrue(config.surface_varnish.enabled, "12A surface varnish enabled parses")
        && ExpectTrue(config.surface_varnish.outer_surface, "12A surface varnish outer surface parses")
        && ExpectTrue(config.surface_varnish.inner_surface, "12A surface varnish inner surface parses")
        && ExpectTrue(config.surface_varnish.thickness_px == 1, "12A surface varnish thickness parses")
        && ExpectTrue(config.surface_varnish.source == "explicit", "12A surface varnish source parses");
}

bool Stage12ASurfaceVarnishDisabledAcceptsZeroThickness()
{
    const std::filesystem::path path = WriteConfig(
        "stage_12a_surface_varnish_disabled_zero_thickness.json",
        MinimalConfigBody(
            ",\n"
            "  \"surfaceVarnish\": {\n"
            "    \"enabled\": false,\n"
            "    \"outerSurface\": true,\n"
            "    \"innerSurface\": true,\n"
            "    \"thicknessPx\": 0,\n"
            "    \"value\": 0,\n"
            "    \"source\": \"explicit\"\n"
            "  }\n"));
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(path);
    return ExpectTrue(!config.surface_varnish.enabled, "12A disabled surface varnish parses")
        && ExpectTrue(config.surface_varnish.thickness_px == 0, "12A disabled surface varnish accepts zero thickness");
}

bool Stage12AModelFillRejectsEmptyProductionRgb()
{
    const std::filesystem::path path = WriteConfig(
        "stage_12a_model_fill_rejects_empty_rgb.json",
        MinimalConfigBody(
            ",\n"
            "  \"texture\": {\n"
            "    \"enabled\": true,\n"
            "    \"applyMode\": \"top_surface_band\",\n"
            "    \"nonSurfaceRgbPolicy\": \"empty\"\n"
            "  },\n"
            "  \"modelFill\": {\n"
            "    \"enabled\": true,\n"
            "    \"material\": \"rgb\",\n"
            "    \"scope\": \"below_texture_surface\",\n"
            "    \"emptyAllowedInProduction\": false,\n"
            "    \"legacyRgbFallback\": false\n"
            "  },\n"
            "  \"slicingMode\": \"relief_heightfield\"\n"));
    try
    {
        (void)slicer_core::load_slice_config(path);
    }
    catch (const std::runtime_error& error)
    {
        return ExpectTrue(
            std::string{error.what()}.find("cannot use rgb fill") != std::string::npos,
            "12A modelFill rejects production rgb empty fill");
    }
    return ExpectTrue(false, "12A modelFill production rgb empty fill must throw");
}

bool Stage12AUpperSupportAcceptsOuterVarnishShell()
{
    const std::filesystem::path path = WriteConfig(
        "stage_12a_upper_support_accepts_outer_varnish.json",
        MinimalConfigBody(
            ",\n"
            "  \"support\": {\n"
            "    \"placement\": \"upper\",\n"
            "    \"upper\": {\n"
            "      \"enabled\": true,\n"
            "      \"outside\": \"outer_varnish_shell\"\n"
            "    }\n"
            "  },\n"
            "  \"outerVarnish\": {\n"
            "    \"enabled\": true,\n"
            "    \"thicknessMm\": 0.02,\n"
            "    \"thicknessStepMm\": 0.01,\n"
            "    \"pixelPitchUm\": 42.3,\n"
            "    \"conflictPolicy\": \"varnish_shell_wins\"\n"
            "  }\n"));
    try
    {
        const slicer_core::SliceConfig config = slicer_core::load_slice_config(path);
        return ExpectTrue(config.support.placement == "upper", "12A upper placement parses with outer varnish")
            && ExpectTrue(config.support.upper.enabled, "12A upper support remains enabled with outer varnish")
            && ExpectTrue(config.outer_varnish.enabled, "12A outer varnish remains enabled")
            && ExpectTrue(config.outer_varnish.thickness_mm > 0.019 && config.outer_varnish.thickness_mm < 0.021,
                          "12A outer varnish thickness parses");
    }
    catch (const std::runtime_error& error)
    {
        return ExpectTrue(false, std::string{"12A upper support with outer varnish shell should parse: "} + error.what());
    }
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

bool LegacyRunSlicerRejectsSurfaceShellCandidateConfig()
{
    const std::filesystem::path path = WriteConfig(
        "legacy_rejects_surface_shell_candidate.json",
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
    try
    {
        (void)slicer_core::run_slicer(path);
    }
    catch (const std::runtime_error& error)
    {
        return ExpectTrue(
            std::string{error.what()}.find("--openvdb-candidate-slice") != std::string::npos,
            "legacy run_slicer rejects surface_shell_from_sdf candidate config");
    }
    return ExpectTrue(false, "legacy run_slicer must reject surface shell candidate config");
}

bool LegacyRunSlicerRejectsWriteProductionRgbwsvCandidateConfig()
{
    const std::filesystem::path path = WriteConfig(
        "legacy_rejects_write_production_candidate.json",
        MinimalConfigBody(
            ",\n"
            "  \"experimental\": {\n"
            "    \"openvdbPipeline\": {\n"
            "      \"enabled\": true,\n"
            "      \"engine\": \"openvdb\",\n"
            "      \"writeProductionRgbwsv\": true\n"
            "    }\n"
            "  }\n"));
    try
    {
        (void)slicer_core::run_slicer(path);
    }
    catch (const std::runtime_error& error)
    {
        return ExpectTrue(
            std::string{error.what()}.find("--openvdb-candidate-slice") != std::string::npos,
            "legacy run_slicer rejects writeProductionRgbwsv candidate config");
    }
    return ExpectTrue(false, "legacy run_slicer must reject writeProductionRgbwsv candidate config");
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
        {"stage_12a_config_placeholders_parse", Stage12AConfigPlaceholdersParse},
        {"stage_12a_surface_varnish_disabled_accepts_zero_thickness", Stage12ASurfaceVarnishDisabledAcceptsZeroThickness},
        {"stage_12a_model_fill_rejects_empty_production_rgb", Stage12AModelFillRejectsEmptyProductionRgb},
        {"stage_12a_upper_support_accepts_outer_varnish_shell", Stage12AUpperSupportAcceptsOuterVarnishShell},
        {"surface_shell_from_sdf_requires_openvdb", SurfaceShellFromSdfRequiresOpenVdb},
        {"surface_shell_from_sdf_accepted_with_openvdb_gate", SurfaceShellFromSdfAcceptedWithOpenVdbGate},
        {"legacy_run_slicer_rejects_surface_shell_candidate_config", LegacyRunSlicerRejectsSurfaceShellCandidateConfig},
        {"legacy_run_slicer_rejects_write_production_rgbwsv_candidate_config", LegacyRunSlicerRejectsWriteProductionRgbwsvCandidateConfig},
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
