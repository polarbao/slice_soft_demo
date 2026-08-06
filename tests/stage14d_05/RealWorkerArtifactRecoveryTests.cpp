#include "slicer_module/WorkerClient.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/api/artifacts/PackageArtifactSafety.h"
#include "slicer_core/config.h"
#include "slicer_core/model.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>

namespace
{

using namespace std::chrono_literals;
namespace artifacts = slicer_core::api::artifacts;
namespace module = slicesoft::module;

int g_failures{0};

void Check(const bool condition, const std::string& message)
{
    if (!condition)
    {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::filesystem::path SourceRoot()
{
    return std::filesystem::path(SLICESOFT_SOURCE_DIR);
}

std::filesystem::path FixturePath()
{
    return std::filesystem::absolute(
        SourceRoot() / "samples/models/openvdb/surface_shell_cube.obj")
        .lexically_normal();
}

std::string ReadBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to read test file: " + path.generic_string());
    }
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

void WriteJson(
    const std::filesystem::path& path,
    const slicer_core::Json& document)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw std::runtime_error("failed to create Worker request");
    }
    output << document.dump(2) << '\n';
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("failed to read Worker result");
    }
    return slicer_core::Json::parse(input);
}

slicer_core::Json MakeProfile(
    const std::filesystem::path& packageDirectory,
    const double layerThicknessMm)
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
            {"modelPath", FixturePath().generic_string()},
            {"format", "obj"},
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
            {"name", "profile-stage14d05-r4"},
            {"target", "stage14d05-r4-fixture"},
        })},
    };
    profile.emplace(
        "profileHash",
        slicer_core::api::ComputeProfileDocumentHash(
            slicer_core::Json{profile}));
    return slicer_core::Json{std::move(profile)};
}

slicer_core::MultiModelScene MakeScene(const slicer_core::SceneModel& model)
{
    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-stage14d05-r4";
    scene.scenerevision = 1U;
    scene.buildvolume = slicer_core::MakeDefaultDeviceBuildVolume();
    scene.resolvedprofileid = "profile-stage14d05-r4";

    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-stage14d05-r4";
    scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = FixturePath().parent_path();
    scene.resourcescopes.push_back(scope);

    slicer_core::ModelSource source;
    source.modelid = "model-stage14d05-r4";
    source.sourcepath = FixturePath();
    source.format = "obj";
    source.resourcescopeid = scope.resourcescopeid;
    source.sourcehash = slicer_core::ComputeSha256(ReadBytes(source.sourcepath));
    source.resourcehash = slicer_core::ComputeSceneResourceHash(model);
    source.displayname = "Stage 14D-05 R4 cube";
    scene.models.push_back(source);

    slicer_core::SceneModelInstance item;
    item.instance.instanceid = "instance-stage14d05-r4";
    item.instance.modelid = source.modelid;
    item.instance.sourcetransformidentity = source.sourcepath.generic_string();
    item.instance.sourcebboxmm = model.bbox_mm;
    item.instance.transform.translatexmm = 10.0 - model.bbox_mm.min.x;
    item.instance.transform.translateymm = 10.0 - model.bbox_mm.min.y;
    item.instance.transformrevision = 1U;
    item.instance.effectivebboxmm = model.bbox_mm;
    item.instance.effectivebboxmm.min.x +=
        item.instance.transform.translatexmm;
    item.instance.effectivebboxmm.max.x +=
        item.instance.transform.translatexmm;
    item.instance.effectivebboxmm.min.y +=
        item.instance.transform.translateymm;
    item.instance.effectivebboxmm.max.y +=
        item.instance.transform.translateymm;
    item.requestedtransform = item.instance.transform;
    item.effectivetransform = item.instance.transform;
    item.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Admitted;
    item.resolvedprofileid = scene.resolvedprofileid;
    scene.instances.push_back(std::move(item));
    return scene;
}

struct WorkerJobFiles
{
    std::filesystem::path requestPath;
    std::filesystem::path cancelPath;
    artifacts::PackageArtifactIdentity artifactIdentity;
};

WorkerJobFiles WriteSliceRequest(
    const std::filesystem::path& root,
    const std::string& jobId,
    const std::string& correlationId,
    const std::filesystem::path& packageDirectory,
    const slicer_core::MultiModelScene& scene,
    const double layerThicknessMm)
{
    const std::filesystem::path jobRoot = root / jobId;
    const std::filesystem::path requestPath = jobRoot / "request.json";
    const slicer_core::Json profile =
        MakeProfile(packageDirectory, layerThicknessMm);
    WriteJson(
        requestPath,
        slicer_core::Json::object({
            {"contract", "file_contract"},
            {"major", 1},
            {"minor", 0},
            {"jobId", jobId},
            {"correlationId", correlationId},
            {"capability", "slice.rgbwsv"},
            {"timeoutMs", 30000},
            {"sceneHash", "sha256:"
                + slicer_core::ComputeMultiModelSceneHash(scene)},
            {"scene", slicer_core::SerializeMultiModelScene(scene)},
            {"profile", profile},
            {"output", slicer_core::Json::object({
                {"contract", "p0.rgbwsv.2"},
                {"packageDir", packageDirectory.generic_string()},
            })},
        }));
    return {
        requestPath,
        jobRoot / "cancel.requested",
        artifacts::MakePackageArtifactIdentity(
            packageDirectory,
            jobId,
            artifacts::MakePackageAttemptId(correlationId))};
}

