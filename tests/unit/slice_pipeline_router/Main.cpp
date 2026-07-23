#include "slicer_core/config.h"
#include "slicer_core/pipeline/SlicePipeline.h"
#include "slicer_core/pipeline/SlicePipelineRouter.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

struct TestCase
{
    std::string name;
    std::function<bool()> run;
};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::filesystem::path MakeTestDirectory()
{
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / ("slicesoft_pipeline_router_" + std::to_string(suffix));
    std::filesystem::create_directories(directory);
    return directory;
}

void WriteText(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream output{path, std::ios::binary};
    if (!output)
    {
        throw std::runtime_error("failed to write fixture: " + path.string());
    }
    output << content;
}

std::filesystem::path WriteBoxModel(const std::filesystem::path& directory)
{
    const std::filesystem::path modelPath = directory / "box.obj";
    WriteText(
        modelPath,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "v 0 0 1\n"
        "v 1 0 1\n"
        "v 1 1 1\n"
        "v 0 1 1\n"
        "f 1 3 2\n"
        "f 1 4 3\n"
        "f 5 6 7\n"
        "f 5 7 8\n"
        "f 1 2 6\n"
        "f 1 6 5\n"
        "f 2 3 7\n"
        "f 2 7 6\n"
        "f 3 4 8\n"
        "f 3 8 7\n"
        "f 4 1 5\n"
        "f 4 5 8\n");
    return modelPath;
}

std::filesystem::path WriteOpenModel(const std::filesystem::path& directory)
{
    const std::filesystem::path modelPath = directory / "open.obj";
    WriteText(
        modelPath,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n");
    return modelPath;
}

std::filesystem::path WriteConfig(
    const std::filesystem::path& directory,
    const std::filesystem::path& modelPath,
    const std::filesystem::path& packagePath,
    const std::string& pipelineJson,
    const bool globalMaterialSemantics)
{
    const std::filesystem::path configPath = directory / "slice.json";
    const std::string textureJson = globalMaterialSemantics
        ? "  \"texture\": {\"enabled\": true, \"applyMode\": "
          "\"global_surface_shell\", \"surfaceShell\": {\"widthMm\": 0.10}},\n"
          "  \"modelFill\": {\"enabled\": true, \"material\": \"white\", "
          "\"scope\": \"complement_of_global_texture_shell\"},\n"
        : "";
    const std::string pipelineEntry = pipelineJson.empty()
        ? ""
        : "  \"slicePipeline\": " + pipelineJson + ",\n";
    WriteText(
        configPath,
        "{\n"
            + pipelineEntry
            + "  \"input\": {\"modelPath\": \""
            + modelPath.generic_string()
            + "\", \"format\": \"obj\"},\n"
              "  \"output\": {\"packageDir\": \""
            + packagePath.generic_string()
            + "\"},\n"
            + textureJson
            + "  \"modelTransform\": {\"unit\": \"mm\", \"scale\": [1, 1, 1], "
              "\"rotationDeg\": [0, 0, 0], \"translationMm\": [0, 0, 0]}\n"
              "}\n");
    return configPath;
}

bool OmittedModeDefaultsToLegacy()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path modelPath = WriteBoxModel(directory);
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(
        WriteConfig(directory, modelPath, directory / "package", "", false));

    slicer_core::SlicePipelineRouteContext context;
    const slicer_core::SlicePipelineRouteDecision decision =
        slicer_core::ResolveSlicePipelineRoute(config.slice_pipeline, context);
    return ExpectTrue(
               config.slice_pipeline.mode == slicer_core::SlicePipelineMode::Legacy,
               "omitted slicePipeline defaults to legacy")
        && ExpectTrue(
            !config.slice_pipeline.explicitly_configured,
            "omitted slicePipeline remains distinguishable for compatibility")
        && ExpectTrue(decision.allowed, "legacy route is admitted")
        && ExpectTrue(
            decision.effective_mode == slicer_core::SlicePipelineMode::Legacy,
            "legacy route remains effective legacy")
        && ExpectTrue(!decision.fallback_applied, "legacy route is not a fallback");
}

