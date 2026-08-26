#include "slicer_worker/runtime/WorkerRequestParser.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

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
        / ("slicesoft_stage14d08_r1_" + std::to_string(suffix));
    std::filesystem::create_directories(root);
    return root;
}

void WriteBytes(
    const std::filesystem::path& path,
    const std::string& bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output)
    {
        throw std::runtime_error("failed to write request fixture");
    }
}

std::string SliceRequest(
    const std::string_view jobId = "job-01",
    const std::string_view timeout = "5000")
{
    return std::string{
        "{\"contract\":\"file_contract\",\"major\":1,\"minor\":0,"
        "\"jobId\":\""}
        + std::string{jobId}
        + "\",\"correlationId\":\"correlation-01\","
          "\"capability\":\"slice.rgbwsv\",\"timeoutMs\":"
        + std::string{timeout}
        + ",\"sceneHash\":\"sha256:1234abcd\","
          "\"scene\":{\"futureScene\":true},"
          "\"profile\":{\"futureProfile\":7},"
          "\"output\":{\"contract\":\"p0.rgbwsv.2\","
          "\"packageDir\":\"C:/worker/package\",\"futureOutput\":true},"
          "\"futureEnvelope\":{\"enabled\":true}}";
}

std::string TransferSliceRequest(
    const std::string_view capability = "slice.rgbwsvt",
    const std::string_view outputContract = "p0.rgbwsvt.1",
    const std::string_view minor = "1")
{
    return std::string{
        "{\"contract\":\"file_contract\",\"major\":1,\"minor\":"}
        + std::string{minor}
        + ",\"jobId\":\"transfer-job\","
          "\"correlationId\":\"transfer-correlation\",\"capability\":\""
        + std::string{capability}
        + "\",\"timeoutMs\":5000,\"sceneHash\":\"sha256:1234abcd\","
          "\"scene\":{},\"profile\":{},\"output\":{\"contract\":\""
        + std::string{outputContract}
        + "\",\"packageDir\":\"C:/worker/transfer-package\"}}";
}

std::string GeometryRequest(const std::string_view capability)
{
    return std::string{
        "{\"contract\":\"file_contract\",\"major\":1,\"minor\":0,"
        "\"jobId\":\"geometry-job\",\"correlationId\":\"geometry-correlation\","
        "\"capability\":\""}
        + std::string{capability}
        + "\",\"timeoutMs\":9000,\"input\":{\"mesh\":\"fixture.obj\"}}";
}

bool ExpectFailure(
    const std::filesystem::path& requestPath,
    const worker::WorkerRequestParseErrorCode expectedCode)
{
    try
    {
        (void)worker::WorkerRequestParser::Parse(requestPath);
    }
    catch (const worker::WorkerRequestParseError& error)
    {
        return error.Code() == expectedCode;
    }
    catch (const std::exception& error)
    {
        std::cerr << "unexpected exception type: " << error.what() << '\n';
        return false;
    }
    return false;
}

void TestValidSliceRequest(const std::filesystem::path& root)
{
    const std::filesystem::path requestPath = root / "slice" / "request.json";
    WriteBytes(requestPath, SliceRequest());
    const worker::WorkerRequestEnvelope first =
        worker::WorkerRequestParser::Parse(std::filesystem::absolute(requestPath));
    const worker::WorkerRequestEnvelope second =
        worker::WorkerRequestParser::Parse(std::filesystem::absolute(requestPath));

    const worker::WorkerJobIdentity& identity = first.Identity();
    Check(identity.JobId() == "job-01", "slice jobId is preserved");
    Check(identity.CorrelationId() == "correlation-01",
        "slice correlationId is preserved");
    Check(identity.Capability() == "slice.rgbwsv",
        "slice capability is preserved");
    Check(identity.RequestPath().is_absolute(), "request path is absolute");
    Check(identity.RequestPath() == std::filesystem::absolute(requestPath).lexically_normal(),
        "request path is normalized without changing its job directory identity");
    Check(identity.JobDirectory() == identity.RequestPath().parent_path(),
        "job directory is request parent");
    Check(identity.ResultPath() == identity.JobDirectory() / "result.json",
        "result path is frozen beside request");
    Check(identity.ResultTemporaryPath()
            == identity.JobDirectory() / "result.json.tmp",
        "temporary result path is frozen beside request");
    Check(identity.CancelPath() == identity.JobDirectory() / "cancel.requested",
        "cancel path is frozen beside request");
    Check(first.Major() == 1U && first.Minor() == 0U,
        "file contract version is preserved");
    Check(first.Timeout() == std::chrono::milliseconds{5000},
        "finite timeout is preserved");
    Check(first.SceneHash() == std::optional<std::string>{"sha256:1234abcd"},
        "scene hash is preserved");
    Check(first.HasScene() && first.Scene().at("futureScene").as_bool(),
        "raw scene object preserves unknown fields");
    Check(first.HasProfile() && first.Profile().at("futureProfile").as_int() == 7,
        "raw profile object preserves unknown fields");
    Check(first.HasOutput() && first.Output().at("futureOutput").as_bool(),
        "raw output object preserves unknown fields");
    Check(!first.HasInput(), "slice request does not invent an input object");
    Check(second.Identity().RequestPath() == identity.RequestPath()
            && second.Identity().ResultPath() == identity.ResultPath(),
        "repeated parsing produces the same canonical identity");
    Check(!std::filesystem::exists(identity.ResultPath())
            && !std::filesystem::exists(identity.ResultTemporaryPath())
            && !std::filesystem::exists(identity.CancelPath()),
        "parser creates no result, cancellation, or package artifacts");
}

