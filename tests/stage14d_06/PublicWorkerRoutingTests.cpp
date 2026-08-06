#include "contracts/print_module_spi.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/model.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{

using namespace std::chrono_literals;

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
        throw std::runtime_error("failed to read test fixture");
    }
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

slicer_core::Json ParseJson(const std::string& text)
{
    std::istringstream input{text};
    return slicer_core::Json::parse(input);
}

std::string ReadBuffered(
    const std::function<int(char*, int, int*)>& operation)
{
    int required{0};
    if (operation(nullptr, 0, &required) != PM_ERR_BUFFER_SMALL || required <= 0)
    {
        throw std::runtime_error("SPI buffer probe failed");
    }
    for (int attempt{0}; attempt < 4; ++attempt)
    {
        std::vector<char> bytes(static_cast<std::size_t>(required) + 1U, '\0');
        const int written = operation(
            bytes.data(),
            static_cast<int>(bytes.size()),
            &required);
        if (written >= 0)
        {
            return std::string(bytes.data(), static_cast<std::size_t>(written));
        }
        if (written != PM_ERR_BUFFER_SMALL || required <= 0)
        {
            break;
        }
    }
    throw std::runtime_error("SPI buffer read failed");
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
            {"maxHeightMm", 9.0}})},
        {"slicePipeline", slicer_core::Json::object({{"mode", "legacy"}})},
        {"input", slicer_core::Json::object({
            {"modelPath", FixturePath().generic_string()},
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
            {"name", "profile-stage14d06-r2"},
            {"target", "stage14d06-r2-fixture"}})}};
    profile.emplace(
        "profileHash",
        slicer_core::api::ComputeProfileDocumentHash(
            slicer_core::Json{profile}));
    return slicer_core::Json{std::move(profile)};
}

slicer_core::MultiModelScene MakeScene(const slicer_core::SceneModel& model)
{
    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-stage14d06-r2";
    scene.scenerevision = 1U;
    scene.buildvolume = slicer_core::MakeDefaultDeviceBuildVolume();
    scene.resolvedprofileid = "profile-stage14d06-r2";

    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-stage14d06-r2";
    scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = FixturePath().parent_path();
    scene.resourcescopes.push_back(scope);

    slicer_core::ModelSource source;
    source.modelid = "model-stage14d06-r2";
    source.sourcepath = FixturePath();
    source.format = "obj";
    source.resourcescopeid = scope.resourcescopeid;
    source.sourcehash = slicer_core::ComputeSha256(ReadBytes(source.sourcepath));
    source.resourcehash = slicer_core::ComputeSceneResourceHash(model);
    source.displayname = "Stage 14D-06 R2 cube";
    scene.models.push_back(source);

    slicer_core::SceneModelInstance item;
    item.instance.instanceid = "instance-stage14d06-r2";
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
    return scene;
}

slicer_core::Json MakeSliceRequest(
    const std::filesystem::path& packageDirectory,
    const slicer_core::MultiModelScene& scene,
    const std::string& jobId,
    const double layerThicknessMm,
    const std::string& backend = "worker")
{
    slicer_core::Json::Object options;
    if (!backend.empty())
    {
        options.emplace("backend", backend);
    }
    return slicer_core::Json::object({
        {"capability", "slice.rgbwsv"},
        {"jobId", jobId},
        {"correlationId", "correlation-" + jobId},
        {"sceneHash", "sha256:"
            + slicer_core::ComputeMultiModelSceneHash(scene)},
        {"scene", slicer_core::SerializeMultiModelScene(scene)},
        {"profile", MakeProfile(packageDirectory, layerThicknessMm)},
        {"output", slicer_core::Json::object({
            {"contract", "p0.rgbwsv.2"},
            {"packageDir", packageDirectory.generic_string()}})},
        {"options", slicer_core::Json{std::move(options)}}});
}

slicer_core::Json MakeFullPreflightRequest(
    const slicer_core::MultiModelScene& scene,
    const slicer_core::Json& profile)
{
    return slicer_core::Json::object({
        {"capability", "geometry.preflight"},
        {"mode", "full"},
        {"scene", slicer_core::SerializeMultiModelScene(scene)},
        {"sceneHash", "sha256:"
            + slicer_core::ComputeMultiModelSceneHash(scene)},
        {"expectedSceneRevision", scene.scenerevision},
        {"profile", profile},
        {"profileHash", profile.at("profileHash").as_string()},
        {"targetMode", "legacy"},
        {"buildVolume", slicer_core::SerializeMultiModelScene(scene).at("buildVolume")}});
}

void WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw std::runtime_error("failed to create repair fixture");
    }
    output << text;
}

slicer_core::Json WaitForTerminal(pm_job_t* job)
{
    const auto deadline = std::chrono::steady_clock::now() + 45s;
    for (;;)
    {
        const slicer_core::Json progress = ParseJson(ReadBuffered(
            [job](char* output, const int capacity, int* required)
            {
                return pm_poll(job, output, capacity, required);
            }));
        const std::string state = progress.at("state").as_string();
        if (state == "succeeded" || state == "failed" || state == "cancelled")
        {
            return progress;
        }
        if (std::chrono::steady_clock::now() >= deadline)
        {
            throw std::runtime_error("public Worker job did not become terminal");
        }
        std::this_thread::sleep_for(20ms);
    }
}

void TestWorkerSliceAndBackendRejection(
    pm_module_t* module,
    const std::filesystem::path& root,
    const slicer_core::MultiModelScene& scene)
{
    const std::filesystem::path packageDirectory = root / "package-success";
    const std::string request = MakeSliceRequest(
        packageDirectory,
        scene,
        "job-public-success",
        0.2,
        "").dump(0);
    pm_job_t* const job = pm_submit(module, request.c_str());
    Check(job != nullptr, "public SPI accepts a Worker-only slice request");
    if (job == nullptr)
    {
        return;
    }
    const slicer_core::Json progress = WaitForTerminal(job);
    Check(progress.at("state").as_string() == "succeeded",
        "public Worker slice reaches succeeded");
    const slicer_core::Json result = ParseJson(ReadBuffered(
        [job](char* output, const int capacity, int* required)
        {
            return pm_result(job, output, capacity, required);
        }));
    Check(result.at("ok").as_bool()
            && result.at("code").as_string() == "PM-SLICER-OK-0000",
        "public Worker result is the public capability envelope");
    Check(result.at("packageDir").as_string()
            == packageDirectory.generic_string(),
        "public Worker result preserves the requested package identity");
    (void)slicer_core::internal::ValidateSlicePackageArtifact(packageDirectory);
    pm_release(job);

    const std::filesystem::path rejectedPackage = root / "package-rejected";
    const std::string rejectedRequest = MakeSliceRequest(
        rejectedPackage,
        scene,
        "job-public-rejected",
        0.2,
        "inprocess").dump(0);
    Check(pm_submit(module, rejectedRequest.c_str()) == nullptr,
        "public SPI rejects backend=inprocess before Worker launch");
    const slicer_core::Json error = ParseJson(ReadBuffered(
        [](char* output, const int capacity, int* required)
        {
            return pm_last_error(output, capacity, required);
        }));
    Check(error.at("code").as_string() == "PM-SLICER-PROFILE-0031",
        "backend=inprocess exposes the frozen profile error");
    Check(!std::filesystem::exists(rejectedPackage),
        "backend rejection creates no package");
}

void TestWorkerCancellation(
    pm_module_t* module,
    const std::filesystem::path& root,
    const slicer_core::MultiModelScene& scene)
{
    const std::string request = MakeSliceRequest(
        root / "package-cancelled",
        scene,
        "job-public-cancelled",
        0.0001).dump(0);
    pm_job_t* const job = pm_submit(module, request.c_str());
    Check(job != nullptr, "public SPI accepts the cancellable Worker job");
    if (job == nullptr)
    {
        return;
    }
    Check(pm_cancel(job) == PM_OK,
        "public SPI accepts cooperative Worker cancellation");
    const slicer_core::Json progress = WaitForTerminal(job);
    Check(progress.at("state").as_string() == "cancelled",
        "public Worker cancellation becomes terminal cancelled");
    const slicer_core::Json result = ParseJson(ReadBuffered(
        [job](char* output, const int capacity, int* required)
        {
            return pm_result(job, output, capacity, required);
        }));
    Check(!result.at("ok").as_bool()
            && result.at("code").as_string() == "PM-SLICER-CANCELLED-0070",
        "public Worker cancellation returns the frozen result code");
    pm_release(job);
}

