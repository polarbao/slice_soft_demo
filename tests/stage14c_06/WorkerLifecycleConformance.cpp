#include "WorkerLifecycleConformance.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/api/artifacts/PackageArtifactSafety.h"
#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/model.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace slicesoft::tests
{
namespace
{

using namespace std::chrono_literals;

using OutputCall = std::function<int(char*, int, int*)>;

void Require(const bool condition, const std::string_view message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string{message});
    }
}

void ReportPass(const std::string_view id, const std::string_view detail)
{
    std::cout << id << " PASS " << detail << '\n';
}

std::string ReadOutput(const OutputCall& call)
{
    int required{0};
    Require(
        call(nullptr, 0, &required) == PM_ERR_BUFFER_SMALL && required > 0,
        "Worker lifecycle output probe failed");
    for (int attempt{0}; attempt < 4; ++attempt)
    {
        std::vector<char> bytes(static_cast<std::size_t>(required) + 1U, '\0');
        const int written = call(
            bytes.data(),
            static_cast<int>(bytes.size()),
            &required);
        if (written >= 0)
        {
            return {bytes.data(), static_cast<std::size_t>(written)};
        }
        Require(
            written == PM_ERR_BUFFER_SMALL && required > 0,
            "Worker lifecycle output read failed");
    }
    throw std::runtime_error("Worker lifecycle output did not stabilize");
}

slicer_core::Json ParseJson(const std::string& text)
{
    std::istringstream input{text};
    return slicer_core::Json::parse(input);
}

std::string ReadBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    Require(input.good(), "Worker lifecycle fixture could not be read");
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

std::filesystem::path FixturePath(const std::filesystem::path& repository)
{
    return std::filesystem::absolute(
        repository / "samples/models/openvdb/surface_shell_cube.obj")
        .lexically_normal();
}

slicer_core::Json MakeProfile(
    const std::filesystem::path& fixture,
    const std::filesystem::path& packageDirectory,
    const double layerThicknessMm)
{
    slicer_core::Json::Object profile{
        {"profileVersion", "1.0"},
        {"slicingMode", "closed_mesh_scanline"},
        {"autoOrient", slicer_core::Json::object({
            {"enabled", false},
            {"maxHeightMm", 9.0}})},
        {"slicePipeline", slicer_core::Json::object({{"mode", "legacy"}})},
        {"input", slicer_core::Json::object({
            {"modelPath", fixture.generic_string()},
            {"format", "obj"}})},
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
            {"rowsPerStrip", 64}})},
        {"preview", slicer_core::Json::object({{"enabled", false}})},
        {"background", slicer_core::Json::object({{"value", 255}})},
        {"modelMaterial", slicer_core::Json::object({
            {"materialChannel", "RGB"},
            {"applyMode", "solid_volume"},
            {"rgb", slicer_core::Json::array({0, 0, 0})},
            {"whiteValue", 255},
            {"varnishValue", 255}})},
        {"materialProcessProfile", slicer_core::Json::object({
            {"enabled", true},
            {"name", "profile-stage14c06b"},
            {"target", "stage14c06b-fixture"}})}};
    profile.emplace(
        "profileHash",
        slicer_core::api::ComputeProfileDocumentHash(
            slicer_core::Json{profile}));
    return slicer_core::Json{std::move(profile)};
}

slicer_core::MultiModelScene MakeScene(
    const std::filesystem::path& fixture,
    const slicer_core::SceneModel& model)
{
    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-stage14c06b";
    scene.scenerevision = 1U;
    scene.buildvolume = slicer_core::MakeDefaultDeviceBuildVolume();
    scene.resolvedprofileid = "profile-stage14c06b";

    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-stage14c06b";
    scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = fixture.parent_path();
    scene.resourcescopes.push_back(scope);

    slicer_core::ModelSource source;
    source.modelid = "model-stage14c06b";
    source.sourcepath = fixture;
    source.format = "obj";
    source.resourcescopeid = scope.resourcescopeid;
    source.sourcehash = slicer_core::ComputeSha256(ReadBytes(fixture));
    source.resourcehash = slicer_core::ComputeSceneResourceHash(model);
    source.displayname = "Stage 14C-06B cube";
    scene.models.push_back(source);

    slicer_core::SceneModelInstance item;
    item.instance.instanceid = "instance-stage14c06b";
    item.instance.modelid = source.modelid;
    item.instance.sourcetransformidentity = fixture.generic_string();
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
    return scene;
}