bool ExplicitLegacyRoutesWithoutFallback()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path modelPath = WriteBoxModel(directory);
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(
        WriteConfig(
            directory,
            modelPath,
            directory / "package",
            "{\"mode\": \"legacy\"}",
            false));

    const slicer_core::SlicePipelineRouteDecision decision =
        slicer_core::ResolveSlicePipelineRoute(
            config.slice_pipeline,
            slicer_core::SlicePipelineRouteContext{});
    return ExpectTrue(
               config.slice_pipeline.explicitly_configured,
               "explicit legacy mode is recorded")
        && ExpectTrue(decision.allowed, "explicit legacy is admitted")
        && ExpectTrue(!decision.fallback_applied, "explicit legacy is direct");
}

bool UnsupportedModeHasStableError()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path modelPath = WriteBoxModel(directory);
    try
    {
        (void)slicer_core::load_slice_config(
            WriteConfig(
                directory,
                modelPath,
                directory / "package",
                "{\"mode\": \"automatic\"}",
                false));
    }
    catch (const slicer_core::SlicePipelineError& error)
    {
        return ExpectTrue(
                   error.Code()
                       == slicer_core::SlicePipelineErrorCode::ModeUnsupported,
                   "unsupported mode preserves typed error")
            && ExpectTrue(
                std::string{error.what()}.find(
                    "E_12E_PIPELINE_MODE_UNSUPPORTED")
                    != std::string::npos,
                "unsupported mode exposes stable error code");
    }
    return ExpectTrue(false, "unsupported mode must fail closed");
}

bool MismatchedModeAndMaterialSemanticsFailClosed()
{
    const std::filesystem::path legacyDirectory = MakeTestDirectory();
    const std::filesystem::path legacyModel = WriteBoxModel(legacyDirectory);
    bool legacyRejected{false};
    try
    {
        (void)slicer_core::load_slice_config(
            WriteConfig(
                legacyDirectory,
                legacyModel,
                legacyDirectory / "package",
                "{\"mode\": \"legacy\"}",
                true));
    }
    catch (const slicer_core::SlicePipelineError& error)
    {
        legacyRejected =
            error.Code() == slicer_core::SlicePipelineErrorCode::ConfigMismatch;
    }

    const std::filesystem::path globalDirectory = MakeTestDirectory();
    const std::filesystem::path globalModel = WriteBoxModel(globalDirectory);
    bool globalRejected{false};
    try
    {
        (void)slicer_core::load_slice_config(
            WriteConfig(
                globalDirectory,
                globalModel,
                globalDirectory / "package",
                "{\"mode\": \"global_surface_shell\"}",
                false));
    }
    catch (const slicer_core::SlicePipelineError& error)
    {
        globalRejected =
            error.Code() == slicer_core::SlicePipelineErrorCode::ConfigMismatch;
    }

    return ExpectTrue(
               legacyRejected,
               "explicit legacy rejects global-only material semantics")
        && ExpectTrue(
            globalRejected,
            "global mode rejects legacy material semantics");
}