void TestWorkerFullPreflight(
    pm_module_t* module,
    const std::filesystem::path& root,
    const slicer_core::MultiModelScene& scene)
{
    const slicer_core::Json profile = MakeProfile(
        root / "unused-preflight-package",
        0.2);
    const std::string request = MakeFullPreflightRequest(scene, profile).dump(0);
    pm_job_t* const job = pm_submit(module, request.c_str());
    Check(job != nullptr, "public SPI routes full preflight to Worker");
    if (job == nullptr)
    {
        return;
    }
    const slicer_core::Json progress = WaitForTerminal(job);
    Check(progress.at("state").as_string() == "succeeded",
        "public Worker full preflight reaches succeeded");
    const slicer_core::Json result = ParseJson(ReadBuffered(
        [job](char* output, const int capacity, int* required)
        {
            return pm_result(job, output, capacity, required);
        }));
    Check(result.at("ok").as_bool()
            && result.at("authoritative").as_bool()
            && result.at("complete").as_bool(),
        "public full preflight preserves authoritative Worker evidence");
    Check(!std::filesystem::exists(root / "unused-preflight-package"),
        "full preflight creates no production package");
    pm_release(job);
}

void TestWorkerRepair(
    pm_module_t* module,
    const std::filesystem::path& root)
{
    const std::filesystem::path source = root / "repair-source" / "fixture.obj";
    const std::filesystem::path output = root / "repair-public" / "fixture.repaired.obj";
    WriteText(
        source,
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "f 1 3 2\nf 1 2 4\nf 2 3 4\nf 3 1 4\nf 1 3 2\n");
    std::ifstream profileInput(
        SourceRoot()
        / "samples/configs/material_process/obj_mtl_texture_rgb_white_ondemand.json",
        std::ios::binary);
    const slicer_core::Json profile = slicer_core::Json::parse(profileInput);
    const slicer_core::Json request = slicer_core::Json::object({
        {"capability", "geometry.repair"},
        {"jobId", "job-public-repair"},
        {"correlationId", "correlation-public-repair"},
        {"modelId", "repair-fixture"},
        {"modelPath", source.generic_string()},
        {"modelFormat", "obj"},
        {"outputPath", output.generic_string()},
        {"profile", profile},
        {"profileHash", slicer_core::api::ComputeProfileDocumentHash(profile)},
        {"sourceResourceScope", slicer_core::Json::object({
            {"rootPath", source.parent_path().generic_string()}})},
        {"repairOutputFormat", "obj"},
        {"policy", "conservative"},
        {"requireStrictPass", true}});
    const std::string requestText = request.dump(0);
    pm_job_t* const job = pm_submit(module, requestText.c_str());
    Check(job != nullptr, "public SPI routes repair to Worker");
    if (job == nullptr)
    {
        return;
    }
    const slicer_core::Json progress = WaitForTerminal(job);
    Check(progress.at("state").as_string() == "succeeded",
        "public Worker repair reaches succeeded");
    const slicer_core::Json result = ParseJson(ReadBuffered(
        [job](char* bytes, const int capacity, int* required)
        {
            return pm_result(job, bytes, capacity, required);
        }));
    Check(result.at("ok").as_bool()
            && result.at("outputPath").as_string() == output.generic_string(),
        "public repair publishes and returns the requested external output path");
    Check(std::filesystem::is_regular_file(output)
            && std::filesystem::is_regular_file(
                std::filesystem::path(output.generic_string() + ".evidence.json")),
        "public repair persists the OBJ and strict evidence beyond job release");
    pm_release(job);
    Check(std::filesystem::is_regular_file(output),
        "public repair output survives private Worker job cleanup");
}

}  // namespace

int main()
{
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("slicesoft_stage14d06_r2_" + std::to_string(suffix));
    std::filesystem::create_directories(root);
    try
    {
        slicer_core::SliceConfig loadConfig;
        loadConfig.input.model_path = FixturePath();
        loadConfig.input.format = "obj";
        loadConfig.auto_orient.enabled = false;
        const slicer_core::SceneModel model = slicer_core::load_model_report(
            loadConfig,
            FixturePath().parent_path());
        const slicer_core::MultiModelScene scene = MakeScene(model);

        pm_module_t* const module = pm_create(nullptr);
        Check(module != nullptr, "public SPI creates a module for Worker routing");
        if (module != nullptr)
        {
            TestWorkerFullPreflight(module, root, scene);
            TestWorkerRepair(module, root);
            TestWorkerSliceAndBackendRejection(module, root, scene);
            TestWorkerCancellation(module, root, scene);
            pm_destroy(module);
        }
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
        std::cerr << g_failures << " public Worker routing test(s) failed\n";
        return 1;
    }
    std::cout << "stage14d06 public Worker routing tests passed\n";
    return 0;
}
