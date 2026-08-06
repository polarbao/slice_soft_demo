#include "EngineConformanceSupport.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/api/artifacts/PackageArtifactSafety.h"
#include "slicer_core/config.h"
#include "slicer_core/model.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace stage14d07
{

using namespace std::chrono_literals;

std::filesystem::path g_repoRoot;
std::filesystem::path g_workerPath;
std::filesystem::path g_evidenceRoot;
std::filesystem::path g_runRoot;
std::map<std::string, bool> g_themeStatus;

std::string ReadBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to read file: " + path.generic_string());
    }
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to read JSON: " + path.generic_string());
    }
    return slicer_core::Json::parse(input);
}

void WriteJson(
    const std::filesystem::path& path,
    const slicer_core::Json& document)
{
    std::filesystem::create_directories(path.parent_path());
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            throw std::runtime_error(
                "failed to create JSON: " + path.generic_string());
        }
        output << document.dump(2) << '\n';
    }
    std::error_code error;
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error)
    {
        throw std::runtime_error(
            "failed to publish JSON: " + error.message());
    }
}

std::string HashFile(const std::filesystem::path& path)
{
    return slicer_core::ComputeSha256(ReadBytes(path));
}

void WriteTheme(
    const std::string& id,
    const std::string& fileName,
    const bool passed,
    slicer_core::Json::Object details)
{
    details.emplace("schema", "slicesoft.engine_conformance.theme.14d_07.1");
    details.emplace("theme", id);
    details.emplace("status", passed ? "pass" : "fail");
    WriteJson(g_evidenceRoot / fileName, slicer_core::Json{std::move(details)});
    g_themeStatus[id] = passed;
}

slicer_core::Json MakeProfile(
    const std::filesystem::path& modelPath,
    const std::string& format,
    const std::filesystem::path& packageDirectory,
    const double layerThicknessMm,
    const std::string& profileName)
{
    slicer_core::Json::Object profile{
        {"profileVersion", "1.0"},
        {"slicingMode", "closed_mesh_scanline"},
        {"autoOrient", slicer_core::Json::object({
            {"enabled", false},
            {"maxHeightMm", 9.0},
        })},
        {"slicePipeline", slicer_core::Json::object({{"mode", "legacy"}})},
        {"input", slicer_core::Json::object({
            {"modelPath", modelPath.generic_string()},
            {"format", format},
        })},
        {"output", slicer_core::Json::object({
            {"packageDir", packageDirectory.generic_string()},
            {"dpiX", 127},
            {"dpiY", 127},
            {"layerThicknessMm", layerThicknessMm},
            {"channelOrder", slicer_core::Json::array(
                {"R", "G", "B", "W", "S", "V"})},
            {"bitDepth", 8},
            {"planarConfig", "contiguous"},
            {"storageMode", "stripped"},
            {"rowsPerStrip", 64},
        })},
        {"preview", slicer_core::Json::object({{"enabled", false}})},
        {"background", slicer_core::Json::object({{"value", 255}})},
        {"modelMaterial", slicer_core::Json::object({
            {"materialChannel", "RGB"},
            {"applyMode", "solid_volume"},
            {"rgb", slicer_core::Json::array({0, 0, 0})},
            {"whiteValue", 255},
            {"varnishValue", 255},
        })},
        {"materialProcessProfile", slicer_core::Json::object({
            {"enabled", true},
            {"name", profileName},
            {"target", "stage14d07-fixture"},
        })},
    };
    profile.emplace(
        "profileHash",
        slicer_core::api::ComputeProfileDocumentHash(
            slicer_core::Json{profile}));
    return slicer_core::Json{std::move(profile)};
}

