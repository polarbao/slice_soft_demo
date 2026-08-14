#include "slicer_worker/preflight/WorkerPreflightExecutor.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/config.h"
#include "slicer_core/engine/ProductionPreflightFullFacadeFactory.h"
#include "slicer_core/model.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>

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
        / ("slicesoft_stage14d08_r3_worker_" + std::to_string(suffix));
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

void WriteJson(
    const std::filesystem::path& path,
    const slicer_core::Json& value)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << value.dump(2);
    if (!output)
    {
        throw std::runtime_error("failed to write test JSON");
    }
}

slicer_core::Json MakeProfile(
    const std::filesystem::path& root)
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
            {"packageDir", (root / "unused-package").generic_string()},
            {"dpiX", 635},
            {"dpiY", 600},
            {"layerThicknessMm", 0.038},
            {"channelOrder", slicer_core::Json::array({"R", "G", "B", "W", "S", "V"})},
            {"bitDepth", 8},
            {"planarConfig", "contiguous"},
            {"storageMode", "stripped"},
            {"rowsPerStrip", 64},
        })},
        {"background", slicer_core::Json::object({{"value", 255}})},
        {"materialProcessProfile", slicer_core::Json::object({
            {"enabled", true},
            {"name", "profile-stage14d-r3-worker"},
            {"target", "stage14d-r3-worker-fixture"},
        })},
    };
    const slicer_core::Json withoutHash(profile);
    profile.emplace(
        "profileHash",
        api::ComputeProfileDocumentHash(withoutHash));
    return slicer_core::Json(std::move(profile));
}

slicer_core::MultiModelScene MakeScene(
    const slicer_core::SceneModel& model,
    const bool collision)
{
    slicer_core::MultiModelScene scene;
    scene.sceneid = collision
        ? "scene-r3-worker-collision"
        : "scene-r3-worker-pass";
    scene.scenerevision = 11U;
    scene.buildvolume = slicer_core::MakeDefaultDeviceBuildVolume();
    scene.resolvedprofileid = "profile-stage14d-r3-worker";

    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-r3-worker";
    scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = FixturePath().parent_path();
    scene.resourcescopes.push_back(scope);

    slicer_core::ModelSource source;
    source.modelid = "model-r3-worker";
    source.sourcepath = FixturePath();
    source.format = "obj";
    source.resourcescopeid = scope.resourcescopeid;
    source.sourcehash = slicer_core::ComputeSha256(ReadFile(source.sourcepath));
    source.resourcehash = slicer_core::ComputeSceneResourceHash(model);
    source.displayname = "R3 Worker cube";
    scene.models.push_back(source);

    const int instanceCount = collision ? 2 : 1;
    for (int index{0}; index < instanceCount; ++index)
    {
        slicer_core::SceneModelInstance item;
        item.instance.instanceid = "instance-r3-" + std::to_string(index + 1);
        item.instance.modelid = source.modelid;
        item.instance.sourcetransformidentity = source.resourcehash;
        item.instance.sourcebboxmm = model.bbox_mm;
        item.instance.transform.translatexmm = 10.0 - model.bbox_mm.min.x;
        item.instance.transform.translateymm = 10.0 - model.bbox_mm.min.y;
        item.instance.transformrevision = 3U;
        item.instance.effectivebboxmm = model.bbox_mm;
        item.instance.effectivebboxmm.min.x += item.instance.transform.translatexmm;
        item.instance.effectivebboxmm.max.x += item.instance.transform.translatexmm;
        item.instance.effectivebboxmm.min.y += item.instance.transform.translateymm;
        item.instance.effectivebboxmm.max.y += item.instance.transform.translateymm;
        item.requestedtransform = item.instance.transform;
        item.effectivetransform = item.instance.transform;
        item.resolvedprofileid = scene.resolvedprofileid;
        scene.instances.push_back(std::move(item));
    }
    return scene;
}

worker::WorkerRequestEnvelope MakeWorkerRequest(
    const std::filesystem::path& jobDirectory,
    const slicer_core::MultiModelScene& scene,
    const slicer_core::Json& profile)
{
    std::filesystem::create_directories(jobDirectory);
    const slicer_core::Json input = slicer_core::Json::object({
        {"mode", "full"},
        {"scene", slicer_core::SerializeMultiModelScene(scene)},
        {"sceneHash", "sha256:" + slicer_core::ComputeMultiModelSceneHash(scene)},
        {"expectedSceneRevision", scene.scenerevision},
        {"profile", profile},
        {"profileHash", profile.at("profileHash").as_string()},
        {"targetMode", "legacy"},
        {"buildVolume", slicer_core::SerializeMultiModelScene(scene).at("buildVolume")},
    });
    return worker::WorkerRequestEnvelope(
        worker::WorkerJobIdentity(
            jobDirectory.filename().generic_string(),
            "correlation-r3-worker",
            "geometry.preflight.full",
            jobDirectory / "request.json"),
        1U,
        0U,
        std::chrono::milliseconds(30000),
        std::nullopt,
        nullptr,
        nullptr,
        input,
        nullptr);
}

api::PreflightResult RunDirect(
    const std::filesystem::path& root,
    const slicer_core::MultiModelScene& scene,
    const slicer_core::Json& profile)
{
    const std::filesystem::path scenePath = root / "direct.scene.json";
    const std::filesystem::path profilePath = root / "direct.profile.json";
    WriteJson(scenePath, slicer_core::SerializeMultiModelScene(scene));
    WriteJson(profilePath, profile);
    api::PreflightRequest request;
    request.scene_config_path = scenePath;
    request.profile_config_path = profilePath;
    request.scene_hash = slicer_core::ComputeMultiModelSceneHash(scene);
    request.profile_hash = profile.at("profileHash").as_string();
    request.expected_scene_revision = scene.scenerevision;
    request.target_mode = "legacy";
    const std::unique_ptr<api::PreflightFullFacade> facade =
        slicer_core::engine::CreateProductionPreflightFullFacade();
    const api::ApiResult<api::PreflightResult> result = facade->RunFull(
        request, TestCancelToken{});
    Check(result.IsOk(), "direct production preflight succeeds");
    if (!result.IsOk() || result.Value() == nullptr)
    {
        return {};
    }
    return *result.Value();
}

