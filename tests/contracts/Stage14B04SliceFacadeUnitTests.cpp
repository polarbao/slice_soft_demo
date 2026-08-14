#include "slicer_core/engine/SliceFacadeAdapter.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using slicer_core::api::ApiError;
using slicer_core::api::ApiResult;
using slicer_core::api::ProgressEvent;
using slicer_core::api::SliceRequest;
using slicer_core::api::SliceResult;
using slicer_core::engine::SliceFacadeAdapter;
using slicer_core::engine::SliceSubmissionContract;

class MutableCancelToken final : public slicer_core::api::ICancelToken
{
public:
    /** @brief 返回测试使用的取消状态。 @return 当前取消状态。 */
    [[nodiscard]] bool IsCancelRequested() const noexcept override
    {
        return m_requested.load();
    }

    /** @brief 修改测试使用的取消状态。 @param requested 新的取消状态。 */
    void SetRequested(const bool requested) noexcept
    {
        m_requested.store(requested);
    }

private:
    std::atomic_bool m_requested{false};
};

void Expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

SliceRequest MakeRequest(const std::filesystem::path& packageDirectory)
{
    SliceRequest request;
    request.job_id = "job-14b04";
    request.correlation_id = "correlation-14b04";
    request.scene_hash = "scene-hash-committed";
    request.scene_config_path = "scene.effective.json";
    request.package_dir = packageDirectory;
    return request;
}

SliceSubmissionContract MakeContract(const SliceRequest& request)
{
    SliceSubmissionContract contract;
    contract.scenehash = request.scene_hash;
    contract.packagedir = request.package_dir;
    return contract;
}

ApiResult<SliceResult> MakeSuccess(const SliceRequest& request)
{
    SliceResult result;
    result.package_dir = request.package_dir;
    result.manifest_path = request.package_dir / "manifest.json";
    result.layer_count = 3;
    result.grid_px = {17, 23};
    result.engine_version = "test-production-entry";
    result.elapsed_ms = 42U;
    return ApiResult<SliceResult>::Success(std::move(result));
}

std::vector<unsigned char> ReadBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    Expect(static_cast<bool>(input), "failed to read TIFF fixture");
    return std::vector<unsigned char>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

void VerifySuccessProgressAndBytePreservation(
    const std::filesystem::path& packageDirectory)
{
    const SliceRequest request = MakeRequest(packageDirectory);
    const std::filesystem::path layerPath =
        packageDirectory / "layers" / "layer_000000.tiff";
    const std::vector<unsigned char> expectedBytes{
        0x49U, 0x49U, 0x2AU, 0x00U, 0x08U, 0x00U,
        0x00U, 0x00U, 0x52U, 0x47U, 0x42U, 0x57U,
        0x53U, 0x56U};

    int resolverCalls{0};
    int runnerCalls{0};
    std::filesystem::path forwardedConfig;
    SliceFacadeAdapter facade(
        [&request, &resolverCalls](const std::filesystem::path&)
        {
            ++resolverCalls;
            return ApiResult<SliceSubmissionContract>::Success(
                MakeContract(request));
        },
        [&request,
         &runnerCalls,
         &forwardedConfig,
         &layerPath,
         &expectedBytes](
            const std::filesystem::path& configPath,
            const slicer_core::api::ProgressSink& progressSink)
        {
            ++runnerCalls;
            forwardedConfig = configPath;
            std::filesystem::create_directories(layerPath.parent_path());
            std::ofstream output(layerPath, std::ios::binary);
            output.write(
                reinterpret_cast<const char*>(expectedBytes.data()),
                static_cast<std::streamsize>(expectedBytes.size()));
            output.close();

            progressSink(ProgressEvent{"scene_model_load", 10, 0, 1, {}});
            progressSink(ProgressEvent{"scene_instance_slice", 9, 1, 3, {}});
            progressSink(ProgressEvent{"scene_package_write", 95, 3, 3, {}});
            progressSink(ProgressEvent{"completed", 100, 1, 1, {}});
            return MakeSuccess(request);
        });

    MutableCancelToken cancelToken;
    std::vector<ProgressEvent> events;
    const ApiResult<SliceResult> result = facade.Run(
        request,
        cancelToken,
        [&events](const ProgressEvent& event)
        {
            events.push_back(event);
        });

    Expect(result.IsOk(), "successful production run was rejected");
    Expect(resolverCalls == 1, "contract resolver call count differs");
    Expect(runnerCalls == 1, "production runner call count differs");
    Expect(
        forwardedConfig == request.scene_config_path,
        "effective config path was not forwarded exactly");
    Expect(events.size() == 4U, "progress event count differs");
    Expect(
        std::is_sorted(
            events.begin(),
            events.end(),
            [](const ProgressEvent& left, const ProgressEvent& right)
            {
                return left.percent < right.percent;
            }),
        "forwarded progress is not monotonic");
    Expect(events[1].percent == 10, "progress regression was not clamped");
    Expect(
        ReadBytes(layerPath) == expectedBytes,
        "SliceFacade adapter changed production TIFF bytes");
}

