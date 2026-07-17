#include "slicer_core/config.h"
#include "slicer_core/diagnostics/ProductionAdmissionPolicy.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionAdmission.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"
#include "slicer_core/reports/TextureFillPartitionReport.h"
#include "slicer_core/slicer.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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

bool ConfigRejectedWith(
    const std::string& name,
    const std::string& configBlock,
    const std::string& expectedMessage)
{
    const std::filesystem::path configPath = WriteConfig(name, MinimalConfigBody(configBlock));
    try
    {
        (void)slicer_core::load_slice_config(configPath);
    }
    catch (const std::runtime_error& error)
    {
        return ExpectTrue(
            std::string{error.what()}.find(expectedMessage) != std::string::npos,
            name + " reports expected validation message");
    }
    return ExpectTrue(false, name + " must be rejected");
}

bool ConfigRejectedWith12ECode(
    const std::string& name,
    const std::string& configBlock,
    const slicer_core::TextureFillPartitionErrorCode expectedCode)
{
    const std::filesystem::path configPath = WriteConfig(name, MinimalConfigBody(configBlock));
    try
    {
        (void)slicer_core::load_slice_config(configPath);
    }
    catch (const slicer_core::TextureFillPartitionError& error)
    {
        return ExpectTrue(error.Code() == expectedCode, name + " reports the expected stable 12E code")
            && ExpectTrue(
                std::string{error.what()}.find(
                    slicer_core::TextureFillPartitionErrorCodeName(expectedCode)) != std::string::npos,
                name + " includes the stable 12E code in the message");
    }
    catch (const std::exception& error)
    {
        return ExpectTrue(false, name + " reported an untyped error: " + error.what());
    }
    return ExpectTrue(false, name + " must be rejected");
}

std::string GlobalSurfaceShellConfigBlock(const std::string& surfaceShellFields)
{
    return std::string{
        ",\n"
        "  \"texture\": {\n"
        "    \"enabled\": true,\n"
        "    \"applyMode\": \"global_surface_shell\",\n"
        "    \"surfaceShell\": {"}
        + surfaceShellFields
        + "}\n"
          "  },\n"
          "  \"modelFill\": {\n"
          "    \"enabled\": true,\n"
          "    \"material\": \"white\",\n"
          "    \"scope\": \"complement_of_global_texture_shell\",\n"
          "    \"value\": 0,\n"
          "    \"emptyAllowedInProduction\": false\n"
          "  }\n";
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
        && ExpectTrue(
               !slicer_core::IsGlobalTextureFillPartitionRequested(config),
               "legacy config does not request global texture/fill partition")
        && ExpectTrue(!config.support.placement_explicit, "legacy config keeps support placement implicit")
        && ExpectTrue(!config.outer_varnish.enabled, "legacy config keeps outerVarnish disabled");
}

bool Stage12EGlobalSurfaceShellConfigParses()
{
    const std::filesystem::path configPath = WriteConfig(
        "stage_12e_global_surface_shell.json",
        MinimalConfigBody(GlobalSurfaceShellConfigBlock(
            "\n"
            "      \"geometryMode\": \"global_3d_distance\",\n"
            "      \"widthMm\": 0.10,\n"
            "      \"widthStepMm\": 0.01,\n"
            "      \"minimumWidthPolicy\": \"two_cells_floor_0_10_mm\",\n"
            "      \"surfaceScope\": \"all_closed_surfaces\",\n"
            "      \"fullTextureAtModelLimit\": true\n"
            "    ")));
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(configPath);
    return ExpectTrue(
               slicer_core::IsGlobalTextureFillPartitionRequested(config),
               "12E global partition request is recognized")
        && ExpectTrue(
               config.texture.surface_shell.geometry_mode == "global_3d_distance",
               "12E geometry mode parses")
        && ExpectTrue(
               config.texture.surface_shell.width_mm > 0.099
                   && config.texture.surface_shell.width_mm < 0.101,
               "12E width parses")
        && ExpectTrue(
               config.texture.surface_shell.width_step_mm > 0.009
                   && config.texture.surface_shell.width_step_mm < 0.011,
               "12E width step parses")
        && ExpectTrue(
               config.texture.surface_shell.minimum_width_policy == "two_cells_floor_0_10_mm",
               "12E minimum width policy parses")
        && ExpectTrue(
               config.texture.surface_shell.surface_scope == "all_closed_surfaces",
               "12E surface scope parses")
        && ExpectTrue(
               config.texture.surface_shell.full_texture_at_model_limit,
               "12E full-texture limit flag parses")
        && ExpectTrue(config.model_fill.enabled, "12E model fill remains enabled")
        && ExpectTrue(
               config.model_fill.scope == "complement_of_global_texture_shell",
               "12E complement model-fill scope parses");
}

