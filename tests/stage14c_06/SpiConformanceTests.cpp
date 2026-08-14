#include "SpiModuleApi.h"
#include "WorkerLifecycleConformance.h"

#include "slicer_core/json_value.h"

#include <Psapi.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using OutputCall = std::function<int(char*, int, int*)>;

void Require(const bool condition, const std::string_view message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string{message});
    }
}

void ReportPass(const std::string_view id, const std::string_view detail)
{
    std::cout << id << " PASS " << detail << '\n';
}

std::string ReadOutput(const OutputCall& call)
{
    int required{-1};
    Require(
        call(nullptr, 0, &required) == PM_ERR_BUFFER_SMALL,
        "buffer probe did not return PM_ERR_BUFFER_SMALL");
    Require(required > 0, "buffer probe returned no bytes");
    std::vector<char> output(static_cast<std::size_t>(required) + 1U, '\0');
    Require(
        call(output.data(), static_cast<int>(output.size()), &required)
            == required,
        "buffer write returned an unexpected byte count");
    Require(output.at(static_cast<std::size_t>(required)) == '\0',
            "buffer output has no trailing NUL");
    return {output.data(), static_cast<std::size_t>(required)};
}

void CheckThreeStateBuffer(const OutputCall& call)
{
    int required{-1};
    Require(
        call(nullptr, 0, &required) == PM_ERR_BUFFER_SMALL && required > 0,
        "C-SPI-05a probe contract failed");
    std::vector<char> shortBuffer(
        static_cast<std::size_t>(required),
        static_cast<char>(0x5a));
    const std::vector<char> sentinel = shortBuffer;
    Require(
        call(shortBuffer.data(), required, nullptr) == PM_ERR_BUFFER_SMALL,
        "C-SPI-05b short buffer contract failed");
    Require(shortBuffer == sentinel, "C-SPI-05b modified the short buffer");
    std::vector<char> fullBuffer(
        static_cast<std::size_t>(required) + 1U,
        static_cast<char>(0x5a));
    int written{-1};
    Require(
        call(fullBuffer.data(), required + 1, &written) == required,
        "C-SPI-05c full buffer contract failed");
    Require(written == required && fullBuffer.back() == '\0',
            "C-SPI-05c length or NUL contract failed");
}

slicer_core::Json ParseJson(const std::string& text)
{
    std::istringstream input{text};
    return slicer_core::Json::parse(input);
}

bool RuntimeMatches(
    const std::string_view moduleRuntime,
    const std::string_view moduleBuild,
    const std::string_view hostRuntime,
    const std::string_view hostBuild)
{
    return moduleRuntime == hostRuntime && moduleBuild == hostBuild;
}

std::uint64_t GetPrivateUsage()
{
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    Require(
        GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)) != FALSE,
        "could not query process private memory");
    return static_cast<std::uint64_t>(counters.PrivateUsage);
}

std::set<std::filesystem::path> ListRelativeFiles(
    const std::filesystem::path& root)
{
    std::set<std::filesystem::path> result;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        result.insert(std::filesystem::relative(entry.path(), root));
    }
    return result;
}

class CurrentDirectoryGuard final
{
public:
    /**
     * @brief 将进程切换到临时自检沙箱。
     * @param target 用作临时工作目录的现有目录。
     */
    explicit CurrentDirectoryGuard(const std::filesystem::path& target)
        : m_original{std::filesystem::current_path()}
    {
        std::filesystem::current_path(target);
    }

    /** @brief 恢复进程原工作目录。 */
    ~CurrentDirectoryGuard()
    {
        std::error_code error;
        std::filesystem::current_path(m_original, error);
    }

private:
    std::filesystem::path m_original;
};

