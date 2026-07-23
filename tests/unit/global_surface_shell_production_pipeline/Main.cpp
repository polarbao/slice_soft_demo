#include "slicer_core/json_value.h"
#include "slicer_core/pipeline/GlobalSurfaceShellProductionPipeline.h"
#include "slicer_core/pipeline/SlicePipeline.h"
#include "slicer_core/rip_reader.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::filesystem::path MakeTestDirectory(const std::string& name)
{
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / ("slicesoft_global_pipeline_" + name + "_"
           + std::to_string(suffix));
    std::filesystem::create_directories(directory);
    return directory;
}

slicer_core::SliceConfig MakeAdmittedProfile()
{
    slicer_core::SliceConfig config;
    config.slice_pipeline.mode =
        slicer_core::SlicePipelineMode::GlobalSurfaceShell;
    config.slice_pipeline.explicitly_configured = true;
    config.texture.enabled = true;
    config.texture.apply_mode = "global_surface_shell";
    config.texture.surface_shell.width_mm = 0.2;
    config.model_fill.enabled = true;
    config.model_fill.material = "white";
    config.model_fill.scope = "complement_of_global_texture_shell";
    config.material_process_profile.enabled = true;
    config.material_process_profile.name =
        "global_surface_shell_restricted_candidate";
    config.material_process_profile.target =
        "global_surface_shell_restricted_candidate";
    config.material_process_profile.white.enabled = true;
    config.material_process_profile.support.expected = false;
    config.support.enabled = false;
    config.surface_varnish.enabled = false;
    config.outer_varnish.enabled = false;
    return config;
}

std::filesystem::path WriteConfig(
    const std::filesystem::path& directory,
    const bool supportEnabled)
{
    const std::filesystem::path modelPath =
        std::filesystem::absolute(
            "samples/models/openvdb_candidate/closed_textured_obj.obj");
    const std::filesystem::path packageDir = directory / "package";
    const std::filesystem::path configPath = directory / "config.json";
    const slicer_core::Json config = slicer_core::Json::object({
        {"slicePipeline",
         slicer_core::Json::object({{"mode", "global_surface_shell"}})},
        {"input",
         slicer_core::Json::object({
             {"modelPath", modelPath.generic_string()},
             {"format", "obj"},
         })},
        {"output",
         slicer_core::Json::object({
             {"packageDir", packageDir.generic_string()},
             {"dpiX", 600},
             {"dpiY", 600},
             {"layerThicknessMm", 0.1},
             {"storageMode", "stripped"},
             {"rowsPerStrip", 8},
         })},
        {"autoOrient",
         slicer_core::Json::object({
             {"enabled", true},
             {"maxHeightMm", 6.0},
             {"strategy", "minimize_height_by_right_angle_rotation"},
         })},
        {"texture",
         slicer_core::Json::object({
             {"enabled", true},
             {"applyMode", "global_surface_shell"},
             {"fallbackRgb", slicer_core::Json::array({20, 40, 60})},
             {"missingTexturePolicy", "warn_and_fallback"},
             {"surfaceShell",
              slicer_core::Json::object({
                  {"geometryMode", "global_3d_distance"},
                  {"widthMm", 0.2},
                  {"widthStepMm", 0.01},
                  {"minimumWidthPolicy", "two_cells_floor_0_10_mm"},
                  {"surfaceScope", "all_closed_surfaces"},
                  {"fullTextureAtModelLimit", true},
              })},
         })},
        {"modelFill",
         slicer_core::Json::object({
             {"enabled", true},
             {"material", "white"},
             {"scope", "complement_of_global_texture_shell"},
             {"value", 0},
             {"emptyAllowedInProduction", false},
         })},
        {"materialProcessProfile",
         slicer_core::Json::object({
             {"enabled", true},
             {"name", "global_surface_shell_restricted_candidate"},
             {"target", "global_surface_shell_restricted_candidate"},
             {"rgb",
              slicer_core::Json::object({
                  {"enabled", true},
                  {"source", "texture_or_color"},
              })},
             {"white",
              slicer_core::Json::object({
                  {"enabled", true},
                  {"mode", "underbase"},
                  {"coverage", "all_model"},
                  {"value", 0},
                  {"expandPx", 0},
                  {"shrinkPx", 0},
              })},
             {"support",
              slicer_core::Json::object({
                  {"expected", false},
                  {"mode", "existing_support_pipeline"},
              })},
         })},
        {"support",
         slicer_core::Json::object({{"enabled", supportEnabled}})},
        {"surfaceVarnish",
         slicer_core::Json::object({{"enabled", false}})},
        {"outerVarnish",
         slicer_core::Json::object({
             {"enabled", false},
             {"thicknessMm", 0.0},
         })},
        {"preview",
         slicer_core::Json::object({
             {"enabled", true},
             {"format", "ppm"},
             {"interval", 1},
         })},
    });

    std::ofstream output{configPath};
    output << config.dump(2) << '\n';
    return configPath;
}