void VerifyPreCancelledRequestStopsBeforeResolution(
    const std::filesystem::path& packageDirectory)
{
    const SliceRequest request = MakeRequest(packageDirectory);
    int resolverCalls{0};
    int runnerCalls{0};
    SliceFacadeAdapter facade(
        [&resolverCalls](const std::filesystem::path&)
        {
            ++resolverCalls;
            return ApiResult<SliceSubmissionContract>::Failure(
                ApiError{});
        },
        [&runnerCalls](
            const std::filesystem::path&,
            const slicer_core::api::ProgressSink&)
        {
            ++runnerCalls;
            return ApiResult<SliceResult>::Failure(ApiError{});
        });

    MutableCancelToken cancelToken;
    cancelToken.SetRequested(true);
    const ApiResult<SliceResult> result = facade.Run(
        request,
        cancelToken,
        {});
    Expect(!result.IsOk(), "pre-cancelled request succeeded");
    Expect(
        result.Error() != nullptr
            && result.Error()->code == "PM-SLICER-CANCELLED-0070",
        "pre-cancelled request returned the wrong error");
    Expect(resolverCalls == 0, "pre-cancelled request resolved a contract");
    Expect(runnerCalls == 0, "pre-cancelled request invoked production");
}

void VerifyCancellationAtExistingProgressBoundary(
    const std::filesystem::path& packageDirectory)
{
    const SliceRequest request = MakeRequest(packageDirectory);
    MutableCancelToken cancelToken;
    int boundaryCalls{0};
    SliceFacadeAdapter facade(
        [&request](const std::filesystem::path&)
        {
            return ApiResult<SliceSubmissionContract>::Success(
                MakeContract(request));
        },
        [&cancelToken, &boundaryCalls](
            const std::filesystem::path&,
            const slicer_core::api::ProgressSink& progressSink)
        {
            progressSink(ProgressEvent{"scene_instance_slice", 30, 1, 3, {}});
            ++boundaryCalls;
            cancelToken.SetRequested(true);
            progressSink(ProgressEvent{"scene_instance_slice", 31, 1, 3, {}});
            return ApiResult<SliceResult>::Failure(ApiError{});
        });

    const ApiResult<SliceResult> result = facade.Run(
        request,
        cancelToken,
        {});
    Expect(!result.IsOk(), "mid-run cancellation succeeded");
    Expect(
        result.Error() != nullptr
            && result.Error()->code == "PM-SLICER-CANCELLED-0070",
        "mid-run cancellation returned the wrong error");
    Expect(boundaryCalls == 1, "unexpected cancellation boundary count");
}

