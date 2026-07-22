#include "slicer_core/preflight/ModelPreflightService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
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
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / ("slicesoft_preflight_" + std::to_string(suffix));
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

std::string ClosedBoxObj(const std::string& header = {})
{
    return header
        + "v 0 0 0\n"
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
          "f 4 5 8\n";
}

std::filesystem::path WriteConfig(
    const std::filesystem::path& directory,
    const std::string& fileName,
    const std::filesystem::path& modelPath,
    const double rotationZ = 0.0,
    const std::string& format = "obj",
    const std::string& missingTexturePolicy = "fail_fast")
{
    const std::filesystem::path configPath = directory / fileName;
    WriteText(
        configPath,
        "{\n"
        "  \"input\": {\"modelPath\": \""
            + modelPath.generic_string()
            + "\", \"format\": \"" + format + "\"},\n"
              "  \"modelTransform\": {\"unit\": \"mm\", \"scale\": [1, 1, 1], "
              "\"rotationDeg\": [0, 0, "
            + std::to_string(rotationZ)
            + "], \"translationMm\": [0, 0, 0]}\n"
              "  ,\"texture\": {\"missingTexturePolicy\": \""
            + missingTexturePolicy + "\"}\n"
              "}\n");
    return configPath;
}

bool HasIssue(
    const slicer_core::ModelPreflightExecutionResult& result,
    const slicer_core::ModelPreflightErrorCode code)
{
    const std::string expected = slicer_core::ModelPreflightErrorCodeName(code);
    for (const slicer_core::ModelPreflightIssue& issue : result.result.issues)
    {
        if (issue.code == expected)
        {
            return true;
        }
    }
    return false;
}

bool ClosedModelPassesAndCaches()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path modelPath = directory / "box.obj";
    WriteText(modelPath, ClosedBoxObj());

    slicer_core::ModelPreflightRequest request;
    request.configPath = WriteConfig(directory, "box.json", modelPath);
    request.generation = 7U;
    slicer_core::ModelPreflightService service;
    const auto first = service.Run(request);
    const auto second = service.Run(request);
    const std::string notRun = slicer_core::ModelPreflightErrorCodeName(
        slicer_core::ModelPreflightErrorCode::NotRun);

    return ExpectTrue(first.fastComplete && first.fullComplete, "both stages complete")
        && ExpectTrue(!first.cacheHit, "first run misses cache")
        && ExpectTrue(first.result.status == slicer_core::ModelPreflightStatus::Passed,
                      "closed model passes")
        && ExpectTrue(first.result.legacyAdmission.blockerCodes == std::vector<std::string>{notRun},
                      "legacy admission stays deferred")
        && ExpectTrue(first.result.globalAdmission.blockerCodes == std::vector<std::string>{notRun},
                      "global admission stays deferred")
        && ExpectTrue(second.cacheHit, "second run hits cache")
        && ExpectTrue(first.result.cacheKey == second.result.cacheKey,
                      "cache key is deterministic")
        && ExpectTrue(service.CacheSize() == 1U, "one result is cached");
}

bool IdentityChangesInvalidateCache()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path modelPath = directory / "box.obj";
    const std::filesystem::path materialPath = directory / "material.mtl";
    WriteText(materialPath, "newmtl base\nKd 1 0 0\n");
    WriteText(modelPath, ClosedBoxObj("mtllib material.mtl\nusemtl base\n"));

    const auto baseConfig = WriteConfig(directory, "base.json", modelPath);
    const auto rotatedConfig = WriteConfig(directory, "rotated.json", modelPath, 15.0);
    slicer_core::ModelPreflightService service;
    slicer_core::ModelPreflightRequest request;
    request.configPath = baseConfig;
    const auto base = service.Run(request);

    WriteText(materialPath, "newmtl base\nKd 0 1 0\n");
    const auto resourceChanged = service.Run(request);
    request.configPath = rotatedConfig;
    const auto transformChanged = service.Run(request);
    request.configPath = baseConfig;
    request.options.voxelMm = 0.08;
    const auto optionsChanged = service.Run(request);
    WriteText(modelPath, ClosedBoxObj("# changed\nmtllib material.mtl\nusemtl base\n"));
    const auto sourceChanged = service.Run(request);

    return ExpectTrue(base.result.status == slicer_core::ModelPreflightStatus::Passed,
                      "identity baseline passes")
        && ExpectTrue(base.result.cacheKey != resourceChanged.result.cacheKey,
                      "resource changes cache key")
        && ExpectTrue(resourceChanged.result.cacheKey != transformChanged.result.cacheKey,
                      "transform changes cache key")
        && ExpectTrue(resourceChanged.result.cacheKey != optionsChanged.result.cacheKey,
                      "options change cache key")
        && ExpectTrue(optionsChanged.result.cacheKey != sourceChanged.result.cacheKey,
                      "source changes cache key")
        && ExpectTrue(!resourceChanged.cacheHit && !transformChanged.cacheHit
                          && !optionsChanged.cacheHit && !sourceChanged.cacheHit,
                      "all identity changes miss cache");
}

