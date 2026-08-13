#include "slicer_core/api/Cancellation.h"
#include "slicer_core/engine/SliceFacadeAdapter.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"
#include "slicer_core/pipeline/SceneLayerComposer.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr std::size_t kChannelCount{6U};

class CountingCancelToken final : public slicer_core::api::ICancelToken
{
public:
    /**
     * @brief Creates a token that permits a fixed number of checks.
     * @param allowedChecks Number of false results before cancellation.
     */
    explicit CountingCancelToken(const std::size_t allowedChecks) noexcept
        : m_allowedChecks(allowedChecks)
    {
    }

    /**
     * @brief Requests cancellation after the configured number of checks.
     * @return True after the configured check budget is exhausted.
     */
    [[nodiscard]] bool IsCancelRequested() const noexcept override
    {
        return ++m_checkCount > m_allowedChecks;
    }

private:
    std::size_t m_allowedChecks{0U};
    mutable std::size_t m_checkCount{0U};
};

class NeverCancelToken final : public slicer_core::api::ICancelToken
{
public:
    /**
     * @brief Keeps normal production on the cancellable execution path.
     * @return Always false.
     */
    [[nodiscard]] bool IsCancelRequested() const noexcept override
    {
        return false;
    }
};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

slicer_core::SceneRasterGrid MakeGrid(
    const int width,
    const int height)
{
    slicer_core::SceneRasterGrid grid;
    grid.widthpx = width;
    grid.heightpx = height;
    grid.layercount = 1;
    grid.pitchxmm = 0.04;
    grid.pitchymm = 0.042333333333333334;
    grid.layerthicknessmm = 0.038;
    return grid;
}

slicer_core::SceneInstanceRaster MakeEmptyInstance(
    const slicer_core::SceneRasterGrid& grid)
{
    slicer_core::SceneInstanceRaster instance;
    instance.sceneid = "cancel-scene";
    instance.modelid = "cancel-model";
    instance.instanceid = "cancel-instance";
    instance.scenerevision = 1U;
    instance.transformrevision = 1U;
    instance.admittedtransformrevision = 1U;
    instance.transformhash = "identity";
    instance.admittedtransformhash = "identity";
    instance.admitted = true;
    instance.localgrid = grid;
    instance.protocol = slicer_core::FixedSceneRasterProtocol();

    const std::size_t pixelCount =
        static_cast<std::size_t>(grid.widthpx)
        * static_cast<std::size_t>(grid.heightpx);
    slicer_core::SceneInstanceRasterLayer layer;
    layer.layerindex = 0;
    layer.zmm = 0.019;
    layer.output.layerIndex = 0;
    layer.output.zMm = 0.019;
    layer.output.widthPx = grid.widthpx;
    layer.output.heightPx = grid.heightpx;
    layer.output.channelOrder = instance.protocol.channel_order;
    layer.output.channels.assign(
        pixelCount * kChannelCount,
        instance.protocol.empty_value);
    layer.modelownership.assign(pixelCount, 0U);
    layer.modelvarnishownership.assign(pixelCount, 0U);
    layer.outervarnishownership.assign(pixelCount, 0U);
    layer.supportownership.assign(pixelCount, 0U);
    instance.layers.push_back(std::move(layer));
    return instance;
}

slicer_core::SceneLayerComposeRequest MakeComposeRequest(
    const int width,
    const int height)
{
    slicer_core::SceneLayerComposeRequest request;
    request.sceneid = "cancel-scene";
    request.currentscenerevision = 1U;
    request.expectedscenerevision = 1U;
    request.admissionpassed = true;
    request.globalgrid = MakeGrid(width, height);
    request.protocol = slicer_core::FixedSceneRasterProtocol();
    request.instances.push_back(MakeEmptyInstance(request.globalgrid));
    return request;
}

bool ComposerStopsBeforeWork()
{
    CountingCancelToken token{0U};
    slicer_core::SceneLayerComposeRequest request =
        MakeComposeRequest(32, 32);
    request.canceltoken = &token;
    const auto start = std::chrono::steady_clock::now();
    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    return ExpectTrue(!result.available, "cancelled composition is unavailable")
        && ExpectTrue(result.layers.empty(), "cancelled composition publishes no layers")
        && ExpectTrue(result.error.has_value(), "cancelled composition has an error")
        && ExpectTrue(
            result.error->code == slicer_core::SceneRasterErrorCode::Cancelled,
            "cancelled composition has the stable code")
        && ExpectTrue(elapsed.count() < 2000, "pre-cancelled composition stops within 2 seconds");
}

