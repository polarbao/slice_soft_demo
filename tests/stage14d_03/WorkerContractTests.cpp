#include "slicer_module/WorkerContract.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace
{

namespace slicer_module = slicesoft::module;

int g_failures{0};

void Check(const bool condition, const std::string& message)
{
    if (!condition)
    {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::filesystem::path CurrentExecutable()
{
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return path;
}

int EmitContractFixture(const std::string& fixture)
{
    if (fixture.find("malformed") != std::string::npos)
    {
        std::cout << "{not-json}\n";
        return 0;
    }
    if (fixture.find("extra_stdout") != std::string::npos)
    {
        std::cout << "ordinary stdout must not share the discovery channel\n";
    }
    if (fixture.find("stderr_log") != std::string::npos)
    {
        std::cerr << "ordinary stderr diagnostic\n";
    }

    const int major = fixture.find("major2") != std::string::npos ? 2 : 1;
    const int minor = fixture.find("minor1") != std::string::npos ? 1 : 0;
    const bool missingPackage = fixture.find("missing_package") != std::string::npos;
    const bool duplicatePackage = fixture.find("duplicate_package") != std::string::npos;
    const bool missingCapability = fixture.find("missing_capability") != std::string::npos;
    const bool unknownCapability = fixture.find("unknown_capability") != std::string::npos;
    const bool duplicateCapability = fixture.find("duplicate_capability") != std::string::npos;

    std::cout
        << "{\"contract\":\"file_contract\","
        << "\"major\":" << major << ",\"minor\":" << minor << ','
        << "\"engineVersion\":\"fixture\","
        << "\"produces\":[\""
        << (missingPackage ? "p0.rgbwsv.1" : "p0.rgbwsv.2") << '"';
    if (duplicatePackage)
    {
        std::cout << ",\"p0.rgbwsv.2\"";
    }
    std::cout << "],\"capabilities\":[";
    if (!missingCapability)
    {
        std::cout << "\"slice.rgbwsv\"";
        if (duplicateCapability)
        {
            std::cout << ",\"slice.rgbwsv\"";
        }
        if (unknownCapability)
        {
            std::cout << ",\"future.unknown\"";
        }
    }
    else
    {
        std::cout << "\"geometry.repair\"";
    }
    std::cout << "],\"futureOptionalField\":true}\n";
    return 0;
}

std::filesystem::path CreateFixtureExecutable(
    const std::filesystem::path& directory,
    const std::string& fixture)
{
    const std::filesystem::path destination = directory / (fixture + ".exe");
    std::filesystem::copy_file(
        CurrentExecutable(), destination, std::filesystem::copy_options::overwrite_existing);
    return destination;
}

slicer_module::WorkerContractResult Negotiate(
    const std::filesystem::path& executable,
    const slicer_module::WorkerContractRequirement& requirement)
{
    slicer_module::WorkerClient client;
    const slicer_module::WorkerContractNegotiator negotiator{client};
    return negotiator.Negotiate(executable, requirement);
}

slicer_module::WorkerContractRequirement SliceRequirement()
{
    slicer_module::WorkerContractRequirement requirement;
    requirement.requiredCapabilities = {"slice.rgbwsv"};
    return requirement;
}

void TestRealWorker(const std::filesystem::path& workerExecutable)
{
    const auto result = Negotiate(workerExecutable, SliceRequirement());
    Check(result.compatible, "real worker contract-info is compatible");
    Check(result.decision == slicer_module::WorkerContractDecision::Compatible,
        "real worker returns compatible decision");
    Check(result.info.major == 1 && result.info.minor == 0,
        "real worker reports file_contract_v1 version 1.0");
    Check(result.info.engineVersion == "0.1.0", "real worker reports engine version");
    Check(result.transport.stdoutLogLines.size() == 1,
        "real worker emits exactly one stdout JSON object");
    Check(result.transport.stderrLogLines.empty(),
        "real worker successful discovery does not emit stderr diagnostics");
}

void TestVersionCompatibility(const std::filesystem::path& fixtures)
{
    const auto newer = Negotiate(
        CreateFixtureExecutable(fixtures, "worker_minor1"), SliceRequirement());
    Check(newer.compatible && newer.info.minor == 1,
        "newer worker minor is backward compatible");

    const auto majorMismatch = Negotiate(
        CreateFixtureExecutable(fixtures, "worker_major2"), SliceRequirement());
    Check(!majorMismatch.compatible
        && majorMismatch.decision == slicer_module::WorkerContractDecision::MajorMismatch,
        "tampered major is rejected fail-closed");
    Check(majorMismatch.errorCode == "PM-SLICER-INTERNAL-0099",
        "major mismatch uses the frozen stable code");

    auto newerRequirement = SliceRequirement();
    newerRequirement.minor = 1;
    const auto older = Negotiate(
        CreateFixtureExecutable(fixtures, "worker_minor0"), newerRequirement);
    Check(!older.compatible
        && older.decision == slicer_module::WorkerContractDecision::MinorTooOld,
        "worker minor below the required minor is rejected");
}

void TestRequiredDeclarations(const std::filesystem::path& fixtures)
{
    const auto missingPackage = Negotiate(
        CreateFixtureExecutable(fixtures, "worker_missing_package"), SliceRequirement());
    Check(!missingPackage.compatible
        && missingPackage.decision
            == slicer_module::WorkerContractDecision::MissingProductionContract,
        "missing p0.rgbwsv.2 is rejected");
    Check(missingPackage.errorCode == "PM-SLICER-CONTRACT-0060",
        "missing production contract uses stable contract error");

    const auto missingCapability = Negotiate(
        CreateFixtureExecutable(fixtures, "worker_missing_capability"), SliceRequirement());
    Check(!missingCapability.compatible
        && missingCapability.decision == slicer_module::WorkerContractDecision::MissingCapability,
        "missing requested capability is rejected before a job launch");

    const auto duplicatePackage = Negotiate(
        CreateFixtureExecutable(fixtures, "worker_duplicate_package"), SliceRequirement());
    Check(duplicatePackage.compatible,
        "duplicate production-contract entries allowed by the schema remain compatible");
}

void TestDocumentAndLogBoundary(const std::filesystem::path& fixtures)
{
    const auto malformed = Negotiate(
        CreateFixtureExecutable(fixtures, "worker_malformed"), SliceRequirement());
    Check(!malformed.compatible
        && malformed.decision == slicer_module::WorkerContractDecision::InvalidDocument,
        "malformed contract JSON is rejected");

    const auto extraStdout = Negotiate(
        CreateFixtureExecutable(fixtures, "worker_extra_stdout"), SliceRequirement());
    Check(!extraStdout.compatible
        && extraStdout.decision == slicer_module::WorkerContractDecision::InvalidDocument,
        "ordinary stdout cannot be confused with the contract JSON");

    const auto stderrLog = Negotiate(
        CreateFixtureExecutable(fixtures, "worker_stderr_log"), SliceRequirement());
    Check(stderrLog.compatible, "ordinary stderr remains a non-contract diagnostic channel");
    Check(stderrLog.transport.stderrLogLines
            == std::vector<std::string>{"ordinary stderr diagnostic"},
        "stderr diagnostics are retained by WorkerClient");

    const auto unknownCapability = Negotiate(
        CreateFixtureExecutable(fixtures, "worker_unknown_capability"), SliceRequirement());
    Check(!unknownCapability.compatible
        && unknownCapability.decision == slicer_module::WorkerContractDecision::InvalidDocument,
        "capability outside the frozen schema enum is rejected");

    const auto duplicateCapability = Negotiate(
        CreateFixtureExecutable(fixtures, "worker_duplicate_capability"), SliceRequirement());
    Check(!duplicateCapability.compatible
        && duplicateCapability.decision == slicer_module::WorkerContractDecision::InvalidDocument,
        "duplicate capabilities violate the frozen uniqueItems rule");
}

}  // namespace

int main(const int argc, char* const argv[])
{
    if (argc == 2 && std::string_view{argv[1]} == "--contract-info")
    {
        return EmitContractFixture(CurrentExecutable().stem().string());
    }
    if (argc != 2)
    {
        std::cerr << "usage: WorkerContractTests <absolute-slicer-worker-path>\n";
        return 2;
    }

    const std::filesystem::path workerExecutable = std::filesystem::absolute(argv[1]);
    const std::filesystem::path fixtures = std::filesystem::temp_directory_path()
        / ("slicesoft_stage14d03_" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(fixtures);
    try
    {
        TestRealWorker(workerExecutable);
        TestVersionCompatibility(fixtures);
        TestRequiredDeclarations(fixtures);
        TestDocumentAndLogBoundary(fixtures);
    }
    catch (const std::exception& error)
    {
        Check(false, std::string{"unexpected test exception: "} + error.what());
    }
    std::error_code ignored;
    std::filesystem::remove_all(fixtures, ignored);
    if (g_failures == 0)
    {
        std::cout << "WorkerContractTests: PASS\n";
    }
    return g_failures == 0 ? 0 : 1;
}