bool FatalAndIncompleteInputsNeverPass()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path missingModel = directory / "missing.obj";
    WriteText(missingModel, ClosedBoxObj("mtllib absent.mtl\nusemtl base\n"));
    slicer_core::ModelPreflightRequest missingRequest;
    missingRequest.configPath = WriteConfig(directory, "missing.json", missingModel);

    const std::filesystem::path boxModel = directory / "box.obj";
    WriteText(boxModel, ClosedBoxObj());
    slicer_core::ModelPreflightRequest auditRequest;
    auditRequest.configPath = WriteConfig(directory, "audit.json", boxModel);
    auditRequest.options.maxCompleteSelfIntersectionCandidatePairs = 1U;

    slicer_core::ModelPreflightService service;
    const auto missing = service.Run(missingRequest);
    const auto incomplete = service.Run(auditRequest);

    return ExpectTrue(missing.fastComplete && !missing.fullComplete,
                      "missing resource stops after fast stage")
        && ExpectTrue(missing.result.status == slicer_core::ModelPreflightStatus::Blocked,
                      "missing resource blocks")
        && ExpectTrue(HasIssue(missing, slicer_core::ModelPreflightErrorCode::ResourceMissing),
                      "resource issue uses stable code")
        && ExpectTrue(incomplete.fullComplete, "incomplete audit invoked full stage")
        && ExpectTrue(incomplete.result.status == slicer_core::ModelPreflightStatus::Blocked,
                      "incomplete audit blocks")
        && ExpectTrue(HasIssue(incomplete, slicer_core::ModelPreflightErrorCode::AuditIncomplete),
                      "audit issue uses stable code")
        && ExpectTrue(service.CacheSize() == 0U, "fatal results are not cached");
}

bool ResourceFallbackPolicyIsPartOfIdentity()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path modelPath = directory / "missing_mtl.obj";
    WriteText(modelPath, ClosedBoxObj("mtllib absent.mtl\nusemtl base\n"));
    const auto warningConfig = WriteConfig(
        directory,
        "warning.json",
        modelPath,
        0.0,
        "obj",
        "warn_and_fallback");
    const auto blockedConfig = WriteConfig(
        directory,
        "blocked.json",
        modelPath,
        0.0,
        "obj",
        "fail_fast");

    slicer_core::ModelPreflightService service;
    slicer_core::ModelPreflightRequest request;
    request.configPath = warningConfig;
    const auto warning = service.Run(request);
    request.configPath = blockedConfig;
    const auto blocked = service.Run(request);

    return ExpectTrue(warning.fullComplete, "fallback policy allows full diagnostics")
        && ExpectTrue(warning.result.status == slicer_core::ModelPreflightStatus::Warning,
                      "fallback policy preserves resource warning")
        && ExpectTrue(HasIssue(warning, slicer_core::ModelPreflightErrorCode::ResourceMissing),
                      "fallback warning has resource code")
        && ExpectTrue(blocked.result.status == slicer_core::ModelPreflightStatus::Blocked,
                      "fail-fast policy blocks")
        && ExpectTrue(!blocked.cacheHit, "fail-fast policy does not reuse warning cache")
        && ExpectTrue(warning.result.cacheKey != blocked.result.cacheKey,
                      "resource policy participates in cache identity");
}

bool InvalidAndNonFiniteInputsUseStableCodes()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path nonFiniteModel = directory / "non_finite.obj";
    std::string nonFiniteObj = ClosedBoxObj();
    nonFiniteObj.replace(0U, std::string{"v 0 0 0"}.size(), "v nan 0 0");
    WriteText(nonFiniteModel, nonFiniteObj);

    slicer_core::ModelPreflightRequest nonFiniteRequest;
    nonFiniteRequest.configPath = WriteConfig(
        directory,
        "non_finite.json",
        nonFiniteModel);
    slicer_core::ModelPreflightRequest invalidRequest;
    invalidRequest.configPath = directory / "missing_config.json";

    slicer_core::ModelPreflightService service;
    const auto nonFinite = service.Run(nonFiniteRequest);
    const auto invalid = service.Run(invalidRequest);

    return ExpectTrue(nonFinite.result.status == slicer_core::ModelPreflightStatus::Blocked,
                      "non-finite input blocks")
        && ExpectTrue(
            HasIssue(nonFinite, slicer_core::ModelPreflightErrorCode::NonFiniteGeometry),
            "non-finite input has stable code")
        && ExpectTrue(invalid.result.status == slicer_core::ModelPreflightStatus::Blocked,
                      "invalid import blocks")
        && ExpectTrue(HasIssue(invalid, slicer_core::ModelPreflightErrorCode::ImportInvalid),
                      "invalid import has stable code");
}

