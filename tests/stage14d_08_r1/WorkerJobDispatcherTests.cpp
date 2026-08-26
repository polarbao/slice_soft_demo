#include "slicer_worker/runtime/WorkerCapabilityExecutor.h"
#include "slicer_worker/runtime/WorkerJobDispatcher.h"
#include "slicer_worker/runtime/WorkerJobRuntime.h"
#include "slicer_worker/runtime/WorkerRequestParser.h"

#include "slicer_core/json_value.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace
{

namespace worker = slicesoft::worker;

int g_failures{0};

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
        / ("slicesoft_stage14d08_r1_dispatch_" + std::to_string(suffix));
    std::filesystem::create_directories(root);
    return root;
}

worker::WorkerRequestEnvelope MakeRequest(
    const std::filesystem::path& root,
    const std::string& jobId,
    const std::string& capability)
{
    const bool transferSlice = capability == "slice.rgbwsvt";
    const std::filesystem::path path = root / jobId / "request.json";
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output
        << "{\"contract\":\"file_contract\",\"major\":1,\"minor\":"
        << (transferSlice ? 1 : 0) << ','
        << "\"jobId\":\"" << jobId << "\",\"correlationId\":\"c-" << jobId << "\","
        << "\"capability\":\"" << capability << "\",\"timeoutMs\":5000";
    if (capability == "slice.rgbwsv" || transferSlice)
    {
        output
            << ",\"sceneHash\":\"sha256:1234abcd\",\"scene\":{},\"profile\":{},"
            << "\"output\":{\"contract\":\""
            << (transferSlice ? "p0.rgbwsvt.1" : "p0.rgbwsv.2") << "\","
            << "\"packageDir\":\"C:/worker/package\"}";
    }
    else
    {
        output << ",\"input\":{\"mesh\":\"fixture.obj\"}";
    }
    output << '}';
    output.close();
    return worker::WorkerRequestParser::Parse(path);
}

class RecordingExecutor final : public worker::IWorkerCapabilityExecutor
{
public:
    explicit RecordingExecutor(
        worker::WorkerCapabilityExecutionResult result,
        const bool mustThrow = false)
        : m_result(std::move(result)), m_mustThrow(mustThrow)
    {
    }

    [[nodiscard]] worker::WorkerCapabilityExecutionResult Execute(
        const worker::WorkerRequestEnvelope& request,
        const slicer_core::api::ICancelToken& cancelToken) override
    {
        ++m_calls;
        m_lastJobId = request.Identity().JobId();
        m_cancelObserved = cancelToken.IsCancelRequested();
        if (m_mustThrow)
        {
            throw std::runtime_error("test executor exception");
        }
        return m_result;
    }

    [[nodiscard]] int Calls() const noexcept
    {
        return m_calls;
    }

    [[nodiscard]] const std::string& LastJobId() const noexcept
    {
        return m_lastJobId;
    }

    [[nodiscard]] bool CancelObserved() const noexcept
    {
        return m_cancelObserved;
    }

private:
    worker::WorkerCapabilityExecutionResult m_result;
    bool m_mustThrow{false};
    int m_calls{0};
    std::string m_lastJobId;
    bool m_cancelObserved{false};
};