void TestBinaryContract(const slicesoft::tests::SpiModuleApi& api)
{
    const std::vector<std::string> expected{
        "pm_cancel",
        "pm_create",
        "pm_destroy",
        "pm_last_error",
        "pm_module_info",
        "pm_poll",
        "pm_release",
        "pm_result",
        "pm_self_test",
        "pm_spi_version",
        "pm_submit"};
    Require(api.ExportNames() == expected, "C-SPI-16 export set drifted");
    ReportPass("C-SPI-16", "exactly 11 undecorated pm_* exports");

    for (std::string dependency : api.ImportedDllNames())
    {
        std::transform(
            dependency.begin(),
            dependency.end(),
            dependency.begin(),
            [](const unsigned char value)
            {
                return static_cast<char>(std::tolower(value));
            });
        Require(dependency != "printsdk.dll", "C-SPI-17 imported PrintSDK.dll");
        Require(!dependency.starts_with("qt5"), "C-SPI-17 imported a Qt5 DLL");
    }
    ReportPass("C-SPI-17", "no PrintSDK.dll or Qt5*.dll dependency");
}

slicer_core::Json TestModuleMetadata(const slicesoft::tests::SpiModuleApi& api)
{
    Require(api.SpiVersion() == PM_SPI_VERSION, "C-SPI-01 version mismatch");
    ReportPass("C-SPI-01", "SPI v1");

    const OutputCall infoCall = [&api](char* output, int capacity, int* required)
    {
        return api.ModuleInfo(output, capacity, required);
    };
    CheckThreeStateBuffer(infoCall);
    const slicer_core::Json info = ParseJson(ReadOutput(infoCall));
    for (const std::string field : {
             "id", "version", "spi", "runtime", "buildConfig", "provides", "produces"})
    {
        Require(info.contains(field), "C-SPI-02 module info omitted a required field");
    }
    Require(info.at("spi").as_int() == PM_SPI_VERSION,
            "C-SPI-02 module info SPI drifted");
    Require(info.at("provides").size() == 15U,
            "C-SPI-02 capability count drifted");
    ReportPass("C-SPI-02", "valid module info with 15 capabilities");

#if defined(_DEBUG)
    constexpr std::string_view expectedRuntime{"MSVC-x64-MDd"};
    constexpr std::string_view expectedBuild{"Debug"};
    constexpr std::string_view rejectedRuntime{"MSVC-x64-MD"};
#else
    constexpr std::string_view expectedRuntime{"MSVC-x64-MD"};
    constexpr std::string_view expectedBuild{"Release"};
    constexpr std::string_view rejectedRuntime{"MSVC-x64-MDd"};
#endif
    const std::string moduleRuntime = info.at("runtime").as_string();
    const std::string moduleBuild = info.at("buildConfig").as_string();
    Require(
        RuntimeMatches(
            moduleRuntime,
            moduleBuild,
            expectedRuntime,
            expectedBuild),
        "C-SPI-03 runtime identity mismatch");
    Require(
        !RuntimeMatches(
            rejectedRuntime,
            moduleBuild,
            expectedRuntime,
            expectedBuild),
        "C-SPI-03 mismatched runtime was not rejected");
    Require(
        !RuntimeMatches(
            moduleRuntime,
            expectedBuild == std::string_view{"Debug"} ? "Release" : "Debug",
            expectedRuntime,
            expectedBuild),
        "C-SPI-03 mismatched build configuration was not rejected");
    ReportPass("C-SPI-03", "host runtime and build configuration match");
    return info;
}

void TestModuleLifecycle(const slicesoft::tests::SpiModuleApi& api)
{
    for (int iteration = 0; iteration < 10; ++iteration)
    {
        pm_module_t* const module = api.Create(nullptr);
        Require(module != nullptr, "module lifecycle warmup failed");
        api.Destroy(module);
    }
    const std::uint64_t before = GetPrivateUsage();
    for (int iteration = 0; iteration < 100; ++iteration)
    {
        pm_module_t* const module = api.Create(nullptr);
        Require(module != nullptr, "C-SPI-04 create failed");
        api.Destroy(module);
    }
    const std::uint64_t after = GetPrivateUsage();
    const std::uint64_t growth = after > before ? after - before : 0U;
    Require(growth < 1024U * 1024U, "C-SPI-04 memory growth reached 1 MiB");
    ReportPass("C-SPI-04", "100 create/destroy pairs below 1 MiB growth");
}

