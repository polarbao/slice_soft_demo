#pragma once

#include "slicer_module/WorkerClient.h"

#include "slicer_core/api/artifacts/PackageArtifactSafety.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneModel.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace stage14d07
{

namespace artifacts = slicer_core::api::artifacts;
namespace module = slicesoft::module;

struct WorkerJobFiles
{
    std::filesystem::path requestPath;
    std::filesystem::path cancelPath;
    std::optional<artifacts::PackageArtifactIdentity> artifactIdentity;
};

struct WorkerOutcome
{
    module::WorkerRunResult run;
    std::optional<slicer_core::Json> result;
};

struct Fixture
{
    slicer_core::SceneModel model;
    slicer_core::MultiModelScene scene;
    std::filesystem::path modelPath;
};

extern std::filesystem::path g_repoRoot;
extern std::filesystem::path g_workerPath;
extern std::filesystem::path g_evidenceRoot;
extern std::filesystem::path g_runRoot;
extern std::map<std::string, bool> g_themeStatus;

std::string ReadBytes(const std::filesystem::path& path);
slicer_core::Json ReadJson(const std::filesystem::path& path);

void WriteTheme(
    const std::string& id,
    const std::string& fileName,
    bool passed,
    slicer_core::Json::Object details);

slicer_core::Json MakeProfile(
    const std::filesystem::path& modelPath,
    const std::string& format,
    const std::filesystem::path& packageDirectory,
    double layerThicknessMm,
    const std::string& profileName);

Fixture LoadFixture(
    const std::filesystem::path& modelPath,
    const std::string& format,
    const std::string& identity);

WorkerJobFiles WriteRequest(
    const std::string& jobId,
    const slicer_core::Json& request,
    std::optional<artifacts::PackageArtifactIdentity> artifactIdentity =
        std::nullopt);

WorkerOutcome RunWorker(
    module::WorkerClient& client,
    const std::filesystem::path& workerPath,
    const WorkerJobFiles& files,
    bool requireProgress);

bool HasIdentity(
    const WorkerOutcome& outcome,
    const std::string& jobId,
    const std::string& correlationId,
    const std::string& capability,
    bool expectedOk);

std::string OutcomeCode(const WorkerOutcome& outcome);

slicer_core::Json MakeSliceRequest(
    const std::string& jobId,
    const std::string& correlationId,
    const Fixture& fixture,
    const slicer_core::Json& profile,
    const std::filesystem::path& packageDirectory,
    const std::optional<std::string>& sceneHashOverride = std::nullopt);

slicer_core::Json MakePreflightRequest(
    const std::string& jobId,
    const std::string& correlationId,
    const Fixture& fixture,
    const slicer_core::Json& profile,
    const std::optional<std::string>& sceneHashOverride = std::nullopt);

slicer_core::Json MakeRepairRequest(
    const std::string& jobId,
    const std::string& correlationId,
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& outputPath,
    const slicer_core::Json& profile);

std::vector<std::pair<std::string, std::string>> LayerHashes(
    const std::filesystem::path& packageDirectory);

slicer_core::Json StableReportProjection(
    const std::filesystem::path& packageDirectory);

bool IsMonotonicProgress(const module::WorkerRunResult& run);

}  // namespace stage14d07