std::string MakeRequest(
    const std::filesystem::path& fixture,
    const std::filesystem::path& packageDirectory,
    const slicer_core::MultiModelScene& scene,
    const std::string& jobId,
    const double layerThicknessMm)
{
    return slicer_core::Json::object({
        {"capability", "slice.rgbwsv"},
        {"jobId", jobId},
        {"correlationId", "correlation-" + jobId},
        {"sceneHash", "sha256:"
            + slicer_core::ComputeMultiModelSceneHash(scene)},
        {"scene", slicer_core::SerializeMultiModelScene(scene)},
        {"profile", MakeProfile(fixture, packageDirectory, layerThicknessMm)},
        {"output", slicer_core::Json::object({
            {"contract", "p0.rgbwsv.2"},
            {"packageDir", packageDirectory.generic_string()}})},
        {"options", slicer_core::Json::object({{"backend", "worker"}})}})
        .dump(0);
}

slicer_core::Json Poll(
    const SpiModuleApi& api,
    pm_job_t* job)
{
    return ParseJson(ReadOutput(
        [&api, job](char* output, const int capacity, int* required)
        {
            return api.Poll(job, output, capacity, required);
        }));
}

slicer_core::Json WaitForTerminal(
    const SpiModuleApi& api,
    pm_job_t* job)
{
    const auto deadline = std::chrono::steady_clock::now() + 45s;
    for (;;)
    {
        const slicer_core::Json progress = Poll(api, job);
        const std::string state = progress.at("state").as_string();
        if (state == "succeeded" || state == "failed" || state == "cancelled")
        {
            return progress;
        }
        Require(
            std::chrono::steady_clock::now() < deadline,
            "Worker lifecycle job did not become terminal");
        std::this_thread::sleep_for(10ms);
    }
}

void WaitForWorkerActivity(
    const SpiModuleApi& api,
    pm_job_t* job)
{
    const auto deadline = std::chrono::steady_clock::now() + 30s;
    for (;;)
    {
        const slicer_core::Json progress = Poll(api, job);
        const std::string state = progress.at("state").as_string();
        const std::string phase = progress.at("phase").as_string();
        if (state == "running"
            && phase != "queued"
            && phase != "worker_contract"
            && phase != "worker_execute")
        {
            return;
        }
        Require(
            state != "succeeded" && state != "failed" && state != "cancelled",
            "Worker lifecycle job ended before active cancellation");
        Require(
            std::chrono::steady_clock::now() < deadline,
            "Worker lifecycle job did not expose active progress");
        std::this_thread::sleep_for(10ms);
    }
}

struct TemporaryRoot final
{
    std::filesystem::path path;

    ~TemporaryRoot()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

}  // namespace