void TestSelfTest(
    const slicesoft::tests::SpiModuleApi& api,
    pm_module_t* const module)
{
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path sandbox = std::filesystem::temp_directory_path()
        / ("slicesoft_stage14c06_" + std::to_string(GetCurrentProcessId())
           + "_" + std::to_string(nonce));
    std::filesystem::create_directories(sandbox);
    std::string reportText;
    {
        CurrentDirectoryGuard guard{sandbox};
        const auto before = ListRelativeFiles(sandbox);
        const OutputCall selfTestCall = [&api, module](
                                                char* output,
                                                int capacity,
                                                int* required)
        {
            return api.SelfTest(module, output, capacity, required);
        };
        CheckThreeStateBuffer(selfTestCall);
        reportText = ReadOutput(selfTestCall);
        Require(ListRelativeFiles(sandbox) == before,
                "C-SPI-18 self-test created persistent files");
    }
    std::filesystem::remove_all(sandbox);

    const slicer_core::Json report = ParseJson(reportText);
    Require(report.at("schema").as_string() == "slicesoft.module_self_test.1",
            "C-SPI-18 self-test schema mismatch");
    Require(report.at("ok").as_bool(), "C-SPI-18 self-test reported failure");
    Require(report.at("sideEffects").at("workerStarted").as_bool() == false,
            "C-SPI-18 self-test started a Worker");
    Require(report.at("sideEffects").at("persistentWrites").as_int() == 0,
            "C-SPI-18 self-test reported persistent writes");
    Require(reportText.find("BLOCKED_BY_WORKER_GATE") != std::string::npos,
            "self-test did not disclose deferred Worker checks");
    ReportPass("C-SPI-18", "structured side-effect-free module-local self-test");
}

std::string ReadErrorCode(const slicesoft::tests::SpiModuleApi& api)
{
    const std::string errorText = ReadOutput(
        [&api](char* output, int capacity, int* required)
        {
            return api.LastError(output, capacity, required);
        });
    return ParseJson(errorText).at("code").as_string();
}

void TestInvalidRequests(
    const slicesoft::tests::SpiModuleApi& api,
    pm_module_t* const module)
{
    const std::array<std::string_view, 12> invalidRequests{
        "", "{", "[]", "null", "true", "{}",
        R"({"capability":null})",
        R"({"capability":1})",
        R"({"capability":""})",
        R"({"capability":"unknown.capability"})",
        R"({"capability":"geometry.preflight"})",
        R"({"capability":"geometry.preflight","mode":"slow"})"};
    const std::regex errorPattern{R"(^PM-[A-Z]+-[A-Z0-9]+-[0-9]{4}$)"};
    bool checkedErrorBuffer{false};
    for (const std::string_view request : invalidRequests)
    {
        Require(api.Submit(module, request.data()) == nullptr,
                "C-SPI-11 invalid request was accepted");
        const std::string firstCode = ReadErrorCode(api);
        Require(std::regex_match(firstCode, errorPattern),
                "C-SPI-10 error code format drifted");
        if (!checkedErrorBuffer)
        {
            CheckThreeStateBuffer(
                [&api](char* output, int capacity, int* required)
                {
                    return api.LastError(output, capacity, required);
                });
            checkedErrorBuffer = true;
        }
        Require(api.Submit(module, request.data()) == nullptr,
                "C-SPI-11 repeated invalid request was accepted");
        Require(ReadErrorCode(api) == firstCode,
                "C-SPI-11 invalid request error code was unstable");
    }
    ReportPass("C-SPI-10", "stable PM-SLICER category code format");
    ReportPass("C-SPI-11", "12 invalid request variants fail closed");
}

pm_job_t* SubmitImportJob(
    const slicesoft::tests::SpiModuleApi& api,
    pm_module_t* const module,
    const std::filesystem::path& repository)
{
    const std::string path = (repository
        / "tests/fixtures/stage14b/model_with_normals.obj").generic_string();
    const std::string request =
        R"({"capability":"model.import","modelPath":")"
        + path
        + R"(","options":{"computeBBox":true,"extractMaterials":false}})";
    return api.Submit(module, request.c_str());
}