bool Stage12ERejectsInvalidSurfaceShellFields()
{
    const std::string validPrefix =
        "\n"
        "      \"geometryMode\": \"global_3d_distance\",\n"
        "      \"widthMm\": 0.10,\n"
        "      \"widthStepMm\": 0.01,\n"
        "      \"minimumWidthPolicy\": \"two_cells_floor_0_10_mm\",\n"
        "      \"surfaceScope\": \"all_closed_surfaces\",\n"
        "      \"fullTextureAtModelLimit\": true\n"
        "    ";
    if (!ConfigRejectedWith12ECode(
            "stage_12e_zero_width.json",
            GlobalSurfaceShellConfigBlock(
                "\n      \"widthMm\": 0.0\n    "),
            slicer_core::TextureFillPartitionErrorCode::SurfaceShellWidthInvalid)
        || !ConfigRejectedWith12ECode(
            "stage_12e_negative_width.json",
            GlobalSurfaceShellConfigBlock(
                "\n      \"widthMm\": -0.01\n    "),
            slicer_core::TextureFillPartitionErrorCode::SurfaceShellWidthInvalid)
        || !ConfigRejectedWith12ECode(
            "stage_12e_invalid_step.json",
            GlobalSurfaceShellConfigBlock(
                "\n      \"widthStepMm\": 0.02\n    "),
            slicer_core::TextureFillPartitionErrorCode::SurfaceShellStepUnsupported)
        || !ConfigRejectedWith12ECode(
            "stage_12e_invalid_geometry.json",
            GlobalSurfaceShellConfigBlock(
                "\n      \"geometryMode\": \"per_layer_distance\"\n    "),
            slicer_core::TextureFillPartitionErrorCode::SurfaceShellGeometryModeUnsupported)
        || !ConfigRejectedWith12ECode(
            "stage_12e_invalid_minimum_policy.json",
            GlobalSurfaceShellConfigBlock(
                "\n      \"minimumWidthPolicy\": \"none\"\n    "),
            slicer_core::TextureFillPartitionErrorCode::SurfaceShellMinimumPolicyUnsupported)
        || !ConfigRejectedWith12ECode(
            "stage_12e_invalid_surface_scope.json",
            GlobalSurfaceShellConfigBlock(
                "\n      \"surfaceScope\": \"outer_surface_only\"\n    "),
            slicer_core::TextureFillPartitionErrorCode::SurfaceScopeUnsupported)
        || !ConfigRejectedWith12ECode(
            "stage_12e_full_texture_disabled.json",
            GlobalSurfaceShellConfigBlock(
                "\n      \"fullTextureAtModelLimit\": false\n    "),
            slicer_core::TextureFillPartitionErrorCode::FullTextureAtModelLimitRequired))
    {
        return false;
    }

    const std::filesystem::path configPath = WriteConfig(
        "stage_12e_non_finite_width_base.json",
        MinimalConfigBody(GlobalSurfaceShellConfigBlock(validPrefix)));
    slicer_core::SliceConfig config = slicer_core::load_slice_config(configPath);
    config.texture.surface_shell.width_mm = std::numeric_limits<double>::infinity();
    try
    {
        slicer_core::validate_slice_config(config);
    }
    catch (const slicer_core::TextureFillPartitionError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::TextureFillPartitionErrorCode::SurfaceShellWidthInvalid,
            "12E non-finite width reports stable error code");
    }
    return ExpectTrue(false, "12E non-finite width must be rejected");
}

