#include "slicer_worker/runtime/WorkerRequestParser.h"
#include "slicer_worker/slice/WorkerSliceRequestMaterializer.h"

#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneEffectiveConfig.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace
{

namespace worker = slicesoft::worker;

int g_failures{0};

class TestCancelToken final : public slicer_core::api::ICancelToken
{
public:
    explicit TestCancelToken(const bool cancelled = false)
        : m_cancelled(cancelled)
    {
    }

    /** @brief Returns the configured deterministic cancellation state. */
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
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / ("slicesoft_stage14d08_r2_" + std::to_string(suffix));
    std::filesystem::create_directories(root);
    return std::filesystem::absolute(root).lexically_normal();
}

void WriteText(const std::filesystem::path& path, const std::string& value)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << value;
    if (!output)
    {
        throw std::runtime_error("failed to write test fixture");
    }
}

slicer_core::MultiModelScene MakeScene(
    const std::filesystem::path& modelPath)
{
    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-r2-fixture";
    scene.scenerevision = 1U;
    scene.buildvolume = slicer_core::MakeDefaultDeviceBuildVolume();
    scene.resolvedprofileid = "profile-r2";

    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-obj";
    scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = modelPath.parent_path();
    scene.resourcescopes.push_back(scope);

    slicer_core::ModelSource source;
    source.modelid = "model-r2";
    source.sourcepath = modelPath;
    source.format = "obj";
    source.resourcescopeid = scope.resourcescopeid;
    source.sourcehash = "source-hash-r2";
    source.resourcehash = "resource-hash-r2";
    source.displayname = "R2 fixture";
    scene.models.push_back(source);

    slicer_core::SceneModelInstance sceneInstance;
    sceneInstance.instance.instanceid = "instance-r2";
    sceneInstance.instance.modelid = source.modelid;
    sceneInstance.instance.sourcetransformidentity = "source-transform-r2";
    sceneInstance.instance.sourcebboxmm.min = {0.0, 0.0, 0.0};
    sceneInstance.instance.sourcebboxmm.max = {10.0, 20.0, 2.0};
    sceneInstance.instance.effectivebboxmm =
        sceneInstance.instance.sourcebboxmm;
    sceneInstance.effectivetransform = sceneInstance.instance.transform;
    sceneInstance.resolvedprofileid = scene.resolvedprofileid;
    scene.instances.push_back(sceneInstance);
    return scene;
}

slicer_core::Json MakeProfile(
    const std::filesystem::path& modelPath,
    const std::filesystem::path& packageDirectory)
{
    slicer_core::Json::Object profile{
        {"profileVersion", "1.0"},
        {"slicingMode", "closed_mesh_scanline"},
        {"slicePipeline", slicer_core::Json::object({{"mode", "legacy"}})},
        {"input", slicer_core::Json::object({
            {"modelPath", modelPath.generic_string()},
            {"format", "obj"},
        })},
        {"output", slicer_core::Json::object({
            {"packageDir", packageDirectory.generic_string()},
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
            {"name", "profile-r2"},
            {"target", "stage14d08-r2-fixture"},
        })},
    };
    slicer_core::Json withoutHash(profile);
    profile.emplace(
        "profileHash",
        worker::WorkerSliceRequestMaterializer::ComputeProfileHash(withoutHash));
    return slicer_core::Json(std::move(profile));
}

std::filesystem::path WriteRequest(
    const std::filesystem::path& jobDirectory,
    const slicer_core::MultiModelScene& scene,
    const slicer_core::Json& profile,
    const std::filesystem::path& packageDirectory,
    const std::string& sceneHashOverride = {},
    const std::string& profileHashOverride = {})
{
    slicer_core::Json::Object profileObject = profile.as_object();
    if (!profileHashOverride.empty())
    {
        profileObject["profileHash"] = profileHashOverride;
    }
    const std::string sceneHash = sceneHashOverride.empty()
        ? "sha256:" + slicer_core::ComputeMultiModelSceneHash(scene)
        : sceneHashOverride;
    const slicer_core::Json request = slicer_core::Json::object({
        {"contract", "file_contract"},
        {"major", 1},
        {"minor", 0},
        {"jobId", jobDirectory.filename().generic_string()},
        {"correlationId", "correlation-r2"},
        {"capability", "slice.rgbwsv"},
        {"timeoutMs", 30000},
        {"sceneHash", sceneHash},
        {"scene", slicer_core::SerializeMultiModelScene(scene)},
        {"profile", slicer_core::Json(std::move(profileObject))},
        {"output", slicer_core::Json::object({
            {"contract", "p0.rgbwsv.2"},
            {"packageDir", packageDirectory.generic_string()},
        })},
    });
    const std::filesystem::path requestPath = jobDirectory / "request.json";
    WriteText(requestPath, request.dump(2));
    return requestPath;
}

bool ExpectFailure(
    const std::filesystem::path& requestPath,
    const std::string& expectedCode,
    const TestCancelToken& cancelToken = TestCancelToken{})
{
    try
    {
        const worker::WorkerRequestEnvelope request =
            worker::WorkerRequestParser::Parse(requestPath);
        (void)worker::WorkerSliceRequestMaterializer::Materialize(
            request, cancelToken);
    }
    catch (const worker::WorkerSliceRequestMaterializationError& error)
    {
        return error.Code() == expectedCode;
    }
    catch (const std::exception& error)
    {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return false;
    }
    return false;
}

