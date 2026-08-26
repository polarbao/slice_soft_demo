#include "slicer_worker/runtime/WorkerRequestParser.h"
#include "slicer_worker/runtime/WorkerResultEnvelope.h"
#include "slicer_worker/runtime/WorkerResultWriter.h"

#include "slicer_core/json_value.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
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
        / ("slicesoft_stage14d08_r1_result_" + std::to_string(suffix));
    std::filesystem::create_directories(root);
    return root;
}

worker::WorkerRequestEnvelope MakeRequest(
    const std::filesystem::path& root,
    const std::string& jobId)
{
    const std::filesystem::path requestPath = root / jobId / "request.json";
    std::filesystem::create_directories(requestPath.parent_path());
    std::ofstream output{requestPath, std::ios::binary | std::ios::trunc};
    output
        << "{\"contract\":\"file_contract\",\"major\":1,\"minor\":0,"
        << "\"jobId\":\"" << jobId << "\","
        << "\"correlationId\":\"correlation-" << jobId << "\","
        << "\"capability\":\"geometry.repair\",\"timeoutMs\":5000,"
        << "\"input\":{\"mesh\":\"fixture.obj\"}}";
    output.close();
    return worker::WorkerRequestParser::Parse(requestPath);
}

worker::WorkerRequestEnvelope MakeTransferRequest(
    const std::filesystem::path& root,
    const std::string& jobId)
{
    const std::filesystem::path requestPath = root / jobId / "request.json";
    std::filesystem::create_directories(requestPath.parent_path());
    std::ofstream output{requestPath, std::ios::binary | std::ios::trunc};
    output
        << "{\"contract\":\"file_contract\",\"major\":1,\"minor\":1,"
        << "\"jobId\":\"" << jobId << "\","
        << "\"correlationId\":\"correlation-" << jobId << "\","
        << "\"capability\":\"slice.rgbwsvt\",\"timeoutMs\":5000,"
        << "\"sceneHash\":\"sha256:1234abcd\",\"scene\":{},\"profile\":{},"
        << "\"output\":{\"contract\":\"p0.rgbwsvt.1\","
        << "\"packageDir\":\"C:/worker/transfer-package\"}}";
    output.close();
    return worker::WorkerRequestParser::Parse(requestPath);
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    return slicer_core::Json::parse(input);
}

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

void TestSuccessAndAtomicReplacement(const std::filesystem::path& root)
{
    const worker::WorkerRequestEnvelope request = MakeRequest(root, "success-job");
    const worker::WorkerResultEnvelope success = worker::WorkerResultEnvelope::Success(
        request,
        slicer_core::Json::object({{"report", "repair-report.json"}}),
        "0.1.0",
        std::chrono::duration<double, std::milli>{12.5});
    worker::WorkerResultWriter::WriteAtomically(success);

    const slicer_core::Json first = ReadJson(request.Identity().ResultPath());
    Check(first.at("contract").as_string() == "file_contract",
        "success result preserves the file contract");
    Check(first.at("jobId").as_string() == request.Identity().JobId()
            && first.at("correlationId").as_string()
                == request.Identity().CorrelationId()
            && first.at("capability").as_string()
                == request.Identity().Capability(),
        "success result copies the trusted request identity");
    Check(first.at("ok").as_bool()
            && first.at("code").as_string() == "PM-SLICER-OK-0000",
        "success result uses the frozen success code");
    Check(first.at("output").at("report").as_string() == "repair-report.json",
        "success result contains a non-empty executor output");
    Check(success.ProcessExitCode() == 0, "success maps to process exit 0");
    Check(!std::filesystem::exists(request.Identity().ResultTemporaryPath()),
        "successful publication leaves no result.json.tmp");

    const worker::WorkerResultEnvelope failure = worker::WorkerResultEnvelope::Failure(
        request,
        "PM-SLICER-INTERNAL-0099",
        "executor is not installed",
        std::optional<std::string>{"production registry is empty"},
        "0.1.0",
        std::chrono::duration<double, std::milli>{13.0});
    worker::WorkerResultWriter::WriteAtomically(failure);
    const slicer_core::Json replaced = ReadJson(request.Identity().ResultPath());
    Check(!replaced.at("ok").as_bool()
            && replaced.at("code").as_string() == "PM-SLICER-INTERNAL-0099",
        "a later complete result atomically replaces the prior result");
    Check(replaced.at("error").at("message").as_string()
            == "executor is not installed",
        "failure result contains a non-empty message");
    Check(replaced.at("error").at("detail").as_string()
            == "production registry is empty",
        "failure result preserves optional detail");
    Check(failure.ProcessExitCode() == 1, "internal failure maps to process exit 1");
}