bool FacadeForwardsCallerToken()
{
    const std::filesystem::path packageDir =
        std::filesystem::temp_directory_path() / "slicesoft_14d04a_facade";
    slicer_core::api::SliceRequest request;
    request.job_id = "cancel-job";
    request.correlation_id = "cancel-correlation";
    request.scene_hash = "cancel-scene-hash";
    request.scene_config_path = "cancel-scene.json";
    request.package_dir = packageDir;

    NeverCancelToken token;
    bool sameToken{false};
    slicer_core::engine::SliceFacadeAdapter facade(
        [&request](const std::filesystem::path&)
        {
            slicer_core::engine::SliceSubmissionContract contract;
            contract.scenehash = request.scene_hash;
            contract.packagedir = request.package_dir;
            return slicer_core::api::ApiResult<
                slicer_core::engine::SliceSubmissionContract>::Success(
                    std::move(contract));
        },
        slicer_core::engine::CancellableSliceProductionRunner{
            [&token, &sameToken, &request](
                const slicer_core::api::SliceRequest& runnerRequest,
                const slicer_core::api::ICancelToken& runnerToken,
                const slicer_core::api::ProgressSink&)
            {
                sameToken = &runnerToken == &token;
                slicer_core::api::SliceResult result;
                result.package_dir = runnerRequest.package_dir;
                result.manifest_path =
                    runnerRequest.package_dir / "manifest.json";
                return slicer_core::api::ApiResult<
                    slicer_core::api::SliceResult>::Success(
                        std::move(result));
            }});

    const auto result = facade.Run(request, token, {});
    return ExpectTrue(result.IsOk(), "facade accepts a normal cancellable run")
        && ExpectTrue(sameToken, "facade forwards the caller token by reference");
}

bool ComposerStopsInsideLongValidation()
{
    CountingCancelToken token{12U};
    slicer_core::SceneLayerComposeRequest request =
        MakeComposeRequest(1024, 1024);
    request.canceltoken = &token;
    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);

    return ExpectTrue(!result.available, "mid-loop cancellation is unavailable")
        && ExpectTrue(result.layers.empty(), "mid-loop cancellation publishes no layers")
        && ExpectTrue(result.error.has_value(), "mid-loop cancellation has an error")
        && ExpectTrue(
            result.error->code == slicer_core::SceneRasterErrorCode::Cancelled,
            "mid-loop cancellation has the stable code");
}

slicer_core::RgbwsvProductionPackageWriteRequest MakeWriteRequest(
    const std::filesystem::path& packageDir)
{
    constexpr int width{32};
    constexpr int height{24};
    slicer_core::RgbwsvProductionPackageWriteRequest request;
    request.packageDir = packageDir;
    request.sourceConfigPath = "fixture/config.json";
    request.sourceModelPath = "fixture/model.obj";
    request.sourceFormat = "obj";
    request.requestedPipelineMode = "legacy";
    request.effectivePipelineMode = "legacy";
    request.productionAcceptance = "admitted";
    request.grid.widthPx = width;
    request.grid.heightPx = height;
    request.grid.layerCount = 1;
    request.grid.dpiX = 635;
    request.grid.dpiY = 600;
    request.grid.pixelSizeXmm = 25.4 / 635.0;
    request.grid.pixelSizeYmm = 25.4 / 600.0;
    request.grid.layerThicknessMm = 0.038;
    request.preview.enabled = false;

    slicer_core::RgbwsvProductionLayer layer;
    layer.layerIndex = 0;
    layer.zMm = 0.019;
    layer.widthPx = width;
    layer.heightPx = height;
    layer.channelOrder = slicer_core::CurrentRgbwsvProtocol().channel_order;
    layer.channels.assign(
        static_cast<std::size_t>(width * height) * kChannelCount,
        slicer_core::CurrentRgbwsvProtocol().empty_value);
    request.layers.push_back(std::move(layer));
    return request;
}