void TestValidTransferSliceRequest(const std::filesystem::path& root)
{
    const std::filesystem::path requestPath =
        root / "transfer-slice" / "request.json";
    WriteBytes(requestPath, TransferSliceRequest());
    const worker::WorkerRequestEnvelope request =
        worker::WorkerRequestParser::Parse(requestPath);
    Check(request.Major() == 1U && request.Minor() == 1U,
        "transfer slice uses file-contract minor 1");
    Check(request.Identity().Capability() == "slice.rgbwsvt",
        "transfer slice capability is preserved");
    Check(request.Output().at("contract").as_string() == "p0.rgbwsvt.1",
        "transfer slice output contract is preserved");
}

void TestValidGeometryRequests(const std::filesystem::path& root)
{
    for (const std::string capability :
        {"geometry.preflight.full", "geometry.repair"})
    {
        const std::filesystem::path requestPath =
            root / capability / "request.json";
        WriteBytes(requestPath, GeometryRequest(capability));
        const worker::WorkerRequestEnvelope request =
            worker::WorkerRequestParser::Parse(requestPath);
        Check(request.Identity().Capability() == capability,
            "geometry capability is preserved exactly");
        Check(request.HasInput()
                && request.Input().at("mesh").as_string() == "fixture.obj",
            "geometry input object is preserved");
        Check(!request.HasScene() && !request.HasProfile() && !request.HasOutput(),
            "geometry request does not invent slice branches");
    }
}

void TestPathAndEncodingFailures(const std::filesystem::path& root)
{
    const std::filesystem::path validPath = root / "path" / "request.json";
    WriteBytes(validPath, SliceRequest());
    Check(ExpectFailure(std::filesystem::path{"request.json"},
            worker::WorkerRequestParseErrorCode::InvalidPath),
        "relative request path is rejected");
    Check(ExpectFailure(root / "missing" / "request.json",
            worker::WorkerRequestParseErrorCode::InvalidPath),
        "missing request file is rejected");
    Check(ExpectFailure(root,
            worker::WorkerRequestParseErrorCode::InvalidPath),
        "directory request path is rejected");

    const std::filesystem::path wrongName = root / "path" / "other.json";
    WriteBytes(wrongName, SliceRequest());
    Check(ExpectFailure(wrongName,
            worker::WorkerRequestParseErrorCode::InvalidPath),
        "request file with a different name is rejected");

    const std::filesystem::path emptyPath = root / "empty" / "request.json";
    WriteBytes(emptyPath, "");
    Check(ExpectFailure(emptyPath,
            worker::WorkerRequestParseErrorCode::InvalidEncoding),
        "empty request file is rejected");

    const std::filesystem::path bomPath = root / "bom" / "request.json";
    WriteBytes(bomPath, std::string{"\xEF\xBB\xBF"} + SliceRequest());
    Check(ExpectFailure(bomPath,
            worker::WorkerRequestParseErrorCode::InvalidEncoding),
        "UTF-8 BOM request is rejected");

    const std::filesystem::path malformedPath = root / "malformed" / "request.json";
    WriteBytes(malformedPath, "{not-json}");
    Check(ExpectFailure(malformedPath,
            worker::WorkerRequestParseErrorCode::InvalidJson),
        "malformed JSON request is rejected");
}