void TestTransferResultPreservesMinor(const std::filesystem::path& root)
{
    const worker::WorkerRequestEnvelope request =
        MakeTransferRequest(root, "transfer-result-job");
    const worker::WorkerResultEnvelope success =
        worker::WorkerResultEnvelope::Success(
            request,
            slicer_core::Json::object({{"packageDir", "C:/worker/transfer-package"}}),
            "0.1.0",
            std::chrono::duration<double, std::milli>{3.0});
    const slicer_core::Json document = success.ToJson();
    Check(document.at("minor").as_int() == 1,
        "transfer result echoes file-contract minor 1");
    Check(document.at("capability").as_string() == "slice.rgbwsvt",
        "transfer result preserves capability identity");
}

void TestEnvelopeValidation(const std::filesystem::path& root)
{
    const worker::WorkerRequestEnvelope request = MakeRequest(root, "validation-job");
    Check(ThrowsInvalidArgument([&request]()
        {
            (void)worker::WorkerResultEnvelope::Success(
                request, slicer_core::Json::object({}), "0.1.0",
                std::chrono::duration<double, std::milli>{1.0});
        }),
        "success with empty output is rejected");
    Check(ThrowsInvalidArgument([&request]()
        {
            (void)worker::WorkerResultEnvelope::Failure(
                request, "PM-SLICER-OK-0000", "not a failure", std::nullopt,
                "0.1.0", std::chrono::duration<double, std::milli>{1.0});
        }),
        "failure with success code is rejected");
    Check(ThrowsInvalidArgument([&request]()
        {
            (void)worker::WorkerResultEnvelope::Failure(
                request, "PM-SLICER-FUTURE-9999", "unknown code", std::nullopt,
                "0.1.0", std::chrono::duration<double, std::milli>{1.0});
        }),
        "failure with unknown stable code is rejected");
    Check(ThrowsInvalidArgument([&request]()
        {
            (void)worker::WorkerResultEnvelope::Failure(
                request, "PM-SLICER-INTERNAL-0099", "", std::nullopt,
                "0.1.0", std::chrono::duration<double, std::milli>{1.0});
        }),
        "failure with empty message is rejected");
    Check(ThrowsInvalidArgument([&request]()
        {
            (void)worker::WorkerResultEnvelope::Failure(
                request, "PM-SLICER-CANCELLED-0070", "cancelled", std::nullopt,
                "0.1.0", std::chrono::duration<double, std::milli>{1.0});
        }),
        "cancelled result without cleanup evidence is rejected");

    const worker::WorkerResultEnvelope cancelled =
        worker::WorkerResultEnvelope::Failure(
            request,
            "PM-SLICER-CANCELLED-0070",
            "job was cancelled",
            std::nullopt,
            "0.1.0",
            std::chrono::duration<double, std::milli>{2.0},
            worker::WorkerResultCleanup{true, false});
    const slicer_core::Json document = cancelled.ToJson();
    Check(cancelled.ProcessExitCode() == 8,
        "cancelled result maps to process exit 8");
    Check(document.at("cleanup").at("stagingRemoved").as_bool()
            && !document.at("cleanup").at("published").as_bool(),
        "cancelled result freezes required cleanup evidence");
}