bool WriterRemovesCancelledStagingOutput()
{
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / ("slicesoft_14d04a_" + std::to_string(suffix));
    std::filesystem::create_directories(root);
    const std::filesystem::path packageDir = root / "package";
    CountingCancelToken token{4U};
    auto request = MakeWriteRequest(packageDir);
    request.canceltoken = &token;

    bool cancelled{false};
    const auto start = std::chrono::steady_clock::now();
    try
    {
        (void)slicer_core::WriteRgbwsvProductionPackage(request);
    }
    catch (const std::exception& error)
    {
        cancelled = std::string{error.what()}.find("cancel")
            != std::string::npos;
    }

    bool stagingExists{false};
    if (std::filesystem::exists(root))
    {
        for (const auto& entry : std::filesystem::directory_iterator(root))
        {
            stagingExists = stagingExists
                || entry.path().filename().string().find(".staging.")
                    != std::string::npos;
        }
    }
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    return ExpectTrue(cancelled, "writer reports cooperative cancellation")
        && ExpectTrue(!std::filesystem::exists(packageDir), "writer publishes no cancelled package")
        && ExpectTrue(!stagingExists, "writer removes cancelled staging output")
        && ExpectTrue(elapsed.count() < 2000, "cancelled writer stops within 2 seconds");
}

std::vector<std::uint8_t> ReadBytes(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

std::vector<std::filesystem::path> RelativeFiles(
    const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (entry.is_regular_file())
        {
            files.push_back(std::filesystem::relative(entry.path(), root));
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

bool NormalWriterOutputIsUnchanged()
{
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / ("slicesoft_14d04a_normal_" + std::to_string(suffix));
    std::filesystem::create_directories(root);
    const std::filesystem::path baselineDir = root / "baseline";
    const std::filesystem::path cancellableDir = root / "cancellable";
    (void)slicer_core::WriteRgbwsvProductionPackage(
        MakeWriteRequest(baselineDir));
    NeverCancelToken token;
    auto cancellableRequest = MakeWriteRequest(cancellableDir);
    cancellableRequest.canceltoken = &token;
    (void)slicer_core::WriteRgbwsvProductionPackage(cancellableRequest);

    const std::vector<std::filesystem::path> baselineFiles =
        RelativeFiles(baselineDir);
    const std::vector<std::filesystem::path> cancellableFiles =
        RelativeFiles(cancellableDir);
    bool same = baselineFiles == cancellableFiles;
    for (const std::filesystem::path& relativePath : baselineFiles)
    {
        same = same
            && ReadBytes(baselineDir / relativePath)
                == ReadBytes(cancellableDir / relativePath);
    }
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    return ExpectTrue(same, "non-cancelled token preserves production bytes");
}

bool CancelledWriterPreservesExistingPackage()
{
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / ("slicesoft_14d04a_existing_" + std::to_string(suffix));
    std::filesystem::create_directories(root);
    const std::filesystem::path packageDir = root / "package";
    (void)slicer_core::WriteRgbwsvProductionPackage(
        MakeWriteRequest(packageDir));
    const std::vector<std::filesystem::path> expectedFiles =
        RelativeFiles(packageDir);
    std::vector<std::vector<std::uint8_t>> expectedBytes;
    for (const std::filesystem::path& relativePath : expectedFiles)
    {
        expectedBytes.push_back(ReadBytes(packageDir / relativePath));
    }

    CountingCancelToken token{4U};
    auto request = MakeWriteRequest(packageDir);
    request.canceltoken = &token;
    try
    {
        (void)slicer_core::WriteRgbwsvProductionPackage(request);
    }
    catch (const std::exception&)
    {
    }

    bool same = expectedFiles == RelativeFiles(packageDir);
    for (std::size_t index{0U}; index < expectedFiles.size(); ++index)
    {
        same = same
            && expectedBytes.at(index)
                == ReadBytes(packageDir / expectedFiles.at(index));
    }
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    return ExpectTrue(same, "cancellation preserves the existing valid package");
}

}  // namespace

int main()
{
    bool ok{true};
    ok = FacadeForwardsCallerToken() && ok;
    ok = ComposerStopsBeforeWork() && ok;
    ok = ComposerStopsInsideLongValidation() && ok;
    ok = WriterRemovesCancelledStagingOutput() && ok;
    ok = NormalWriterOutputIsUnchanged() && ok;
    ok = CancelledWriterPreservesExistingPackage() && ok;
    if (ok)
    {
        std::cout << "Stage 14D-04A cancellation tests passed\n";
        return 0;
    }
    return 1;
}