void TestProductionFacadeAndWorker(
    const std::filesystem::path& root,
    const slicer_core::SceneModel& model,
    const slicer_core::Json& profile)
{
    const slicer_core::MultiModelScene scene = MakeScene(model, false);
    const api::PreflightResult direct = RunDirect(root, scene, profile);
    Check(direct.authoritative && direct.admitted && direct.complete,
        "closed fixture is authoritatively admitted");
    Check(direct.instances.size() == 1U
            && direct.checked_instance_count == 1U,
        "direct facade preserves instance evidence");

    worker::WorkerPreflightExecutor executor(
        slicer_core::engine::CreateProductionPreflightFullFacade());
    const worker::WorkerCapabilityExecutionResult workerResult = executor.Execute(
        MakeWorkerRequest(root / "job-pass", scene, profile),
        TestCancelToken{});
    Check(workerResult.Ok(), "Worker full preflight succeeds");
    if (workerResult.Ok())
    {
        const slicer_core::Json& output = workerResult.Output();
        Check(output.at("sceneId").as_string() == direct.scene_id,
            "Worker and direct facade preserve scene identity");
        Check(output.at("sceneRevision").as_int()
                == static_cast<int>(direct.scene_revision),
            "Worker and direct facade preserve scene revision");
        Check(output.at("checkedInstanceCount").as_int()
                == static_cast<int>(direct.checked_instance_count),
            "Worker and direct facade preserve checked instance count");
        Check(output.at("admission").as_string() == "passed",
            "Worker maps admitted business result without fallback");
    }
    Check(!std::filesystem::exists(root / "unused-package"),
        "preflight never creates a production package");
}

void TestBlockedAndFailureSemantics(
    const std::filesystem::path& root,
    const slicer_core::SceneModel& model,
    const slicer_core::Json& profile)
{
    const slicer_core::MultiModelScene collisionScene = MakeScene(model, true);
    worker::WorkerPreflightExecutor collisionExecutor(
        slicer_core::engine::CreateProductionPreflightFullFacade());
    const worker::WorkerCapabilityExecutionResult blocked =
        collisionExecutor.Execute(
            MakeWorkerRequest(root / "job-blocked", collisionScene, profile),
            TestCancelToken{});
    Check(blocked.Ok(), "topology/layout blocker is a successful business result");
    if (blocked.Ok())
    {
        Check(blocked.Output().at("admission").as_string() == "blocked",
            "blocked scene remains explicit in Worker output");
        Check(blocked.Output().at("collisions").size() == 1U,
            "collision evidence is preserved");
    }

    worker::WorkerPreflightExecutor cancelledExecutor(
        slicer_core::engine::CreateProductionPreflightFullFacade());
    const worker::WorkerCapabilityExecutionResult cancelled =
        cancelledExecutor.Execute(
            MakeWorkerRequest(root / "job-cancelled", MakeScene(model, false), profile),
            TestCancelToken{true});
    Check(!cancelled.Ok() && cancelled.Code() == "PM-SLICER-CANCELLED-0070",
        "pre-cancelled Worker request fails with stable cancellation code");
    Check(cancelled.Cleanup().has_value()
            && cancelled.Cleanup()->StagingRemoved()
            && !cancelled.Cleanup()->Published(),
        "cancelled preflight reports safe cleanup evidence");

    slicer_core::Json::Object staleInput = MakeWorkerRequest(
        root / "job-stale-source", MakeScene(model, false), profile)
        .Input().as_object();
    staleInput["sceneHash"] =
        "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    const worker::WorkerRequestEnvelope staleRequest(
        worker::WorkerJobIdentity(
            "job-stale", "correlation-r3-worker", "geometry.preflight.full",
            root / "job-stale" / "request.json"),
        1U, 0U, std::chrono::milliseconds(30000), std::nullopt,
        nullptr, nullptr, slicer_core::Json(std::move(staleInput)), nullptr);
    std::filesystem::create_directories(root / "job-stale");
    worker::WorkerPreflightExecutor staleExecutor(
        slicer_core::engine::CreateProductionPreflightFullFacade());
    const worker::WorkerCapabilityExecutionResult stale = staleExecutor.Execute(
        staleRequest, TestCancelToken{});
    Check(!stale.Ok() && stale.Code() == "PM-SLICER-LAYOUT-0022",
        "stale scene identity fails closed before model loading");
}

}  // namespace

int main()
{
    const std::filesystem::path root = MakeTestRoot();
    try
    {
        const slicer_core::Json profile = MakeProfile(root);
        const std::filesystem::path profilePath = root / "source.profile.json";
        WriteJson(profilePath, profile);
        const slicer_core::SliceConfig config = slicer_core::load_slice_config(
            profilePath);
        const slicer_core::SceneModel model = slicer_core::load_model_report(
            config, profilePath.parent_path());
        TestProductionFacadeAndWorker(root, model, profile);
        TestBlockedAndFailureSemantics(root, model, profile);
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
        std::cerr << g_failures << " stage14d08 R3 Worker test(s) failed\n";
        return 1;
    }
    std::cout << "stage14d08 R3 Worker preflight tests passed\n";
    return 0;
}