module::WorkerLaunchOptions MakeOptions(
    const std::filesystem::path& workerPath,
    const WorkerJobFiles& files)
{
    module::WorkerLaunchOptions options;
    options.executablePath = workerPath;
    options.arguments = {"--spi-request", files.requestPath.generic_string()};
    options.workingDirectory = workerPath.parent_path();
    options.cancellationMarkerPath = files.cancelPath;
    options.timeout = 30s;
    options.cancelGracePeriod = 2s;
    options.packageArtifacts = module::WorkerPackageArtifactContext{
        files.artifactIdentity.package_directory,
        files.artifactIdentity.job_id,
        files.artifactIdentity.attempt_id};
    return options;
}

void CheckNoOwnedResidue(
    const artifacts::PackageArtifactIdentity& identity,
    const std::string& context)
{
    Check(!std::filesystem::exists(identity.staging_directory),
        context + " leaves no owned staging directory");
    Check(!std::filesystem::exists(identity.backup_directory),
        context + " leaves no owned backup directory");
    Check(!std::filesystem::exists(identity.lease_directory),
        context + " leaves no target lease directory");
}

void TestRealWorkerNormalCancelAndStrongKill(
    const std::filesystem::path& root,
    const std::filesystem::path& workerPath,
    const slicer_core::MultiModelScene& scene)
{
    const std::filesystem::path packageDirectory = root / "package";
    const WorkerJobFiles normalFiles = WriteSliceRequest(
        root, "job-normal", "correlation-normal",
        packageDirectory, scene, 0.2);
    module::WorkerClient client;
    const module::WorkerRunResult normal =
        client.Run(MakeOptions(workerPath, normalFiles));
    Check(normal.exitCategory == module::WorkerExitCategory::Ok,
        "real Worker publishes one valid package");
    Check(normal.artifactCleanupAttempted
            && normal.artifactCleanupSucceeded,
        "module recovery confirms the normal Worker publication");
    CheckNoOwnedResidue(normalFiles.artifactIdentity, "normal Worker run");
    (void)slicer_core::internal::ValidateSlicePackageArtifact(
        packageDirectory);
    const std::string baselineManifest =
        ReadBytes(packageDirectory / "manifest.json");
    const slicer_core::Json normalResult =
        ReadJson(normalFiles.requestPath.parent_path() / "result.json");
    Check(normalResult.at("ok").as_bool()
            && normalResult.at("code").as_string() == "PM-SLICER-OK-0000",
        "real Worker result identity reports success");

    const WorkerJobFiles cancelFiles = WriteSliceRequest(
        root, "job-cancel", "correlation-cancel",
        packageDirectory, scene, 0.0001);
    module::WorkerRunResult cancelled;
    std::thread cancelThread([&]()
    {
        cancelled = client.Run(MakeOptions(workerPath, cancelFiles));
    });
    while (!client.IsRunning())
    {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(100ms);
    Check(client.RequestCancel(),
        "real Worker accepts a cooperative cancellation request");
    cancelThread.join();
    Check(cancelled.stopReason == module::WorkerStopReason::Cancelled,
        "real Worker cooperative cancellation is terminal only after exit");
    Check(cancelled.artifactCleanupSucceeded,
        "module cleanup succeeds after real Worker cancellation");
    CheckNoOwnedResidue(cancelFiles.artifactIdentity, "cancelled Worker run");
    Check(ReadBytes(packageDirectory / "manifest.json") == baselineManifest,
        "cooperative cancellation preserves the last valid package");

    const WorkerJobFiles timeoutFiles = WriteSliceRequest(
        root, "job-timeout", "correlation-timeout",
        packageDirectory, scene, 0.0001);
    module::WorkerLaunchOptions timeoutOptions =
        MakeOptions(workerPath, timeoutFiles);
    timeoutOptions.timeout = 100ms;
    timeoutOptions.cancelGracePeriod = 0ms;
    const module::WorkerRunResult timedOut = client.Run(timeoutOptions);
    Check(timedOut.stopReason == module::WorkerStopReason::TimedOut,
        "real Worker timeout has the frozen terminal reason");
    Check(timedOut.forcedTermination,
        "zero-grace timeout force-terminates the real Worker Job Object");
    Check(timedOut.artifactCleanupSucceeded,
        "module cleanup succeeds after real Worker strong kill");
    CheckNoOwnedResidue(timeoutFiles.artifactIdentity, "strong-killed Worker run");
    Check(ReadBytes(packageDirectory / "manifest.json") == baselineManifest,
        "strong kill preserves the last valid package");
    (void)slicer_core::internal::ValidateSlicePackageArtifact(
        packageDirectory);
}

}  // namespace

int main(const int argc, char* const argv[])
{
    if (argc != 2)
    {
        std::cerr << "usage: stage14d05_real_worker_artifact_tests <slicer_worker>\n";
        return 2;
    }
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / ("slicesoft_stage14d05_r4_" + std::to_string(suffix));
    std::filesystem::create_directories(root);
    try
    {
        slicer_core::SliceConfig loadConfig;
        loadConfig.input.model_path = FixturePath();
        loadConfig.input.format = "obj";
        loadConfig.auto_orient.enabled = false;
        const slicer_core::SceneModel model = slicer_core::load_model_report(
            loadConfig, FixturePath().parent_path());
        TestRealWorkerNormalCancelAndStrongKill(
            root,
            std::filesystem::absolute(argv[1]).lexically_normal(),
            MakeScene(model));
    }
    catch (const std::exception& error)
    {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    if (g_failures != 0)
    {
        std::cerr << g_failures << " real Worker artifact test(s) failed\n";
        return 1;
    }
    std::cout << "stage14d05 real Worker artifact tests passed\n";
    return 0;
}
