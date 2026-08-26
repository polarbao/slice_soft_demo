#include "slicer_worker/slice/WorkerSliceExecutor.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/api/artifacts/PackageArtifactSafety.h"
#include "slicer_core/config.h"
#include "slicer_core/engine/ProductionPreflightFullFacadeFactory.h"
#include "slicer_core/engine/ProductionSliceFacadeFactory.h"
#include "slicer_core/model.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtPackageReader.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/slicer.h"
#include "slicer_core/system/Sha256.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace
{

namespace api = slicer_core::api;
namespace worker = slicesoft::worker;

int g_failures{0};

class TestCancelToken final : public api::ICancelToken
{
public:
    explicit TestCancelToken(const bool cancelled = false)
        : m_cancelled(cancelled)
    {
    }

    /** @brief 返回确定性的取消状态。 */
    [[nodiscard]] bool IsCancelRequested() const noexcept override
    {
        return m_cancelled;
    }

private:
    bool m_cancelled{false};
};

void Check(const bool condition, const std::string& message)
{
    if (!condition)
    {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::filesystem::path MakeTestRoot()
{
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("slicesoft_stage14d08_r2_executor_" + std::to_string(suffix));
    std::filesystem::create_directories(root);
    return std::filesystem::absolute(root).lexically_normal();
}

std::filesystem::path FixturePath()
{
    return std::filesystem::absolute(
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "samples/models/openvdb/surface_shell_cube.obj")
        .lexically_normal();
}

std::filesystem::path TransferFixturePath()
{
    return std::filesystem::absolute(
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "model/obj/reality/finger_suoguo/03.obj")
        .lexically_normal();
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("failed to read fixture");
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

slicer_core::Json MakeProfile(
    const std::filesystem::path& packageDirectory,
    const bool transfer = false)
{
    const std::filesystem::path fixture =
        transfer ? TransferFixturePath() : FixturePath();
    slicer_core::Json::Object profile{
        {"profileVersion", "1.0"},
        {"slicingMode", "closed_mesh_scanline"},
        {"autoOrient", slicer_core::Json::object({
            {"enabled", false},
            {"maxHeightMm", 9.0},
        })},
        {"slicePipeline", slicer_core::Json::object({{"mode", "legacy"}})},
        {"input", slicer_core::Json::object({
            {"modelPath", fixture.generic_string()},
            {"format", "obj"},
        })},
        {"output", slicer_core::Json::object({
            {"packageDir", packageDirectory.generic_string()},
            {"dpiX", 127},
            {"dpiY", 127},
            {"layerThicknessMm", 0.2},
            {"channelOrder", transfer
                ? slicer_core::Json::array({"R", "G", "B", "W", "S", "V", "T"})
                : slicer_core::Json::array({"R", "G", "B", "W", "S", "V"})},
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
            {"name", "profile-stage14d-r2-executor"},
            {"target", "stage14d-r2-executor-fixture"},
        })},
    };
    if (transfer)
    {
        slicer_core::Json::Object output = profile.at("output").as_object();
        output["packageProtocol"] = "p0.rgbwsvt.1";
        profile["output"] = slicer_core::Json{std::move(output)};
        profile["transferChannelPolicy"] = slicer_core::Json::object({
            {"enabled", true},
            {"matchSource", "material_diffuse_rgb"},
            {"materialDiffuseRgbValues", slicer_core::Json::array({
                slicer_core::Json::array({255, 220, 198})})},
            {"missingRegion", "allow_empty"},
            {"multipleMatches", "fail_closed"},
            {"value", 0},
            {"topology", slicer_core::Json::object({
                {"selfIntersectionPolicy", "tolerate_closed_self_intersection"},
                {"maxSelfIntersectionPairs", 64}})}});
    }
    const slicer_core::Json withoutHash(profile);
    profile.emplace(
        "profileHash",
        api::ComputeProfileDocumentHash(withoutHash));
    return slicer_core::Json(std::move(profile));
}

slicer_core::MultiModelScene MakeScene(
    const slicer_core::SceneModel& model,
    const bool transfer = false)
{
    const std::filesystem::path fixture =
        transfer ? TransferFixturePath() : FixturePath();
    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-r2-worker-executor";
    scene.scenerevision = 12U;
    scene.buildvolume = slicer_core::MakeDefaultDeviceBuildVolume();
    scene.resolvedprofileid = "profile-stage14d-r2-executor";

    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-r2-worker-executor";
    scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = fixture.parent_path();
    scene.resourcescopes.push_back(scope);

    slicer_core::ModelSource source;
    source.modelid = "model-r2-worker-executor";
    source.sourcepath = fixture;
    source.format = "obj";
    source.resourcescopeid = scope.resourcescopeid;
    source.sourcehash = slicer_core::ComputeSha256(ReadFile(source.sourcepath));
    source.resourcehash = slicer_core::ComputeSceneResourceHash(model);
    source.displayname = "R2 Worker cube";
    scene.models.push_back(source);

    slicer_core::SceneModelInstance item;
    item.instance.instanceid = "instance-r2-worker-executor";
    item.instance.modelid = source.modelid;
    item.instance.sourcetransformidentity = source.sourcepath.generic_string();
    item.instance.sourcebboxmm = model.bbox_mm;
    item.instance.transform.translatexmm = 10.0 - model.bbox_mm.min.x;
    item.instance.transform.translateymm = 10.0 - model.bbox_mm.min.y;
    item.instance.transformrevision = 4U;
    item.instance.effectivebboxmm = model.bbox_mm;
    item.instance.effectivebboxmm.min.x += item.instance.transform.translatexmm;
    item.instance.effectivebboxmm.max.x += item.instance.transform.translatexmm;
    item.instance.effectivebboxmm.min.y += item.instance.transform.translateymm;
    item.instance.effectivebboxmm.max.y += item.instance.transform.translateymm;
    item.requestedtransform = item.instance.transform;
    item.effectivetransform = item.instance.transform;
    item.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Admitted;
    item.resolvedprofileid = scene.resolvedprofileid;
    scene.instances.push_back(std::move(item));
    return scene;
}

worker::WorkerRequestEnvelope MakeRequest(
    const std::filesystem::path& jobDirectory,
    const std::filesystem::path& packageDirectory,
    const slicer_core::MultiModelScene& scene,
    const slicer_core::Json& profile,
    const bool transfer = false)
{
    std::filesystem::create_directories(jobDirectory);
    return worker::WorkerRequestEnvelope(
        worker::WorkerJobIdentity(
            jobDirectory.filename().generic_string(),
            "correlation-r2-executor",
            transfer ? "slice.rgbwsvt" : "slice.rgbwsv",
            jobDirectory / "request.json"),
        1U,
        transfer ? 1U : 0U,
        std::chrono::milliseconds(30000),
        "sha256:" + slicer_core::ComputeMultiModelSceneHash(scene),
        slicer_core::SerializeMultiModelScene(scene),
        profile,
        nullptr,
        slicer_core::Json::object({
            {"contract", transfer ? "p0.rgbwsvt.1" : "p0.rgbwsv.2"},
            {"packageDir", packageDirectory.generic_string()},
        }));
}

class CandidateTransferFacade final : public api::SliceFacade
{
public:
    [[nodiscard]] api::ApiResult<api::SliceResult> Run(
        const api::SliceRequest& request,
        const api::ICancelToken&,
        const api::ProgressSink&) noexcept override
    {
        try
        {
            const std::filesystem::path profilePath =
                request.scene_config_path.parent_path()
                / "direct-candidate-profile.json";
            std::ofstream output(
                profilePath, std::ios::binary | std::ios::trunc);
            output << MakeProfile(request.package_dir, true).dump(2) << '\n';
            output.close();
            if (!output)
            {
                throw std::runtime_error("failed to write candidate profile");
            }
            slicer_core::SliceRunOptions options;
            options.write_preview_files = false;
            const slicer_core::SliceRunResult produced =
                slicer_core::run_slicer(profilePath, options);
            api::SliceResult result;
            result.package_dir = produced.package_dir;
            result.manifest_path = produced.package_dir / "manifest.json";
            result.layer_count = produced.layer_count;
            result.grid_px = {produced.width_px, produced.height_px};
            result.engine_version = "candidate-transfer-test";
            result.profile = produced.profile;
            return api::ApiResult<api::SliceResult>::Success(
                std::move(result));
        }
        catch (const std::exception& error)
        {
            return api::ApiResult<api::SliceResult>::Failure(api::ApiError{
                "PM-SLICER-INTERNAL-0099",
                "candidate transfer test facade failed",
                error.what()});
        }
    }
};

void TestRealTransferProductionExecution(const std::filesystem::path& root)
{
    slicer_core::SliceConfig loadConfig;
    loadConfig.input.model_path = TransferFixturePath();
    loadConfig.input.format = "obj";
    loadConfig.auto_orient.enabled = false;
    const slicer_core::SceneModel model = slicer_core::load_model_report(
        loadConfig, TransferFixturePath().parent_path());
    const slicer_core::MultiModelScene scene = MakeScene(model, true);
    const std::filesystem::path packageDirectory =
        root / "package-transfer-success";
    const slicer_core::Json profile = MakeProfile(packageDirectory, true);
    std::ostringstream protocol;
    worker::WorkerSliceExecutor executor(
        slicer_core::engine::CreateProductionPreflightFullFacade(),
        slicer_core::engine::CreateProductionSliceFacade(),
        protocol);
    const worker::WorkerCapabilityExecutionResult result = executor.Execute(
        MakeRequest(
            root / "job-transfer-success",
            packageDirectory,
            scene,
            profile,
            true),
        TestCancelToken{});
    if (!result.Ok())
    {
        std::cerr << "RGBWSVT executor failure: code=" << result.Code()
                  << " message=" << result.Message();
        if (result.Detail().has_value())
        {
            std::cerr << " detail=" << *result.Detail();
        }
        std::cerr << '\n';
    }
    Check(result.Ok(), "explicit RGBWSVT Worker Scene execution succeeds");
    if (result.Ok())
    {
        const slicer_core::RgbwsvtPackageValidation package =
            slicer_core::ValidateRgbwsvtPackage(packageDirectory);
        Check(
            package.productionAcceptance == "admitted",
            "Worker Scene RGBWSVT package is explicitly admitted");
    }
}

void TestCandidateTransferPackageIsRejectedAndRemoved(
    const std::filesystem::path& root)
{
    slicer_core::SliceConfig loadConfig;
    loadConfig.input.model_path = TransferFixturePath();
    loadConfig.input.format = "obj";
    loadConfig.auto_orient.enabled = false;
    const slicer_core::SceneModel model = slicer_core::load_model_report(
        loadConfig, TransferFixturePath().parent_path());
    const slicer_core::MultiModelScene scene = MakeScene(model, true);
    const std::filesystem::path packageDirectory =
        root / "package-transfer-candidate";
    const std::filesystem::path jobDirectory =
        root / "job-transfer-candidate";
    const slicer_core::Json profile = MakeProfile(packageDirectory, true);
    const auto artifactIdentity =
        slicer_core::api::artifacts::MakePackageArtifactIdentity(
            packageDirectory,
            jobDirectory.filename().generic_string(),
            slicer_core::api::artifacts::MakePackageAttemptId(
                "correlation-r2-executor"));
    std::ostringstream protocol;
    worker::WorkerSliceExecutor executor(
        slicer_core::engine::CreateProductionPreflightFullFacade(),
        std::make_unique<CandidateTransferFacade>(),
        protocol);
    const worker::WorkerCapabilityExecutionResult result = executor.Execute(
        MakeRequest(
            jobDirectory,
            packageDirectory,
            scene,
            profile,
            true),
        TestCancelToken{});
    if (result.Ok() || result.Code() != "PM-SLICER-CONTRACT-0060")
    {
        std::cerr << "candidate executor result: ok=" << result.Ok()
                  << " code=" << result.Code()
                  << " message=" << result.Message();
        if (result.Detail().has_value())
        {
            std::cerr << " detail=" << *result.Detail();
        }
        std::cerr << '\n';
    }
    Check(!result.Ok() && result.Code() == "PM-SLICER-CONTRACT-0060",
        "Worker rejects a structurally valid RGBWSVT candidate package");
    Check(!std::filesystem::exists(packageDirectory),
        "Worker removes the newly published rejected candidate package");
    Check(!std::filesystem::exists(artifactIdentity.staging_directory)
            && !std::filesystem::exists(artifactIdentity.backup_directory)
            && !std::filesystem::exists(artifactIdentity.lease_directory),
        "candidate rejection leaves no owned package artifacts");
}

void TestRealProductionExecution(
    const std::filesystem::path& root,
    const slicer_core::MultiModelScene& scene)
{
    const std::filesystem::path packageDirectory = root / "package-success";
    const slicer_core::Json profile = MakeProfile(packageDirectory);
    std::ostringstream protocol;
    worker::WorkerSliceExecutor executor(
        slicer_core::engine::CreateProductionPreflightFullFacade(),
        slicer_core::engine::CreateProductionSliceFacade(),
        protocol);
    const worker::WorkerCapabilityExecutionResult result = executor.Execute(
        MakeRequest(root / "job-success", packageDirectory, scene, profile),
        TestCancelToken{});
    if (!result.Ok())
    {
        std::cerr << "executor failure: code=" << result.Code()
                  << " message=" << result.Message();
        if (result.Detail().has_value())
        {
            std::cerr << " detail=" << *result.Detail();
        }
        std::cerr << '\n';
    }
    Check(result.Ok(), "production Worker slice executor succeeds");
    if (result.Ok())
    {
        Check(result.Output().at("packageDir").as_string()
                == packageDirectory.generic_string(),
            "Worker result preserves the caller package identity");
        Check(result.Output().at("layerCount").as_int() > 0,
            "Worker result exposes a positive layer count");
        Check(result.Output().at("grid").at("dpiX").as_int() == 127
                && result.Output().at("grid").at("dpiY").as_int() == 127,
            "Worker result echoes the effective production resolution");
        Check(result.Output().at("executionScope").as_string()
                == "14D-08-R2-02",
            "R2 execution-only response remains explicit");
        const slicer_core::Json& timing =
            result.Output().at("timing");
        const slicer_core::Json& imports =
            timing.at("imports");
        const slicer_core::Json& instances =
            timing.at("instances");
        Check(imports.size() == 1U
                && imports.at(0U).at("parseMs").as_double() >= 0.0
                && imports.at(0U).at("textureMs").is_null()
                && imports.at(0U).at("previewMs").is_null()
                && imports.at(0U).at("hashMs").as_double() >= 0.0,
            "Worker serializes honest import timing and null unavailable phases");
        Check(instances.size() == 1U
                && instances.at(0U).at("coreSliceMs").as_double() >= 0.0
                && instances.at(0U).at("composeMs").as_double() >= 0.0
                && instances.at(0U).at("layerCount").as_int() > 0,
            "Worker serializes one visible instance core and compose timing");
    }
    Check(std::filesystem::is_regular_file(packageDirectory / "manifest.json"),
        "real SliceFacade publishes a manifest");
    Check(protocol.str().find("SLICE_PROGRESS") != std::string::npos
            && protocol.str().find("percent=100") != std::string::npos
            && protocol.str().find("SLICE_TIMING") != std::string::npos,
        "reserved progress and timing lines are emitted");
}

void TestCancelledBeforeMaterialization(
    const std::filesystem::path& root,
    const slicer_core::MultiModelScene& scene)
{
    const std::filesystem::path packageDirectory = root / "package-cancelled";
    const slicer_core::Json profile = MakeProfile(packageDirectory);
    std::ostringstream protocol;
    worker::WorkerSliceExecutor executor(
        slicer_core::engine::CreateProductionPreflightFullFacade(),
        slicer_core::engine::CreateProductionSliceFacade(),
        protocol);
    const worker::WorkerCapabilityExecutionResult result = executor.Execute(
        MakeRequest(root / "job-cancelled", packageDirectory, scene, profile),
        TestCancelToken{true});
    Check(!result.Ok() && result.Code() == "PM-SLICER-CANCELLED-0070",
        "pre-cancelled production request fails with the stable code");
    Check(result.Cleanup().has_value()
            && result.Cleanup()->StagingRemoved()
            && !result.Cleanup()->Published(),
        "pre-cancelled request reports no staging or publication");
    Check(!std::filesystem::exists(packageDirectory),
        "pre-cancelled request creates no package");
}

void TestUncommittedAdmissionFailsClosed(
    const std::filesystem::path& root,
    slicer_core::MultiModelScene scene)
{
    scene.instances.front().admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Unknown;
    const std::filesystem::path packageDirectory =
        root / "package-uncommitted";
    const std::filesystem::path jobDirectory = root / "job-uncommitted";
    const auto artifactIdentity =
        slicer_core::api::artifacts::MakePackageArtifactIdentity(
            packageDirectory,
            jobDirectory.filename().generic_string(),
            slicer_core::api::artifacts::MakePackageAttemptId(
                "correlation-r2-executor"));
    std::filesystem::create_directories(
        artifactIdentity.staging_directory);
    const auto lease =
        slicer_core::api::artifacts::AcquirePackageArtifactLease(
            artifactIdentity);
    Check(lease.success, "test residue lease is created");
    const slicer_core::Json profile = MakeProfile(packageDirectory);
    std::ostringstream protocol;
    worker::WorkerSliceExecutor executor(
        slicer_core::engine::CreateProductionPreflightFullFacade(),
        slicer_core::engine::CreateProductionSliceFacade(),
        protocol);
    const worker::WorkerCapabilityExecutionResult result = executor.Execute(
        MakeRequest(
            jobDirectory,
            packageDirectory,
            scene,
            profile),
        TestCancelToken{});
    Check(!result.Ok() && result.Code() == "PM-SLICER-LAYOUT-0022",
        "uncommitted admission fails with the stable scene code");
    Check(!std::filesystem::exists(packageDirectory),
        "uncommitted admission publishes no package");
    Check(!std::filesystem::exists(artifactIdentity.staging_directory)
            && !std::filesystem::exists(artifactIdentity.lease_directory),
        "Worker startup and exit cleanup remove exact owned residue before Writer");
}

}  // namespace

int main()
{
    const std::filesystem::path root = MakeTestRoot();
    try
    {
        slicer_core::SliceConfig loadConfig;
        loadConfig.input.model_path = FixturePath();
        loadConfig.input.format = "obj";
        loadConfig.auto_orient.enabled = false;
        const slicer_core::SceneModel model = slicer_core::load_model_report(
            loadConfig, FixturePath().parent_path());
        const slicer_core::MultiModelScene scene = MakeScene(model);
        TestRealProductionExecution(root, scene);
        TestRealTransferProductionExecution(root);
        TestCandidateTransferPackageIsRejectedAndRemoved(root);
        TestCancelledBeforeMaterialization(root, scene);
        TestUncommittedAdmissionFailsClosed(root, scene);
    }
    catch (const std::exception& error)
    {
        ++g_failures;
        std::cerr << "FAIL: unexpected test exception: " << error.what() << '\n';
    }

    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    if (g_failures != 0)
    {
        std::cerr << g_failures << " stage14d08 R2 executor test(s) failed\n";
        return 1;
    }
    std::cout << "stage14d08 R2 executor tests passed\n";
    return 0;
}