bool Stage12ERejectsMismatchedTextureAndFill()
{
    return ConfigRejectedWith12ECode(
               "stage_12e_global_with_legacy_fill.json",
               ",\n"
               "  \"texture\": {\"enabled\": true, \"applyMode\": \"global_surface_shell\"},\n"
               "  \"modelFill\": {\"enabled\": true, \"scope\": \"all_model\"}\n",
               slicer_core::TextureFillPartitionErrorCode::TextureFillScopeMismatch)
        && ConfigRejectedWith12ECode(
               "stage_12e_complement_with_legacy_texture.json",
               ",\n"
               "  \"modelFill\": {\"enabled\": true, \"scope\": \"complement_of_global_texture_shell\"}\n",
               slicer_core::TextureFillPartitionErrorCode::TextureFillScopeMismatch)
        && ConfigRejectedWith12ECode(
               "stage_12e_global_with_invalid_fill_scope.json",
               ",\n"
               "  \"texture\": {\"enabled\": true, \"applyMode\": \"global_surface_shell\"},\n"
               "  \"modelFill\": {\"enabled\": true, \"scope\": \"unsupported_scope\"}\n",
               slicer_core::TextureFillPartitionErrorCode::TextureFillScopeMismatch)
        && ConfigRejectedWith12ECode(
               "stage_12e_model_fill_disabled.json",
               ",\n"
               "  \"texture\": {\"enabled\": true, \"applyMode\": \"global_surface_shell\"},\n"
               "  \"modelFill\": {\"enabled\": false, \"scope\": \"complement_of_global_texture_shell\"}\n",
               slicer_core::TextureFillPartitionErrorCode::ModelFillRequired);
}

bool Stage12EBackendUnavailableBlocksBeforePackageWrite()
{
    const std::filesystem::path packagePath =
        std::filesystem::path{"output"} / "ExperimentalConfigUnit" / "stage_12e_blocked_package";
    std::error_code removeError;
    std::filesystem::remove_all(packagePath, removeError);
    const std::filesystem::path configPath = WriteConfig(
        "stage_12e_backend_unavailable.json",
        "{\n"
        "  \"input\": {\"modelPath\": \"samples/models/sample.stl\", \"format\": \"auto\"},\n"
        "  \"output\": {\"packageDir\": \"output/ExperimentalConfigUnit/stage_12e_blocked_package\"},\n"
        "  \"texture\": {\"enabled\": true, \"applyMode\": \"global_surface_shell\"},\n"
        "  \"modelFill\": {\"enabled\": true, \"scope\": \"complement_of_global_texture_shell\"}\n"
        "}\n");
    try
    {
        (void)slicer_core::run_slicer(configPath);
    }
    catch (const slicer_core::TextureFillPartitionError& error)
    {
        return ExpectTrue(
                   error.Code()
                       == slicer_core::TextureFillPartitionErrorCode::PartitionBackendUnavailable,
                   "12E unavailable backend reports stable error code")
            && ExpectTrue(
                   !std::filesystem::exists(packagePath),
                   "12E unavailable backend does not write a package");
    }
    return ExpectTrue(false, "12E unavailable backend must block slicing");
}