void TestWorkerLifecycleConformance(
    const SpiModuleApi& api,
    pm_module_t* const module,
    const std::filesystem::path& repository)
{
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    TemporaryRoot root{
        std::filesystem::temp_directory_path()
        / ("slicesoft_stage14c06b_" + std::to_string(nonce))};
    std::filesystem::create_directories(root.path);

    const std::filesystem::path fixture = FixturePath(repository);
    slicer_core::SliceConfig loadConfig;
    loadConfig.input.model_path = fixture;
    loadConfig.input.format = "obj";
    loadConfig.auto_orient.enabled = false;
    const slicer_core::SceneModel model = slicer_core::load_model_report(
        loadConfig,
        fixture.parent_path());
    const slicer_core::MultiModelScene scene = MakeScene(fixture, model);
    const std::filesystem::path packageDirectory = root.path / "package";

    const std::string baselineRequest = MakeRequest(
        fixture,
        packageDirectory,
        scene,
        "job-stage14c06b-baseline",
        0.2);
    pm_job_t* const baselineJob = api.Submit(module, baselineRequest.c_str());
    Require(baselineJob != nullptr, "C-SPI-09 baseline job was rejected");
    Require(
        WaitForTerminal(api, baselineJob).at("state").as_string()
            == "succeeded",
        "C-SPI-09 baseline package was not published");
    api.Release(baselineJob);
    (void)slicer_core::internal::ValidateSlicePackageArtifact(packageDirectory);
    const std::string baselineManifest =
        ReadBytes(packageDirectory / "manifest.json");

    constexpr std::string_view jobId{"job-stage14c06b-cancel"};
    constexpr std::string_view correlationId{"correlation-job-stage14c06b-cancel"};
    const std::string request = MakeRequest(
        fixture,
        packageDirectory,
        scene,
        std::string{jobId},
        0.0001);
    pm_job_t* const job = api.Submit(module, request.c_str());
    Require(job != nullptr, "Worker lifecycle job was rejected");
    int required{0};
    Require(
        api.Result(job, nullptr, 0, &required) == PM_ERR_INVALID_STATE,
        "C-SPI-13 queued result was not rejected");
    WaitForWorkerActivity(api, job);
    Require(
        api.Result(job, nullptr, 0, &required) == PM_ERR_INVALID_STATE,
        "C-SPI-13 running result was not rejected");

    const auto cancelledAt = std::chrono::steady_clock::now();
    Require(api.Cancel(job) == PM_OK, "C-SPI-08 cancellation was rejected");
    const slicer_core::Json cancelling = Poll(api, job);
    Require(
        cancelling.at("state").as_string() == "cancelling",
        "C-SPI-13 cancelling state was not observable");
    Require(
        api.Result(job, nullptr, 0, &required) == PM_ERR_INVALID_STATE,
        "C-SPI-13 cancelling result was not rejected");
    Require(
        api.Cancel(job) == PM_OK,
        "C-SPI-15 repeated running cancellation was not idempotent");

    const slicer_core::Json terminal = WaitForTerminal(api, job);
    const auto cancelLatency = std::chrono::steady_clock::now() - cancelledAt;
    Require(
        terminal.at("state").as_string() == "cancelled",
        "C-SPI-08 Worker did not reach cancelled");
    Require(cancelLatency <= 2000ms, "C-SPI-08 exceeded cancelLatencyMs");
    const slicer_core::Json result = ParseJson(ReadOutput(
        [&api, job](char* output, const int capacity, int* outputRequired)
        {
            return api.Result(job, output, capacity, outputRequired);
        }));
    Require(
        !result.at("ok").as_bool()
            && result.at("code").as_string() == "PM-SLICER-CANCELLED-0070",
        "C-SPI-08 result code did not close against cancellation");
    Require(
        api.Cancel(job) == PM_OK,
        "C-SPI-15 terminal cancellation was not idempotent");
    api.Release(job);

    const auto identity =
        slicer_core::api::artifacts::MakePackageArtifactIdentity(
            packageDirectory,
            std::string{jobId},
            slicer_core::api::artifacts::MakePackageAttemptId(correlationId));
    Require(
        ReadBytes(packageDirectory / "manifest.json") == baselineManifest,
        "C-SPI-09 cancellation overwrote the previous valid package");
    Require(
        !std::filesystem::exists(identity.staging_directory)
            && !std::filesystem::exists(identity.backup_directory)
            && !std::filesystem::exists(identity.lease_directory),
        "C-SPI-09 cancellation left owned package residue");
    (void)slicer_core::internal::ValidateSlicePackageArtifact(packageDirectory);

    ReportPass("C-SPI-08", "real Worker cancellation completed within 2000 ms");
    ReportPass("C-SPI-09", "old package preserved and owned residue removed");
    ReportPass("C-SPI-13", "queued/running/cancelling results fail closed");
    ReportPass("C-SPI-15", "running, repeated and terminal cancel are idempotent");
}

}  // namespace slicesoft::tests