Fixture LoadFixture(
    const std::filesystem::path& modelPath,
    const std::string& format,
    const std::string& identity)
{
    slicer_core::SliceConfig loadConfig;
    loadConfig.input.model_path = modelPath;
    loadConfig.input.format = format;
    loadConfig.auto_orient.enabled = false;
    const slicer_core::SceneModel model = slicer_core::load_model_report(
        loadConfig, modelPath.parent_path());

    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-stage14d07-" + identity;
    scene.scenerevision = 1U;
    scene.buildvolume = slicer_core::MakeDefaultDeviceBuildVolume();
    scene.resolvedprofileid = "profile-stage14d07-" + identity;

    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-stage14d07-" + identity;
    scope.kind = format == "3mf"
        ? slicer_core::ResourceScopeKind::ThreeMfPackage
        : slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = format == "3mf" ? modelPath : modelPath.parent_path();
    if (format == "3mf")
    {
        scope.packagepath = modelPath;
        scope.partidentity = model.three_mf.model_part_path.empty()
            ? "3D/3dmodel.model"
            : model.three_mf.model_part_path;
    }
    scene.resourcescopes.push_back(scope);

    slicer_core::ModelSource source;
    source.modelid = "model-stage14d07-" + identity;
    source.sourcepath = modelPath;
    source.format = format;
    source.resourcescopeid = scope.resourcescopeid;
    source.sourcehash = HashFile(modelPath);
    source.resourcehash = slicer_core::ComputeSceneResourceHash(model);
    source.displayname = identity;
    scene.models.push_back(source);

    slicer_core::SceneModelInstance item;
    item.instance.instanceid = "instance-stage14d07-" + identity;
    item.instance.modelid = source.modelid;
    item.instance.sourcetransformidentity = source.sourcepath.generic_string();
    item.instance.sourcebboxmm = model.bbox_mm;
    item.instance.transform.translatexmm = 10.0 - model.bbox_mm.min.x;
    item.instance.transform.translateymm = 10.0 - model.bbox_mm.min.y;
    item.instance.transformrevision = 1U;
    item.instance.effectivebboxmm = model.bbox_mm;
    item.instance.effectivebboxmm.min.x += item.instance.transform.translatexmm;
    item.instance.effectivebboxmm.max.x += item.instance.transform.translatexmm;
    item.instance.effectivebboxmm.min.y += item.instance.transform.translateymm;
    item.instance.effectivebboxmm.max.y += item.instance.transform.translateymm;
    item.requestedtransform = item.instance.transform;
    item.effectivetransform = item.instance.transform;
    item.admissionstatus = slicer_core::SceneInstanceAdmissionStatus::Admitted;
    item.resolvedprofileid = scene.resolvedprofileid;
    scene.instances.push_back(std::move(item));
    return {model, scene, modelPath};
}

WorkerJobFiles WriteRequest(
    const std::string& jobId,
    const slicer_core::Json& request,
    std::optional<artifacts::PackageArtifactIdentity> artifactIdentity)
{
    const std::filesystem::path jobRoot = g_runRoot / jobId;
    const std::filesystem::path requestPath = jobRoot / "request.json";
    WriteJson(requestPath, request);
    return {requestPath, jobRoot / "cancel.requested", artifactIdentity};
}

module::WorkerLaunchOptions MakeOptions(
    const std::filesystem::path& workerPath,
    const WorkerJobFiles& files,
    const bool requireProgress)
{
    module::WorkerLaunchOptions options;
    options.executablePath = workerPath;
    options.arguments = {"--spi-request", files.requestPath.generic_string()};
    options.workingDirectory = workerPath.parent_path();
    options.cancellationMarkerPath = files.cancelPath;
    options.timeout = 30s;
    options.cancelGracePeriod = 2s;
    options.requireTerminalProgress = requireProgress;
    if (files.artifactIdentity.has_value())
    {
        const auto& identity = *files.artifactIdentity;
        options.packageArtifacts = module::WorkerPackageArtifactContext{
            identity.package_directory,
            identity.job_id,
            identity.attempt_id};
    }
    return options;
}

WorkerOutcome RunWorker(
    module::WorkerClient& client,
    const std::filesystem::path& workerPath,
    const WorkerJobFiles& files,
    const bool requireProgress)
{
    WorkerOutcome outcome;
    outcome.run = client.Run(MakeOptions(workerPath, files, requireProgress));
    const std::filesystem::path resultPath =
        files.requestPath.parent_path() / "result.json";
    if (std::filesystem::is_regular_file(resultPath))
    {
        outcome.result = ReadJson(resultPath);
    }
    return outcome;
}

bool HasIdentity(
    const WorkerOutcome& outcome,
    const std::string& jobId,
    const std::string& correlationId,
    const std::string& capability,
    const bool expectedOk)
{
    if (!outcome.result.has_value())
    {
        return false;
    }
    const slicer_core::Json& result = *outcome.result;
    return result.at("contract").as_string() == "file_contract"
        && result.at("major").as_int() == 1
        && result.at("minor").as_int() == 0
        && result.at("jobId").as_string() == jobId
        && result.at("correlationId").as_string() == correlationId
        && result.at("capability").as_string() == capability
        && result.at("ok").as_bool() == expectedOk;
}

std::string OutcomeCode(const WorkerOutcome& outcome)
{
    if (outcome.result.has_value()
        && outcome.result->contains("code")
        && outcome.result->at("code").is_string())
    {
        return outcome.result->at("code").as_string();
    }
    return outcome.run.errorCode;
}