void TestTerminalJob(
    const slicesoft::tests::SpiModuleApi& api,
    pm_module_t* const module,
    const std::filesystem::path& repository)
{
    pm_job_t* const job = SubmitImportJob(api, module, repository);
    Require(job != nullptr, "C-SPI-06 synchronous import was not accepted");
    const OutputCall pollCall = [&api, job](char* output, int capacity, int* required)
    {
        return api.Poll(job, output, capacity, required);
    };
    CheckThreeStateBuffer(pollCall);
    const slicer_core::Json firstProgress = ParseJson(ReadOutput(pollCall));
    const slicer_core::Json secondProgress = ParseJson(ReadOutput(pollCall));
    Require(firstProgress.at("state").as_string() == "succeeded",
            "C-SPI-06 first poll was not terminal success");
    Require(firstProgress.at("percent").as_int()
                <= secondProgress.at("percent").as_int(),
            "C-SPI-07 progress regressed");
    const OutputCall resultCall = [&api, job](char* output, int capacity, int* required)
    {
        return api.Result(job, output, capacity, required);
    };
    CheckThreeStateBuffer(resultCall);
    Require(ParseJson(ReadOutput(resultCall)).at("ok").as_bool(),
            "C-SPI-06 result was not successful");
    Require(api.Cancel(job) == PM_OK && api.Cancel(job) == PM_OK,
            "C-SPI-15 terminal cancel was not idempotent");
    api.Release(job);
    ReportPass("C-SPI-05a/b/c", "all exercised string outputs use one three-state contract");
    ReportPass("C-SPI-06", "submit, terminal poll and result closure");
    ReportPass("C-SPI-07", "recorded progress is monotonic");
    ReportPass("C-SPI-15", "repeated terminal cancellation is idempotent");
}

void TestNullHandles(const slicesoft::tests::SpiModuleApi& api)
{
    Require(api.Submit(nullptr, "{}") == nullptr,
            "C-SPI-12 submit accepted a null module");
    Require(api.Poll(nullptr, nullptr, 0, nullptr) == PM_ERR_INVALID_ARG,
            "C-SPI-12 poll accepted a null job");
    Require(api.Cancel(nullptr) == PM_ERR_INVALID_ARG,
            "C-SPI-12 cancel accepted a null job");
    Require(api.Result(nullptr, nullptr, 0, nullptr) == PM_ERR_INVALID_ARG,
            "C-SPI-12 result accepted a null job");
    Require(api.SelfTest(nullptr, nullptr, 0, nullptr) == PM_ERR_INVALID_ARG,
            "C-SPI-12 self-test accepted a null module");
    api.Destroy(nullptr);
    api.Release(nullptr);
    ReportPass("C-SPI-12", "null handles fail closed or follow documented no-op rules");
}

void TestDestroyWithJob(
    const slicesoft::tests::SpiModuleApi& api,
    const std::filesystem::path& repository)
{
    pm_module_t* const module = api.Create(nullptr);
    Require(module != nullptr, "C-SPI-14 module setup failed");
    pm_job_t* const job = SubmitImportJob(api, module, repository);
    Require(job != nullptr, "C-SPI-14 job setup failed");
    api.Destroy(module);
    Require(api.Poll(job, nullptr, 0, nullptr) == PM_ERR_INVALID_ARG,
            "C-SPI-14 destroy left its job handle live");
    ReportPass("C-SPI-14", "destroy retires unreleased jobs without a crash");
}

}  // namespace

int main(const int argumentCount, char* arguments[])
{
    try
    {
        Require(argumentCount == 3, "usage: test_spi_conformance <dll> <repo>");
        const std::filesystem::path libraryPath =
            std::filesystem::absolute(arguments[1]);
        const std::filesystem::path repository =
            std::filesystem::absolute(arguments[2]);
        slicesoft::tests::SpiModuleApi api{libraryPath};

        TestBinaryContract(api);
        (void)TestModuleMetadata(api);
        TestModuleLifecycle(api);
        pm_module_t* const module = api.Create(nullptr);
        Require(module != nullptr, "module setup failed");
        TestSelfTest(api, module);
        TestInvalidRequests(api, module);
        TestTerminalJob(api, module, repository);
        slicesoft::tests::TestWorkerLifecycleConformance(
            api,
            module,
            repository);
        api.Destroy(module);
        TestNullHandles(api);
        TestDestroyWithJob(api, repository);

        std::cout << "14C-06 COMPLETE; C-SPI-01..18 PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Stage 14C-06 SPI conformance: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