void VerifySwallowedWriterCancellationIsRemapped(
    const std::filesystem::path& packageDirectory)
{
    const SliceRequest request = MakeRequest(packageDirectory);
    MutableCancelToken cancelToken;
    SliceFacadeAdapter facade(
        [&request](const std::filesystem::path&)
        {
            return ApiResult<SliceSubmissionContract>::Success(
                MakeContract(request));
        },
        [&cancelToken](
            const std::filesystem::path&,
            const slicer_core::api::ProgressSink& progressSink)
        {
            cancelToken.SetRequested(true);
            try
            {
                progressSink(ProgressEvent{"scene_package_write", 80, 1, 5, {}});
            }
            catch (const std::exception&)
            {
                return ApiResult<SliceResult>::Failure(
                    ApiError{
                        "PM-SLICER-CONTRACT-0060",
                        "writer translated callback failure",
                        {}});
            }
            return ApiResult<SliceResult>::Failure(ApiError{});
        });

    const ApiResult<SliceResult> result = facade.Run(
        request,
        cancelToken,
        {});
    Expect(!result.IsOk(), "translated writer cancellation succeeded");
    Expect(
        result.Error() != nullptr
            && result.Error()->code == "PM-SLICER-CANCELLED-0070",
        "translated writer cancellation was not remapped");
}

void VerifySubmissionIdentityGuards(
    const std::filesystem::path& packageDirectory)
{
    const SliceRequest request = MakeRequest(packageDirectory);
    MutableCancelToken cancelToken;
    int runnerCalls{0};
    SliceFacadeAdapter staleFacade(
        [&request](const std::filesystem::path&)
        {
            SliceSubmissionContract contract = MakeContract(request);
            contract.scenehash = "different-committed-scene";
            return ApiResult<SliceSubmissionContract>::Success(
                std::move(contract));
        },
        [&runnerCalls](
            const std::filesystem::path&,
            const slicer_core::api::ProgressSink&)
        {
            ++runnerCalls;
            return ApiResult<SliceResult>::Failure(ApiError{});
        });
    const ApiResult<SliceResult> stale = staleFacade.Run(
        request,
        cancelToken,
        {});
    Expect(
        !stale.IsOk() && stale.Error() != nullptr
            && stale.Error()->code == "PM-SLICER-LAYOUT-0022",
        "stale scene hash was not rejected");
    Expect(runnerCalls == 0, "stale scene invoked production");

    SliceFacadeAdapter pathFacade(
        [&request](const std::filesystem::path&)
        {
            SliceSubmissionContract contract = MakeContract(request);
            contract.packagedir = request.package_dir / "different";
            return ApiResult<SliceSubmissionContract>::Success(
                std::move(contract));
        },
        [&runnerCalls](
            const std::filesystem::path&,
            const slicer_core::api::ProgressSink&)
        {
            ++runnerCalls;
            return ApiResult<SliceResult>::Failure(ApiError{});
        });
    const ApiResult<SliceResult> wrongPath = pathFacade.Run(
        request,
        cancelToken,
        {});
    Expect(
        !wrongPath.IsOk() && wrongPath.Error() != nullptr
            && wrongPath.Error()->code == "PM-SLICER-PROFILE-0031",
        "package identity mismatch was not rejected");
    Expect(runnerCalls == 0, "package mismatch invoked production");
}