void TestExitCategoryMapping(const std::filesystem::path& root)
{
    const worker::WorkerRequestEnvelope request = MakeRequest(root, "mapping-job");
    const std::pair<const char*, int> mappings[] = {
        {"PM-SLICER-INPUT-0002", 2},
        {"PM-SLICER-PROFILE-0030", 3},
        {"PM-SLICER-TOPOLOGY-0010", 4},
        {"PM-SLICER-RESOURCE-0040", 5},
        {"PM-SLICER-OUTPUT-0050", 6},
        {"PM-SLICER-CONTRACT-0060", 7},
        {"PM-SLICER-INTERNAL-0099", 1},
        {"PM-SLICER-LAYOUT-0020", 1}};
    for (const auto& [code, exitCode] : mappings)
    {
        const worker::WorkerResultEnvelope result =
            worker::WorkerResultEnvelope::Failure(
                request, code, "mapped failure", std::nullopt, "0.1.0",
                std::chrono::duration<double, std::milli>{1.0});
        Check(result.ProcessExitCode() == exitCode,
            std::string{code} + " maps to its frozen process category");
    }
}

void TestWriteFailures(const std::filesystem::path& root)
{
    {
        const worker::WorkerRequestEnvelope request = MakeRequest(root, "tmp-failure-job");
        std::filesystem::create_directory(request.Identity().ResultTemporaryPath());
        const worker::WorkerResultEnvelope result = worker::WorkerResultEnvelope::Failure(
            request, "PM-SLICER-INTERNAL-0099", "failure", std::nullopt, "0.1.0",
            std::chrono::duration<double, std::milli>{1.0});
        try
        {
            worker::WorkerResultWriter::WriteAtomically(result);
            Check(false, "temporary-open failure must throw");
        }
        catch (const worker::WorkerResultWriteError& error)
        {
            Check(error.StableCode() == "PM-SLICER-OUTPUT-0050"
                    && error.ProcessExitCode() == 6,
                "temporary-open failure has stable output semantics");
        }
        Check(!std::filesystem::exists(request.Identity().ResultPath()),
            "temporary-open failure publishes no result");
    }
    {
        const worker::WorkerRequestEnvelope request = MakeRequest(root, "replace-failure-job");
        std::filesystem::create_directory(request.Identity().ResultPath());
        const worker::WorkerResultEnvelope result = worker::WorkerResultEnvelope::Failure(
            request, "PM-SLICER-INTERNAL-0099", "failure", std::nullopt, "0.1.0",
            std::chrono::duration<double, std::milli>{1.0});
        try
        {
            worker::WorkerResultWriter::WriteAtomically(result);
            Check(false, "atomic replacement failure must throw");
        }
        catch (const worker::WorkerResultWriteError& error)
        {
            Check(error.ProcessExitCode() == 6,
                "atomic replacement failure maps to process exit 6");
        }
        Check(!std::filesystem::exists(request.Identity().ResultTemporaryPath()),
            "atomic replacement failure removes result.json.tmp");
        Check(std::filesystem::is_directory(request.Identity().ResultPath()),
            "atomic replacement failure does not delete the existing destination");
    }
}

}  // namespace

int main()
{
    const std::filesystem::path root = MakeTestRoot();
    try
    {
        TestSuccessAndAtomicReplacement(root);
        TestTransferResultPreservesMinor(root);
        TestEnvelopeValidation(root);
        TestExitCategoryMapping(root);
        TestWriteFailures(root);
    }
    catch (const std::exception& error)
    {
        Check(false, std::string{"unexpected test exception: "} + error.what());
    }
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    if (g_failures == 0)
    {
        std::cout << "Stage 14D-08-R1-02 worker result writer tests passed\n";
    }
    return g_failures == 0 ? 0 : 1;
}