bool GlobalRouteNeverFallsBack()
{
    slicer_core::SlicePipelineConfig config;
    config.mode = slicer_core::SlicePipelineMode::GlobalSurfaceShell;
    config.explicitly_configured = true;

    slicer_core::SlicePipelineRouteContext topologyContext;
    topologyContext.global_topology_blocked = true;
    const auto topologyDecision = slicer_core::ResolveSlicePipelineRoute(
        config,
        topologyContext);

    slicer_core::SlicePipelineRouteContext unavailableContext;
    unavailableContext.global_preflight_admitted = true;
    const auto unavailableDecision = slicer_core::ResolveSlicePipelineRoute(
        config,
        unavailableContext);

    return ExpectTrue(
               !topologyDecision.allowed,
               "topology blocker rejects global route")
        && ExpectTrue(
            topologyDecision.error_code
                == slicer_core::SlicePipelineErrorCode::GlobalTopologyBlocked,
            "topology blocker has stable pipeline code")
        && ExpectTrue(
            topologyDecision.effective_mode
                == slicer_core::SlicePipelineMode::GlobalSurfaceShell,
            "blocked global request is not rewritten to legacy")
        && ExpectTrue(
            !topologyDecision.fallback_applied,
            "topology blocker applies no fallback")
        && ExpectTrue(
            unavailableDecision.error_code
                == slicer_core::SlicePipelineErrorCode::GlobalNotAdmitted,
            "missing production adapter has stable admission code")
        && ExpectTrue(
            !unavailableDecision.fallback_applied,
            "missing production adapter applies no fallback");
}

bool CleanGlobalModelStopsBeforeWriter()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path packagePath = directory / "package";
    const std::filesystem::path configPath = WriteConfig(
        directory,
        WriteBoxModel(directory),
        packagePath,
        "{\"mode\": \"global_surface_shell\"}",
        true);

    try
    {
        (void)slicer_core::RunSlicePipeline(
            configPath,
            slicer_core::SliceRunOptions{});
    }
    catch (const slicer_core::SlicePipelineError& error)
    {
        return ExpectTrue(
                   error.Code()
                       == slicer_core::SlicePipelineErrorCode::GlobalNotAdmitted,
                   "clean global request stops at the unavailable production adapter")
            && ExpectTrue(
                !std::filesystem::exists(packagePath),
                "unavailable global writer creates no package");
    }
    return ExpectTrue(false, "08D-01 global route must not publish production output");
}

bool TopologyBlockedGlobalModelStopsBeforeWriter()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path packagePath = directory / "package";
    const std::filesystem::path configPath = WriteConfig(
        directory,
        WriteOpenModel(directory),
        packagePath,
        "{\"mode\": \"global_surface_shell\"}",
        true);

    try
    {
        (void)slicer_core::RunSlicePipeline(
            configPath,
            slicer_core::SliceRunOptions{});
    }
    catch (const slicer_core::SlicePipelineError& error)
    {
        return ExpectTrue(
                   error.Code()
                       == slicer_core::SlicePipelineErrorCode::GlobalTopologyBlocked,
                   "open global model maps to the stable topology blocker")
            && ExpectTrue(
                std::string{error.what()}.find(
                    "E_12E_PREFLIGHT_GLOBAL_TOPOLOGY_BLOCKED")
                    != std::string::npos,
                "pipeline error retains detailed preflight blocker")
            && ExpectTrue(
                !std::filesystem::exists(packagePath),
                "topology-blocked global request creates no package");
    }
    return ExpectTrue(false, "topology-blocked global request must fail closed");
}

}  // namespace

int main()
{
    const std::vector<TestCase> tests{
        {"omitted_mode_defaults_to_legacy", OmittedModeDefaultsToLegacy},
        {"explicit_legacy_routes_without_fallback", ExplicitLegacyRoutesWithoutFallback},
        {"unsupported_mode_has_stable_error", UnsupportedModeHasStableError},
        {"mismatched_mode_and_material_semantics_fail_closed", MismatchedModeAndMaterialSemanticsFailClosed},
        {"global_route_never_falls_back", GlobalRouteNeverFallsBack},
        {"clean_global_model_stops_before_writer", CleanGlobalModelStopsBeforeWriter},
        {"topology_blocked_global_model_stops_before_writer", TopologyBlockedGlobalModelStopsBeforeWriter},
    };

    bool passed{true};
    for (const TestCase& test : tests)
    {
        const bool current = test.run();
        std::cout << (current ? "PASS: " : "FAIL: ") << test.name << '\n';
        passed = current && passed;
    }
    return passed ? 0 : 1;
}