void TestPositiveMaterialization(const std::filesystem::path& root)
{
    const std::filesystem::path modelPath = root / "assets" / "model.obj";
    const std::filesystem::path packageDirectory = root / "packages" / "package-a";
    WriteText(modelPath, "o fixture\n");
    const slicer_core::MultiModelScene scene = MakeScene(modelPath);
    const slicer_core::Json profile = MakeProfile(modelPath, packageDirectory);
    const std::filesystem::path requestPath = WriteRequest(
        root / "job-positive", scene, profile, packageDirectory);

    const worker::WorkerRequestEnvelope request =
        worker::WorkerRequestParser::Parse(requestPath);
    const worker::WorkerSliceMaterialization result =
        worker::WorkerSliceRequestMaterializer::Materialize(
            request, TestCancelToken{});

    Check(std::filesystem::is_regular_file(result.SceneSnapshotPath()),
        "scene snapshot is materialized");
    Check(std::filesystem::is_regular_file(result.ProfilePath()),
        "Profile is materialized");
    Check(std::filesystem::is_regular_file(result.SceneConfigPath()),
        "scene effective config is materialized");
    Check(result.PackageDirectory() == packageDirectory,
        "request package directory remains authoritative");
    Check(result.SceneHash() == slicer_core::ComputeMultiModelSceneHash(scene),
        "internal scene hash is the plain canonical digest");
    Check(result.ProfileHash()
            == profile.at("profileHash").as_string(),
        "Profile hash is preserved after verification");
    Check(result.ProfileVersion() == "1.0",
        "Profile version is preserved");
    Check(!std::filesystem::exists(packageDirectory),
        "materialization does not create a production package");

    const slicer_core::SceneEffectiveConfigResult effective =
        slicer_core::ReadSceneEffectiveConfig(result.SceneConfigPath());
    Check(effective.IsValid(), "materialized effective config is readable");
    if (effective.IsValid())
    {
        Check(effective.document.at("identity").at("sceneHash").as_string()
                == result.SceneHash(),
            "effective config identity matches the committed scene");
        Check(effective.document.at("sliceContract")
                    .at("outputPackageDir").as_string()
                == packageDirectory.generic_string(),
            "effective config output path matches the request");
    }
}

void TestIdentityFailures(const std::filesystem::path& root)
{
    const std::filesystem::path modelPath = root / "assets" / "model.obj";
    const std::filesystem::path packageDirectory = root / "packages" / "package-b";
    const slicer_core::MultiModelScene scene = MakeScene(modelPath);
    const slicer_core::Json profile = MakeProfile(modelPath, packageDirectory);

    const std::filesystem::path sceneRequest = WriteRequest(
        root / "job-bad-scene",
        scene,
        profile,
        packageDirectory,
        "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    Check(ExpectFailure(sceneRequest, "PM-SLICER-LAYOUT-0022"),
        "scene hash mismatch fails closed");

    const std::filesystem::path profileRequest = WriteRequest(
        root / "job-bad-profile",
        scene,
        profile,
        packageDirectory,
        {},
        "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    Check(ExpectFailure(profileRequest, "PM-SLICER-PROFILE-0030"),
        "Profile hash mismatch fails closed");
}

void TestPathAndCancellationFailures(const std::filesystem::path& root)
{
    const std::filesystem::path modelPath = root / "assets" / "model.obj";
    const std::filesystem::path packageDirectory = root / "packages" / "package-c";
    slicer_core::MultiModelScene relativeScene = MakeScene(modelPath);
    relativeScene.models.front().sourcepath = "relative/model.obj";
    relativeScene.resourcescopes.front().rootpath = "relative";
    const slicer_core::Json relativeProfile = MakeProfile(modelPath, packageDirectory);
    const std::filesystem::path relativeRequest = WriteRequest(
        root / "job-relative", relativeScene, relativeProfile, packageDirectory);
    Check(ExpectFailure(relativeRequest, "PM-SLICER-INPUT-0001"),
        "relative model resources fail closed");

    const slicer_core::MultiModelScene scene = MakeScene(modelPath);
    const slicer_core::Json profile = MakeProfile(modelPath, packageDirectory);
    const std::filesystem::path cancelledRequest = WriteRequest(
        root / "job-cancelled", scene, profile, packageDirectory);
    Check(ExpectFailure(
            cancelledRequest,
            "PM-SLICER-CANCELLED-0070",
            TestCancelToken{true}),
        "pre-cancelled materialization uses the stable cancellation code");
    Check(!std::filesystem::exists(
            cancelledRequest.parent_path() / "scene.snapshot.json"),
        "cancelled materialization leaves no scene snapshot");
    Check(!std::filesystem::exists(
            cancelledRequest.parent_path() / "profile.effective.json"),
        "cancelled materialization leaves no Profile");
    Check(!std::filesystem::exists(
            cancelledRequest.parent_path() / "scene_config.effective.json"),
        "cancelled materialization leaves no scene effective config");
}

}  // namespace

int main()
{
    const std::filesystem::path root = MakeTestRoot();
    try
    {
        TestPositiveMaterialization(root);
        TestIdentityFailures(root);
        TestPathAndCancellationFailures(root);
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
        std::cerr << g_failures << " stage14d08 R2 test(s) failed\n";
        return 1;
    }
    std::cout << "stage14d08 R2 materializer tests passed\n";
    return 0;
}