void VerifyFacadeFailureBoundaries(
    const std::filesystem::path& packageDirectory)
{
    const SliceRequest request = MakeRequest(packageDirectory);
    MutableCancelToken cancelToken;
    SliceFacadeAdapter observerFacade(
        [&request](const std::filesystem::path&)
        {
            return ApiResult<SliceSubmissionContract>::Success(
                MakeContract(request));
        },
        [](const std::filesystem::path&,
           const slicer_core::api::ProgressSink& progressSink)
        {
            progressSink(ProgressEvent{"scene_model_load", 10, 0, 1, {}});
            return ApiResult<SliceResult>::Failure(ApiError{});
        });
    const ApiResult<SliceResult> observerFailure = observerFacade.Run(
        request,
        cancelToken,
        [](const ProgressEvent&)
        {
            throw std::runtime_error("observer failure");
        });
    Expect(
        !observerFailure.IsOk() && observerFailure.Error() != nullptr
            && observerFailure.Error()->code == "PM-SLICER-INTERNAL-0099",
        "progress observer exception crossed the facade");

    SliceFacadeAdapter swallowedObserverFacade(
        [&request](const std::filesystem::path&)
        {
            return ApiResult<SliceSubmissionContract>::Success(
                MakeContract(request));
        },
        [](const std::filesystem::path&,
           const slicer_core::api::ProgressSink& progressSink)
        {
            try
            {
                progressSink(
                    ProgressEvent{"scene_package_write", 80, 1, 5, {}});
            }
            catch (const std::exception&)
            {
                return ApiResult<SliceResult>::Failure(
                    ApiError{
                        "PM-SLICER-CONTRACT-0060",
                        "writer translated observer failure",
                        {}});
            }
            return ApiResult<SliceResult>::Failure(ApiError{});
        });
    const ApiResult<SliceResult> swallowedObserverFailure =
        swallowedObserverFacade.Run(
            request,
            cancelToken,
            [](const ProgressEvent&)
            {
                throw std::runtime_error("observer failure in package write");
            });
    Expect(
        !swallowedObserverFailure.IsOk()
            && swallowedObserverFailure.Error() != nullptr
            && swallowedObserverFailure.Error()->code
                == "PM-SLICER-INTERNAL-0099",
        "translated package-write observer failure lost its identity");

    SliceRequest incomplete = request;
    incomplete.job_id.clear();
    const ApiResult<SliceResult> invalid = observerFacade.Run(
        incomplete,
        cancelToken,
        {});
    Expect(
        !invalid.IsOk() && invalid.Error() != nullptr
            && invalid.Error()->code == "PM-SLICER-PROFILE-0030",
        "incomplete slice request was not rejected");

    SliceFacadeAdapter badResultFacade(
        [&request](const std::filesystem::path&)
        {
            return ApiResult<SliceSubmissionContract>::Success(
                MakeContract(request));
        },
        [&request](
            const std::filesystem::path&,
            const slicer_core::api::ProgressSink&)
        {
            SliceResult result;
            result.package_dir = request.package_dir / "unexpected";
            return ApiResult<SliceResult>::Success(std::move(result));
        });
    const ApiResult<SliceResult> badResult = badResultFacade.Run(
        request,
        cancelToken,
        {});
    Expect(
        !badResult.IsOk() && badResult.Error() != nullptr
            && badResult.Error()->code == "PM-SLICER-CONTRACT-0060",
        "invalid production package identity was accepted");
}

int RunTests()
{
    const std::filesystem::path testRoot =
        std::filesystem::current_path()
        / "build-slicesoft"
        / "stage14b04"
        / "unit-package";
    std::filesystem::remove_all(testRoot);
    std::filesystem::create_directories(testRoot);

    VerifySuccessProgressAndBytePreservation(testRoot / "success");
    VerifyPreCancelledRequestStopsBeforeResolution(testRoot / "pre-cancel");
    VerifyCancellationAtExistingProgressBoundary(testRoot / "mid-cancel");
    VerifySwallowedWriterCancellationIsRemapped(testRoot / "writer-cancel");
    VerifySubmissionIdentityGuards(testRoot / "identity");
    VerifyFacadeFailureBoundaries(testRoot / "failures");

    std::filesystem::remove_all(testRoot);
    return 0;
}

}  // namespace

int main()
{
    try
    {
        const int result = RunTests();
        std::cout << "Stage 14B-04 SliceFacade unit tests: PASS\n";
        return result;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Stage 14B-04 SliceFacade unit tests: FAIL: "
                  << exception.what() << '\n';
        return 1;
    }
}