template <typename Callable>
bool ThrowsInvalidArgument(Callable&& callable)
{
    try
    {
        callable();
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    return false;
}

void TestExactDispatch(const std::filesystem::path& root)
{
    worker::WorkerJobDispatcher dispatcher;
    auto executor = std::make_unique<RecordingExecutor>(
        worker::WorkerCapabilityExecutionResult::Success(
            slicer_core::Json::object({{"report", "full.json"}})));
    RecordingExecutor* observer = executor.get();
    dispatcher.Register("geometry.preflight.full", std::move(executor));

    const worker::WorkerRequestEnvelope request =
        MakeRequest(root, "exact-job", "geometry.preflight.full");
    const worker::WorkerResultEnvelope result = dispatcher.Dispatch(request);
    Check(result.Ok() && result.ProcessExitCode() == 0,
        "exact registered capability dispatches successfully");
    Check(observer->Calls() == 1 && observer->LastJobId() == "exact-job",
        "exact executor is called once with the immutable request");
    Check(!observer->CancelObserved(),
        "normal dispatch observes no cancellation marker");
    Check(result.Identity().JobId() == request.Identity().JobId()
            && result.Identity().Capability() == request.Identity().Capability(),
        "dispatcher, not executor, owns result identity");

    Check(ThrowsInvalidArgument([&dispatcher]()
        {
            dispatcher.Register(
                "geometry.preflight.full",
                std::make_unique<RecordingExecutor>(
                    worker::WorkerCapabilityExecutionResult::Failure(
                        "PM-SLICER-INTERNAL-0099", "duplicate")));
        }),
        "duplicate executor registration is rejected");
    Check(ThrowsInvalidArgument([&dispatcher]()
        {
            dispatcher.Register("geometry.future", nullptr);
        }),
        "unknown capability registration is rejected before null fallback");
}

void TestTransferSliceDispatch(const std::filesystem::path& root)
{
    worker::WorkerJobDispatcher dispatcher;
    auto executor = std::make_unique<RecordingExecutor>(
        worker::WorkerCapabilityExecutionResult::Success(
            slicer_core::Json::object({{"package", "rgbwsvt"}})));
    RecordingExecutor* observer = executor.get();
    dispatcher.Register("slice.rgbwsvt", std::move(executor));

    const worker::WorkerRequestEnvelope request =
        MakeRequest(root, "transfer-job", "slice.rgbwsvt");
    const worker::WorkerResultEnvelope result = dispatcher.Dispatch(request);
    Check(result.Ok() && observer->Calls() == 1,
        "RGBWSVT capability is accepted and dispatched exactly once");
}

void TestFailClosedDispatch(const std::filesystem::path& root)
{
    const worker::WorkerRequestEnvelope request =
        MakeRequest(root, "missing-job", "geometry.repair");
    const worker::WorkerJobDispatcher emptyDispatcher;
    const worker::WorkerResultEnvelope missing = emptyDispatcher.Dispatch(request);
    Check(!missing.Ok()
            && missing.Code() == "PM-SLICER-INTERNAL-0099"
            && missing.ProcessExitCode() == 1,
        "missing production executor fails closed");

    worker::WorkerJobDispatcher throwingDispatcher;
    throwingDispatcher.Register(
        "geometry.repair",
        std::make_unique<RecordingExecutor>(
            worker::WorkerCapabilityExecutionResult::Failure(
                "PM-SLICER-INTERNAL-0099", "unused"),
            true));
    const worker::WorkerResultEnvelope thrown = throwingDispatcher.Dispatch(request);
    Check(!thrown.Ok() && thrown.Code() == "PM-SLICER-INTERNAL-0099",
        "executor exception becomes a stable internal failure");
    Check(thrown.ToJson().at("error").at("detail").as_string()
            == "test executor exception",
        "executor exception is retained only as failure detail");
}

void TestCancellationBeforeDispatch(const std::filesystem::path& root)
{
    const worker::WorkerRequestEnvelope request =
        MakeRequest(root, "cancel-job", "slice.rgbwsv");
    auto executor = std::make_unique<RecordingExecutor>(
        worker::WorkerCapabilityExecutionResult::Success(
            slicer_core::Json::object({{"package", "never"}})));
    RecordingExecutor* observer = executor.get();
    worker::WorkerJobDispatcher dispatcher;
    dispatcher.Register("slice.rgbwsv", std::move(executor));
    std::ofstream marker{request.Identity().CancelPath(), std::ios::binary};
    marker.close();

    const worker::WorkerResultEnvelope result = dispatcher.Dispatch(request);
    Check(!result.Ok()
            && result.Code() == "PM-SLICER-CANCELLED-0070"
            && result.ProcessExitCode() == 8,
        "pre-existing cancellation marker prevents dispatch");
    Check(observer->Calls() == 0,
        "pre-cancelled job never calls its executor");
}

void TestSharedRuntime(const std::filesystem::path& root)
{
    const worker::WorkerRequestEnvelope request =
        MakeRequest(root, "runtime-job", "geometry.repair");
    const worker::WorkerJobDispatcher dispatcher;
    const worker::WorkerJobRuntimeResult outcome =
        worker::WorkerJobRuntime::Run(request.Identity().RequestPath(), dispatcher);
    Check(outcome.processexitcode == 1
            && outcome.stablecode == "PM-SLICER-INTERNAL-0099",
        "runtime maps missing executor to stable internal exit");
    Check(outcome.trustedidentity && outcome.resultwritten,
        "runtime publishes an identity-closed failure after parsing succeeds");

    std::ifstream input{request.Identity().ResultPath(), std::ios::binary};
    const slicer_core::Json result = slicer_core::Json::parse(input);
    Check(result.at("jobId").as_string() == request.Identity().JobId()
            && result.at("capability").as_string() == request.Identity().Capability(),
        "runtime result identity matches the request");

    const std::filesystem::path invalidPath = root / "invalid" / "request.json";
    std::filesystem::create_directories(invalidPath.parent_path());
    std::ofstream invalid{invalidPath, std::ios::binary};
    invalid << "{\"contract\":\"future\"}";
    invalid.close();
    const worker::WorkerJobRuntimeResult rejected =
        worker::WorkerJobRuntime::Run(invalidPath, dispatcher);
    Check(rejected.processexitcode == 7
            && rejected.stablecode == "PM-SLICER-CONTRACT-0060",
        "untrusted contract failure maps to exit 7");
    Check(!rejected.trustedidentity && !rejected.resultwritten
            && !std::filesystem::exists(invalidPath.parent_path() / "result.json"),
        "untrusted request never guesses or writes a result path");
}

}  // namespace

int main()
{
    const std::filesystem::path root = MakeTestRoot();
    try
    {
        TestExactDispatch(root);
        TestTransferSliceDispatch(root);
        TestFailClosedDispatch(root);
        TestCancellationBeforeDispatch(root);
        TestSharedRuntime(root);
    }
    catch (const std::exception& error)
    {
        Check(false, std::string{"unexpected test exception: "} + error.what());
    }
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    if (g_failures == 0)
    {
        std::cout << "Stage 14D-08-R1-03 worker dispatcher tests passed\n";
    }
    return g_failures == 0 ? 0 : 1;
}