bool Stage12EUnavailableReportSkeletonIsHonest()
{
    const std::filesystem::path configPath = WriteConfig(
        "stage_12e_report_skeleton.json",
        MinimalConfigBody(GlobalSurfaceShellConfigBlock("")));
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(configPath);
    const slicer_core::Json report = slicer_core::BuildTextureFillPartitionReportSkeleton(config);
    return ExpectTrue(
               report.at("schema").as_string() == "slicesoft.texture_fill_partition.12e.1",
               "12E report schema is stable")
        && ExpectTrue(report.at("availability").as_string() == "unavailable", "12E report is unavailable")
        && ExpectTrue(report.at("status").as_string() == "blocked", "12E report is blocked")
        && ExpectTrue(
               report.at("productionAcceptance").as_string() == "not_evaluated",
               "12E production acceptance is not evaluated")
        && ExpectTrue(
               !report.at("partition").at("partitionPass").as_bool(),
               "12E unavailable skeleton never reports partition pass")
        && ExpectTrue(report.at("issues").size() == 1U, "12E unavailable skeleton contains one stable issue")
        && ExpectTrue(
               report.at("issues").at(0).at("code").as_string()
                   == "E_12E_PARTITION_BACKEND_UNAVAILABLE",
               "12E unavailable report contains stable backend code");
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

bool MaterialClosureDefaultsAreDiagnosticOnly()
{
    const std::filesystem::path configPath =
        WriteConfig("material_closure_defaults.json", MinimalConfigBody(""));
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(configPath);
    return ExpectTrue(config.material_closure.enabled, "material closure defaults enabled")
        && ExpectTrue(config.material_closure.mode == "diagnostic", "material closure default mode")
        && ExpectTrue(config.material_closure.connectivity == 8, "material closure default connectivity")
        && ExpectTrue(config.material_closure.max_gap_px == 1, "material closure default max gap")
        && ExpectTrue(!config.material_closure.repair.enabled, "material closure repair defaults disabled")
        && ExpectTrue(config.material_closure.repair.color_fill_gap == "model_fill", "color fill repair default")
        && ExpectTrue(config.material_closure.repair.model_support_gap == "contextual", "model support repair default")
        && ExpectTrue(config.material_closure.repair.internal_void_gap == "support", "internal void repair default")
        && ExpectTrue(config.material_closure.repair.varnish_support_gap == "support", "varnish support repair default")
        && ExpectTrue(config.material_closure.fail_on_gap, "material closure failOnGap default")
        && ExpectTrue(!config.material_closure.write_gap_preview, "material closure preview default disabled");
}

bool MaterialClosureDiagnosticConfigParses()
{
    const std::filesystem::path configPath = WriteConfig(
        "material_closure_diagnostic.json",
        MinimalConfigBody(
            ",\n"
            "  \"materialClosure\": {\n"
            "    \"enabled\": false,\n"
            "    \"mode\": \"diagnostic\",\n"
            "    \"connectivity\": 4,\n"
            "    \"maxGapPx\": 2,\n"
            "    \"repair\": {\n"
            "      \"enabled\": false,\n"
            "      \"colorFillGap\": \"model_fill\",\n"
            "      \"modelSupportGap\": \"contextual\",\n"
            "      \"internalVoidGap\": \"support\",\n"
            "      \"varnishSupportGap\": \"support\"\n"
            "    },\n"
            "    \"failOnGap\": false,\n"
            "    \"writeGapPreview\": true\n"
            "  }\n"));
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(configPath);
    return ExpectTrue(!config.material_closure.enabled, "material closure explicit enabled parses")
        && ExpectTrue(config.material_closure.mode == "diagnostic", "material closure explicit mode parses")
        && ExpectTrue(config.material_closure.connectivity == 4, "material closure explicit connectivity parses")
        && ExpectTrue(config.material_closure.max_gap_px == 2, "material closure diagnostic max gap parses")
        && ExpectTrue(!config.material_closure.repair.enabled, "material closure explicit repair parses")
        && ExpectTrue(!config.material_closure.fail_on_gap, "material closure failOnGap parses")
        && ExpectTrue(config.material_closure.write_gap_preview, "material closure preview flag parses");
}

bool MaterialClosureSlicerConfig1Parses()
{
    const std::filesystem::path configPath = WriteConfig(
        "material_closure_schema_v1.json",
        "{\n"
        "  \"schema\": \"slicer.config.1\",\n"
        "  \"input\": {\"modelPath\": \"samples/models/sample.stl\", \"format\": \"auto\"},\n"
        "  \"materialClosure\": {\"enabled\": false, \"connectivity\": 4, \"failOnGap\": false}\n"
        "}\n");
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(configPath);
    return ExpectTrue(!config.material_closure.enabled, "schema v1 material closure enabled parses")
        && ExpectTrue(config.material_closure.connectivity == 4, "schema v1 material closure connectivity parses")
        && ExpectTrue(!config.material_closure.fail_on_gap, "schema v1 material closure failOnGap parses");
}

bool MaterialClosureRepairConfigParses()
{
    const std::filesystem::path configPath = WriteConfig(
        "material_closure_repair.json",
        MinimalConfigBody(
            ",\n"
            "  \"modelFill\": {\"enabled\": true, \"material\": \"white\", \"scope\": \"all_model\"},\n"
            "  \"materialClosure\": {\n"
            "    \"mode\": \"repair_then_report\",\n"
            "    \"maxGapPx\": 1,\n"
            "    \"repair\": {\"enabled\": true}\n"
            "  }\n"));
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(configPath);
    return ExpectTrue(config.material_closure.enabled, "material closure remains enabled")
        && ExpectTrue(
            config.material_closure.mode == "repair_then_report",
            "repair mode parses")
        && ExpectTrue(config.material_closure.repair.enabled, "repair enabled parses")
        && ExpectTrue(config.material_closure.max_gap_px == 1, "repair max gap remains one pixel");
}

bool MaterialClosureRejectsInvalidConfiguration()
{
    return ConfigRejectedWith(
               "material_closure_invalid_mode.json",
               ",\n  \"materialClosure\": {\"mode\": \"repair\"}\n",
               "materialClosure.mode")
        && ConfigRejectedWith(
               "material_closure_invalid_connectivity.json",
               ",\n  \"materialClosure\": {\"connectivity\": 6}\n",
               "materialClosure.connectivity")
        && ConfigRejectedWith(
               "material_closure_invalid_max_gap.json",
               ",\n  \"materialClosure\": {\"maxGapPx\": 0}\n",
               "materialClosure.maxGapPx")
        && ConfigRejectedWith(
               "material_closure_invalid_color_fill.json",
               ",\n  \"materialClosure\": {\"repair\": {\"colorFillGap\": \"support\"}}\n",
               "materialClosure.repair.colorFillGap")
        && ConfigRejectedWith(
               "material_closure_repair_mode_without_flag.json",
               ",\n  \"materialClosure\": {\"mode\": \"repair_then_report\"}\n",
               "materialClosure repair mode and enabled flag must be configured together")
        && ConfigRejectedWith(
               "material_closure_repair_flag_without_mode.json",
               ",\n  \"materialClosure\": {\"mode\": \"diagnostic\", \"repair\": {\"enabled\": true}}\n",
               "materialClosure repair mode and enabled flag must be configured together")
        && ConfigRejectedWith(
               "material_closure_repair_gap_too_large.json",
               ",\n  \"materialClosure\": {\"mode\": \"repair_then_report\", \"maxGapPx\": 2, \"repair\": {\"enabled\": true}}\n",
               "materialClosure repair supports maxGapPx=1 only");
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
        {"stage_12e_global_surface_shell_config_parses", Stage12EGlobalSurfaceShellConfigParses},
        {"stage_12e_rejects_invalid_surface_shell_fields", Stage12ERejectsInvalidSurfaceShellFields},
        {"stage_12e_rejects_mismatched_texture_and_fill", Stage12ERejectsMismatchedTextureAndFill},
        {"stage_12e_backend_unavailable_blocks_before_package_write", Stage12EBackendUnavailableBlocksBeforePackageWrite},
        {"stage_12e_unavailable_report_skeleton_is_honest", Stage12EUnavailableReportSkeletonIsHonest},
        {"empty_experimental_defaults", EmptyExperimentalDefaults},
        {"material_closure_defaults_are_diagnostic_only", MaterialClosureDefaultsAreDiagnosticOnly},
        {"material_closure_diagnostic_config_parses", MaterialClosureDiagnosticConfigParses},
        {"material_closure_slicer_config_1_parses", MaterialClosureSlicerConfig1Parses},
        {"material_closure_repair_config_parses", MaterialClosureRepairConfigParses},
        {"material_closure_rejects_invalid_configuration", MaterialClosureRejectsInvalidConfiguration},
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