slicer_core::Json MakeSliceRequest(
    const std::string& jobId,
    const std::string& correlationId,
    const Fixture& fixture,
    const slicer_core::Json& profile,
    const std::filesystem::path& packageDirectory,
    const std::optional<std::string>& sceneHashOverride)
{
    return slicer_core::Json::object({
        {"contract", "file_contract"},
        {"major", 1},
        {"minor", 0},
        {"jobId", jobId},
        {"correlationId", correlationId},
        {"capability", "slice.rgbwsv"},
        {"timeoutMs", 30000},
        {"sceneHash", sceneHashOverride.value_or(
            "sha256:" + slicer_core::ComputeMultiModelSceneHash(fixture.scene))},
        {"scene", slicer_core::SerializeMultiModelScene(fixture.scene)},
        {"profile", profile},
        {"output", slicer_core::Json::object({
            {"contract", "p0.rgbwsv.2"},
            {"packageDir", packageDirectory.generic_string()},
        })},
    });
}

slicer_core::Json MakePreflightRequest(
    const std::string& jobId,
    const std::string& correlationId,
    const Fixture& fixture,
    const slicer_core::Json& profile,
    const std::optional<std::string>& sceneHashOverride)
{
    const slicer_core::Json scene =
        slicer_core::SerializeMultiModelScene(fixture.scene);
    return slicer_core::Json::object({
        {"contract", "file_contract"},
        {"major", 1},
        {"minor", 0},
        {"jobId", jobId},
        {"correlationId", correlationId},
        {"capability", "geometry.preflight.full"},
        {"timeoutMs", 30000},
        {"input", slicer_core::Json::object({
            {"mode", "full"},
            {"scene", scene},
            {"sceneHash", sceneHashOverride.value_or(
                "sha256:" + slicer_core::ComputeMultiModelSceneHash(fixture.scene))},
            {"expectedSceneRevision", fixture.scene.scenerevision},
            {"profile", profile},
            {"profileHash", profile.at("profileHash")},
            {"targetMode", "legacy"},
            {"buildVolume", scene.at("buildVolume")},
        })},
    });
}

slicer_core::Json MakeRepairRequest(
    const std::string& jobId,
    const std::string& correlationId,
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& outputPath,
    const slicer_core::Json& profile)
{
    return slicer_core::Json::object({
        {"contract", "file_contract"},
        {"major", 1},
        {"minor", 0},
        {"jobId", jobId},
        {"correlationId", correlationId},
        {"capability", "geometry.repair"},
        {"timeoutMs", 30000},
        {"profile", profile},
        {"input", slicer_core::Json::object({
            {"modelId", "repair-stage14d07"},
            {"modelPath", sourcePath.generic_string()},
            {"modelFormat", "obj"},
            {"outputPath", outputPath.generic_string()},
            {"profileHash", profile.at("profileHash")},
            {"sourceResourceScope", slicer_core::Json::object({
                {"rootPath", sourcePath.parent_path().generic_string()},
            })},
            {"repairOutputFormat", "obj"},
            {"policy", "conservative"},
            {"requireStrictPass", true},
        })},
    });
}

std::vector<std::pair<std::string, std::string>> LayerHashes(
    const std::filesystem::path& packageDirectory)
{
    std::vector<std::filesystem::path> paths;
    for (const std::filesystem::directory_entry& entry
         : std::filesystem::directory_iterator(packageDirectory / "layers"))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".tiff")
        {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    std::vector<std::pair<std::string, std::string>> hashes;
    for (const auto& path : paths)
    {
        hashes.emplace_back(path.filename().generic_string(), HashFile(path));
    }
    return hashes;
}

slicer_core::Json StableReportProjection(
    const std::filesystem::path& packageDirectory)
{
    const slicer_core::Json slice =
        ReadJson(packageDirectory / "reports/slice_report.json");
    const slicer_core::Json scene =
        ReadJson(packageDirectory / "reports/multimodel_scene_report.json");
    return slicer_core::Json::object({
        {"sliceSchema", slice.at("schema")},
        {"requestedPipelineMode", slice.at("requestedPipelineMode")},
        {"effectivePipelineMode", slice.at("effectivePipelineMode")},
        {"productionAcceptance", slice.at("productionAcceptance")},
        {"productionTiffLayerCount", slice.at("productionTiffLayerCount")},
        {"materialAndSupportTotals", slice.at("totals")},
        {"sceneSchema", scene.at("schema")},
        {"sceneId", scene.at("sceneId")},
        {"sceneRevision", scene.at("sceneRevision")},
        {"sceneHash", scene.at("sceneHash")},
        {"modelCount", scene.at("modelCount")},
        {"instanceCount", scene.at("instanceCount")},
        {"visibleInstanceCount", scene.at("visibleInstanceCount")},
    });
}

bool IsMonotonicProgress(const module::WorkerRunResult& run)
{
    if (run.progressEvents.empty()
        || run.progressEvents.back().percent != 100U)
    {
        return false;
    }
    return std::adjacent_find(
        run.progressEvents.begin(),
        run.progressEvents.end(),
        [](const auto& left, const auto& right)
        {
            return left.percent > right.percent
                || left.elapsedMs < 0.0
                || right.elapsedMs < 0.0;
        }) == run.progressEvents.end();
}

}  // namespace stage14d07