void TestContractFailures(const std::filesystem::path& root)
{
    const auto CheckDocument = [&root](
        const std::string& name,
        const std::string& document)
    {
        const std::filesystem::path path = root / name / "request.json";
        WriteBytes(path, document);
        Check(ExpectFailure(path,
                worker::WorkerRequestParseErrorCode::ContractViolation),
            name + " is rejected as a contract violation");
    };

    CheckDocument("wrong_contract",
        "{\"contract\":\"other\",\"major\":1,\"minor\":0,"
        "\"jobId\":\"job\",\"correlationId\":\"c\","
        "\"capability\":\"geometry.repair\",\"timeoutMs\":1000,\"input\":{}}");
    CheckDocument("wrong_major",
        "{\"contract\":\"file_contract\",\"major\":2,\"minor\":0,"
        "\"jobId\":\"job\",\"correlationId\":\"c\","
        "\"capability\":\"geometry.repair\",\"timeoutMs\":1000,\"input\":{}}");
    CheckDocument("wrong_minor",
        "{\"contract\":\"file_contract\",\"major\":1,\"minor\":1,"
        "\"jobId\":\"job\",\"correlationId\":\"c\","
        "\"capability\":\"geometry.repair\",\"timeoutMs\":1000,\"input\":{}}");
    CheckDocument("bad_job", SliceRequest("../escape"));
    CheckDocument("empty_correlation",
        "{\"contract\":\"file_contract\",\"major\":1,\"minor\":0,"
        "\"jobId\":\"job\",\"correlationId\":\"\","
        "\"capability\":\"geometry.repair\",\"timeoutMs\":1000,\"input\":{}}");
    CheckDocument("unknown_capability",
        "{\"contract\":\"file_contract\",\"major\":1,\"minor\":0,"
        "\"jobId\":\"job\",\"correlationId\":\"c\","
        "\"capability\":\"geometry.future\",\"timeoutMs\":1000,\"input\":{}}");
    CheckDocument("timeout_low", SliceRequest("job", "999"));
    CheckDocument("timeout_fractional", SliceRequest("job", "1000.5"));
    CheckDocument("slice_missing_output",
        "{\"contract\":\"file_contract\",\"major\":1,\"minor\":0,"
        "\"jobId\":\"job\",\"correlationId\":\"c\","
        "\"capability\":\"slice.rgbwsv\",\"timeoutMs\":1000,"
        "\"sceneHash\":\"sha256:1234abcd\",\"scene\":{},\"profile\":{}}");
    CheckDocument("transfer_on_minor_zero",
        TransferSliceRequest("slice.rgbwsvt", "p0.rgbwsvt.1", "0"));
    CheckDocument("legacy_on_minor_one",
        TransferSliceRequest("slice.rgbwsv", "p0.rgbwsv.2", "1"));
    CheckDocument("transfer_with_legacy_contract",
        TransferSliceRequest("slice.rgbwsvt", "p0.rgbwsv.2", "1"));
    CheckDocument("legacy_with_transfer_contract",
        TransferSliceRequest("slice.rgbwsv", "p0.rgbwsvt.1", "0"));
    CheckDocument("geometry_missing_input",
        "{\"contract\":\"file_contract\",\"major\":1,\"minor\":0,"
        "\"jobId\":\"job\",\"correlationId\":\"c\","
        "\"capability\":\"geometry.repair\",\"timeoutMs\":1000}");
}

}  // namespace

int main()
{
    const std::filesystem::path root = MakeTestRoot();
    try
    {
        TestValidSliceRequest(root);
        TestValidTransferSliceRequest(root);
        TestValidGeometryRequests(root);
        TestPathAndEncodingFailures(root);
        TestContractFailures(root);
    }
    catch (const std::exception& error)
    {
        Check(false, std::string{"unexpected test exception: "} + error.what());
    }

    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    if (g_failures == 0)
    {
        std::cout << "Stage 14D-08-R1-01 worker request parser tests passed\n";
    }
    return g_failures == 0 ? 0 : 1;
}