bool CancelledAndStaleResultsAreNotCached()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path modelPath = directory / "box.obj";
    WriteText(modelPath, ClosedBoxObj());
    const auto configPath = WriteConfig(directory, "box.json", modelPath);

    slicer_core::ModelPreflightService service;
    slicer_core::ModelPreflightRequest cancelledRequest;
    cancelledRequest.configPath = configPath;
    cancelledRequest.cancellationRequested = []()
    {
        return true;
    };
    const auto cancelled = service.Run(cancelledRequest);

    std::size_t checkpoint{0U};
    slicer_core::ModelPreflightRequest staleRequest;
    staleRequest.configPath = configPath;
    staleRequest.cancellationRequested = [&]()
    {
        ++checkpoint;
        if (checkpoint == 3U)
        {
            std::ofstream output{modelPath, std::ios::app};
            output << "# modified during preflight\n";
        }
        return false;
    };
    const auto stale = service.Run(staleRequest);

    return ExpectTrue(cancelled.cancelled, "cancel state is exposed")
        && ExpectTrue(cancelled.result.status == slicer_core::ModelPreflightStatus::Cancelled,
                      "cancelled result has stable status")
        && ExpectTrue(HasIssue(cancelled, slicer_core::ModelPreflightErrorCode::Cancelled),
                      "cancelled result has stable code")
        && ExpectTrue(stale.stale, "runtime mutation marks stale")
        && ExpectTrue(stale.result.status == slicer_core::ModelPreflightStatus::Stale,
                      "stale result cannot pass")
        && ExpectTrue(HasIssue(stale, slicer_core::ModelPreflightErrorCode::Stale),
                      "stale result has stable code")
        && ExpectTrue(service.CacheSize() == 0U, "cancelled and stale results are not cached");
}

bool CancellationWorksAtStageBoundaries()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path modelPath = directory / "box.obj";
    WriteText(modelPath, ClosedBoxObj());
    const auto configPath = WriteConfig(directory, "box.json", modelPath);

    slicer_core::ModelPreflightService service;
    std::size_t beforeFullCheckpoint{0U};
    slicer_core::ModelPreflightRequest beforeFull;
    beforeFull.configPath = configPath;
    beforeFull.cancellationRequested = [&]()
    {
        ++beforeFullCheckpoint;
        return beforeFullCheckpoint == 2U;
    };
    const auto cancelledBeforeFull = service.Run(beforeFull);

    std::size_t afterFullCheckpoint{0U};
    slicer_core::ModelPreflightRequest afterFull;
    afterFull.configPath = configPath;
    afterFull.cancellationRequested = [&]()
    {
        ++afterFullCheckpoint;
        return afterFullCheckpoint == 3U;
    };
    const auto cancelledAfterFull = service.Run(afterFull);

    return ExpectTrue(cancelledBeforeFull.fastComplete && !cancelledBeforeFull.fullComplete,
                      "cancellation before full preserves stage state")
        && ExpectTrue(cancelledBeforeFull.cancelled, "before-full cancellation is reported")
        && ExpectTrue(cancelledAfterFull.fastComplete && cancelledAfterFull.fullComplete,
                      "cancellation after full preserves stage state")
        && ExpectTrue(cancelledAfterFull.cancelled, "after-full cancellation is reported")
        && ExpectTrue(service.CacheSize() == 0U, "stage cancellations are not cached");
}

bool RealCleanInputsPassSharedDiagnostics()
{
    const std::filesystem::path sourceRoot{SLICESOFT_SOURCE_DIR};
    const std::filesystem::path directory = MakeTestDirectory();
    const std::vector<std::pair<std::filesystem::path, std::string>> inputs{
        {sourceRoot / "model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj", "obj"},
        {sourceRoot / "model/obj/yecan/3.obj", "obj"},
        {sourceRoot / "samples/models/3mf/texture2d_checker_cube.3mf", "3mf"},
    };

    slicer_core::ModelPreflightService service;
    bool passed{true};
    for (std::size_t index{0U}; index < inputs.size(); ++index)
    {
        slicer_core::ModelPreflightRequest request;
        request.configPath = WriteConfig(
            directory,
            "real_" + std::to_string(index) + ".json",
            inputs.at(index).first,
            0.0,
            inputs.at(index).second);
        const auto result = service.Run(request);
        passed = ExpectTrue(
                     result.result.status == slicer_core::ModelPreflightStatus::Passed,
                     "real clean input passes shared diagnostics")
            && ExpectTrue(result.fullComplete, "real clean input completes full audit")
            && passed;
    }
    return passed;
}

}  // namespace

int main()
{
    const std::vector<TestCase> tests{
        {"closed_model_passes_and_caches", ClosedModelPassesAndCaches},
        {"identity_changes_invalidate_cache", IdentityChangesInvalidateCache},
        {"fatal_and_incomplete_inputs_never_pass", FatalAndIncompleteInputsNeverPass},
        {"resource_fallback_policy_is_part_of_identity", ResourceFallbackPolicyIsPartOfIdentity},
        {"invalid_and_non_finite_inputs_use_stable_codes", InvalidAndNonFiniteInputsUseStableCodes},
        {"cancelled_and_stale_results_are_not_cached", CancelledAndStaleResultsAreNotCached},
        {"cancellation_works_at_stage_boundaries", CancellationWorksAtStageBoundaries},
        {"real_clean_inputs_pass_shared_diagnostics", RealCleanInputsPassSharedDiagnostics},
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