bool ExplicitRestrictedProfileIsAdmitted()
{
    const auto decision =
        slicer_core::EvaluateGlobalSurfaceShellProductionProfile(
            MakeAdmittedProfile());
    return ExpectTrue(decision.allowed, "explicit restricted profile is admitted")
        && ExpectTrue(
            decision.productionAcceptance == "admitted",
            "profile records admitted production");
}

bool SupportEnabledProfileIsBlocked()
{
    auto config = MakeAdmittedProfile();
    config.support.enabled = true;
    const auto decision =
        slicer_core::EvaluateGlobalSurfaceShellProductionProfile(config);
    return ExpectTrue(!decision.allowed, "unsupported Global support is blocked")
        && ExpectTrue(
            decision.detail.find("support") != std::string::npos,
            "support blocker is auditable");
}

bool ExplicitProfileWritesProductionPackage()
{
    const std::filesystem::path directory =
        MakeTestDirectory("production");
    const std::filesystem::path configPath =
        WriteConfig(directory, false);
    try
    {
        const slicer_core::SliceRunResult result =
            slicer_core::RunSlicePipeline(
                configPath,
                slicer_core::SliceRunOptions{});
        const slicer_core::RipValidationResult rip =
            slicer_core::validate_slice_package(result.package_dir);
        std::ifstream manifestInput{result.package_dir / "manifest.json"};
        const slicer_core::Json manifest =
            slicer_core::Json::parse(manifestInput);

        return ExpectTrue(
                   std::filesystem::exists(result.package_dir / "manifest.json"),
                   "Global package manifest exists")
            && ExpectTrue(
                result.effective_pipeline_mode == "global_surface_shell",
                "run result records effective Global mode")
            && ExpectTrue(
                manifest.at("requestedPipelineMode").as_string()
                    == "global_surface_shell",
                "manifest records requested Global mode")
            && ExpectTrue(
                manifest.at("productionAcceptance").as_string() == "admitted",
                "manifest records admitted production")
            && ExpectTrue(
                !manifest.at("fallbackApplied").as_bool(),
                "Global production does not fallback")
            && ExpectTrue(
                rip.layer_count == result.layer_count,
                "RIP validates every Global layer")
            && ExpectTrue(
                rip.total_channel_stats.at(3U).print_pixels > 0U,
                "Global model fill produces white channel pixels");
    }
    catch (const std::exception& error)
    {
        return ExpectTrue(
            false,
            "Global production package failed: " + std::string{error.what()});
    }
}

bool UnsupportedProfileWritesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("blocked_support");
    const std::filesystem::path configPath =
        WriteConfig(directory, true);
    try
    {
        (void)slicer_core::RunSlicePipeline(
            configPath,
            slicer_core::SliceRunOptions{});
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(directory / "package"),
            "blocked Global profile writes no package");
    }
    return ExpectTrue(false, "unsupported Global profile must fail closed");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"explicit_restricted_profile_is_admitted", ExplicitRestrictedProfileIsAdmitted},
        {"support_enabled_profile_is_blocked", SupportEnabledProfileIsBlocked},
        {"explicit_profile_writes_production_package", ExplicitProfileWritesProductionPackage},
        {"unsupported_profile_writes_nothing", UnsupportedProfileWritesNothing},
    };

    bool passed{true};
    for (const auto& test : tests)
    {
        std::cout << "RUN: " << test.first << std::endl;
        const bool current = test.second();
        std::cout << (current ? "PASS: " : "FAIL: ")
                  << test.first << std::endl;
        passed = current && passed;
    }
    return passed ? 0 : 1;
}
