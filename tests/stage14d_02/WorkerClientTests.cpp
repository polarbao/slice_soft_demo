#include "slicer_module/WorkerClient.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{

using namespace std::chrono_literals;
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

std::string Utf8(const std::filesystem::path& path)
{
    const std::u8string value = path.u8string();
    return std::string{reinterpret_cast<const char*>(value.data()), value.size()};
}

void EmitTerminalProgress()
{
    std::cout << "SLICE_PROGRESS phase=completed current=1 total=1 percent=100 elapsedMs=1.000\n";
}

int SpawnSleepingChild(const std::filesystem::path& executable)
{
    std::wstring commandLine = L"\"" + executable.wstring()
        + L"\" --worker-helper sleep-child";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) == FALSE)
    {
        return 1;
    }
    std::cout << "HELPER_CHILD_PID=" << process.dwProcessId << std::endl;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    Sleep(60000);
    return 1;
}

int RunWorkerHelper(const int argc, char* const argv[])
{
    const std::string mode = argc >= 3 ? argv[2] : "";
    if (mode == "success")
    {
        std::cout << "ordinary stdout\n";
        std::cout << "SLICE_PROGRESS phase=load current=0 total=1 percent=0 elapsedMs=0.000\n";
        EmitTerminalProgress();
        std::cout << "SLICE_TIMING engine=test totalMs=1.000 workingSetBytes=10 peakWorkingSetBytes=20\n";
        std::cerr << "ordinary stderr\n";
        return 0;
    }
    if (mode == "plain-success")
    {
        std::cout << "plain command output\n";
        return 0;
    }
    if (mode == "echo")
    {
        std::cout << "ARG1=" << (argc >= 4 ? argv[3] : "") << '\n';
        std::cout << "ARG2=" << (argc >= 5 ? argv[4] : "") << '\n';
        EmitTerminalProgress();
        return 0;
    }
    if (mode == "exit")
    {
        return argc >= 4 ? std::stoi(argv[3]) : 1;
    }
    if (mode == "malformed")
    {
        std::cout << "SLICE_PROGRESS phase=load current=0 total=1 percent=0 elapsedMs=0.0\n";
        return 7;
    }
    if (mode == "backwards")
    {
        std::cout << "SLICE_PROGRESS phase=layers current=2 total=3 percent=50 elapsedMs=2.000\n";
        std::cout << "SLICE_PROGRESS phase=layers current=1 total=3 percent=49 elapsedMs=1.000\n";
        return 7;
    }
    if (mode == "bad-timing")
    {
        std::cout << "SLICE_TIMING engine=test totalMs=1 workingSetBytes=10 peakWorkingSetBytes=20\n";
        return 7;
    }
    if (mode == "cancel-aware")
    {
        const std::filesystem::path marker{argv[3]};
        for (int index = 0; index < 1000; ++index)
        {
            if (std::filesystem::exists(marker))
            {
                return 8;
            }
            Sleep(10);
        }
        return 1;
    }
    if (mode == "process-tree")
    {
        return SpawnSleepingChild(CurrentExecutable());
    }
    if (mode == "sleep-child")
    {
        Sleep(60000);
        return 0;
    }
    return 2;
}

slicer_module::WorkerLaunchOptions Options(const std::string& mode)
{
    slicer_module::WorkerLaunchOptions options;
    options.executablePath = CurrentExecutable();
    options.arguments = {"--worker-helper", mode};
    options.timeout = 5s;
    options.cancelGracePeriod = 500ms;
    return options;
}

void TestSuccessAndArbitraryArguments()
{
    slicer_module::WorkerClient client;
    std::atomic<int> callbacks{0};
    auto options = Options("success");
    options.progressSink = [&callbacks](const slicer_module::WorkerProgressEvent&)
    {
        callbacks.fetch_add(1);
    };
    const auto result = client.Run(options);
    if (result.exitCategory != slicer_module::WorkerExitCategory::Ok)
    {
        std::cerr << "success diagnostic: code=" << result.errorCode
                  << " message=" << result.errorMessage
                  << " processExit=" << result.processExitCode << '\n';
    }
    Check(result.started, "success command starts");
    Check(result.stopReason == slicer_module::WorkerStopReason::Exited, "success exits normally");
    Check(result.exitCategory == slicer_module::WorkerExitCategory::Ok, "exit zero maps to ok");
    Check(result.errorCode == "PM-SLICER-OK-0000", "success maps to stable code");
    Check(result.progressEvents.size() == 2 && callbacks == 2, "progress parses and streams");
    Check(result.timingEvents.size() == 1, "timing parses");
    Check(result.stdoutLogLines == std::vector<std::string>{"ordinary stdout"},
        "unknown stdout is retained");
    Check(result.stderrLogLines == std::vector<std::string>{"ordinary stderr"},
        "stderr is retained");

    options = Options("echo");
    options.arguments.push_back("value with spaces");
    options.arguments.push_back("UTF-8-\xE5\x8F\x82\xE6\x95\xB0");
    const auto echo = client.Run(options);
    if (echo.exitCategory != slicer_module::WorkerExitCategory::Ok)
    {
        std::cerr << "echo diagnostic: code=" << echo.errorCode
                  << " message=" << echo.errorMessage
                  << " processExit=" << echo.processExitCode << '\n';
    }
    Check(echo.exitCategory == slicer_module::WorkerExitCategory::Ok,
        "arbitrary UTF-8 arguments execute");
    Check(echo.stdoutLogLines.size() == 2
        && echo.stdoutLogLines[0] == "ARG1=value with spaces"
        && echo.stdoutLogLines[1] == "ARG2=UTF-8-\xE5\x8F\x82\xE6\x95\xB0",
        "Windows argument quoting preserves spaces and UTF-8");
}

void TestNoProgressModeAndExitMapping()
{
    slicer_module::WorkerClient client;
    DWORD handlesBefore{0};
    GetProcessHandleCount(GetCurrentProcess(), &handlesBefore);
    auto plain = Options("plain-success");
    plain.requireTerminalProgress = false;
    Check(client.Run(plain).exitCategory == slicer_module::WorkerExitCategory::Ok,
        "non-job commands may opt out of terminal progress");

    const std::vector<slicer_module::WorkerExitCategory> expected{
        slicer_module::WorkerExitCategory::Internal,
        slicer_module::WorkerExitCategory::Input,
        slicer_module::WorkerExitCategory::Profile,
        slicer_module::WorkerExitCategory::Topology,
        slicer_module::WorkerExitCategory::Resource,
        slicer_module::WorkerExitCategory::Output,
        slicer_module::WorkerExitCategory::Contract,
        slicer_module::WorkerExitCategory::Cancelled};
    for (int exitCode = 1; exitCode <= 8; ++exitCode)
    {
        auto options = Options("exit");
        options.arguments.push_back(std::to_string(exitCode));
        const auto result = client.Run(options);
        Check(result.exitCategory == expected[static_cast<std::size_t>(exitCode - 1)],
            "exit category mapping for " + std::to_string(exitCode));
    }
    auto unknown = Options("exit");
    unknown.arguments.push_back("42");
    Check(client.Run(unknown).exitCategory == slicer_module::WorkerExitCategory::Internal,
        "unknown exit maps to internal");
    DWORD handlesAfter{0};
    GetProcessHandleCount(GetCurrentProcess(), &handlesAfter);
    Check(handlesAfter <= handlesBefore + 2, "repeated worker runs do not leak handles");
}

void TestProtocolFailures()
{
    slicer_module::WorkerClient client;
    const auto missingTerminal = client.Run(Options("plain-success"));
    Check(missingTerminal.stopReason == slicer_module::WorkerStopReason::ContractViolation,
        "successful worker without terminal progress is rejected");

    const auto malformed = client.Run(Options("malformed"));
    Check(malformed.stopReason == slicer_module::WorkerStopReason::ContractViolation,
        "malformed reserved line is a contract error");
    Check(malformed.errorCode == "PM-SLICER-CONTRACT-0060", "contract error is stable");

    const auto backwards = client.Run(Options("backwards"));
    Check(backwards.stopReason == slicer_module::WorkerStopReason::ContractViolation,
        "backwards progress is rejected");

    const auto badTiming = client.Run(Options("bad-timing"));
    Check(badTiming.stopReason == slicer_module::WorkerStopReason::ContractViolation,
        "malformed reserved timing line is rejected");

    auto throwingCallback = Options("success");
    throwingCallback.progressSink = [](const slicer_module::WorkerProgressEvent&)
    {
        throw 1;
    };
    const auto callbackFailure = client.Run(throwingCallback);
    Check(callbackFailure.exitCategory == slicer_module::WorkerExitCategory::Internal,
        "progress callback exception is contained and reported");
}

void TestCooperativeCancellation()
{
    const std::filesystem::path marker = std::filesystem::temp_directory_path()
        / (L"slicesoft_worker_cancel_" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ignored;
    std::filesystem::remove(marker, ignored);
    slicer_module::WorkerClient client;
    auto options = Options("cancel-aware");
    options.arguments.push_back(Utf8(marker));
    options.cancellationMarkerPath = marker;
    slicer_module::WorkerRunResult result;
    std::thread runner([&]()
    {
        result = client.Run(options);
    });
    while (!client.IsRunning())
    {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(50ms);
    Check(client.RequestCancel(), "running command accepts cancellation");
    runner.join();
    Check(result.stopReason == slicer_module::WorkerStopReason::Cancelled,
        "cooperative cancellation is reported");
    Check(!result.forcedTermination, "cooperative cancellation avoids forced termination");
    std::filesystem::remove(marker, ignored);
}

DWORD ParseChildProcessId(const std::vector<std::string>& lines)
{
    constexpr std::string_view prefix{"HELPER_CHILD_PID="};
    for (const std::string& line : lines)
    {
        if (line.starts_with(prefix))
        {
            return static_cast<DWORD>(std::stoul(line.substr(prefix.size())));
        }
    }
    return 0;
}

bool IsProcessGone(const DWORD processId)
{
    if (processId == 0)
    {
        return false;
    }
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, processId);
    if (process == nullptr)
    {
        return GetLastError() == ERROR_INVALID_PARAMETER;
    }
    const bool gone = WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
    CloseHandle(process);
    return gone;
}

void TestTimeoutKillsProcessTree()
{
    slicer_module::WorkerClient client;
    auto options = Options("process-tree");
    options.timeout = 200ms;
    options.cancelGracePeriod = 100ms;
    const auto result = client.Run(options);
    Check(result.stopReason == slicer_module::WorkerStopReason::TimedOut, "timeout is distinguished");
    Check(result.forcedTermination, "uncooperative timeout forces the Job Object");
    Check(IsProcessGone(result.processId), "worker process is reaped");
    const DWORD childId = ParseChildProcessId(result.stdoutLogLines);
    Check(childId != 0 && IsProcessGone(childId), "worker descendants are reaped with the job");
}

void TestStartupFailure()
{
    slicer_module::WorkerClient client;
    auto options = Options("success");
    options.executablePath = CurrentExecutable().parent_path() / L"missing-worker.exe";
    const auto result = client.Run(options);
    Check(!result.started, "missing executable does not start");
    Check(result.stopReason == slicer_module::WorkerStopReason::StartupFailed,
        "startup failure is classified");
}

}  // namespace

int main(const int argc, char* const argv[])
{
    if (argc >= 2 && std::string_view{argv[1]} == "--worker-helper")
    {
        return RunWorkerHelper(argc, argv);
    }
    TestSuccessAndArbitraryArguments();
    TestNoProgressModeAndExitMapping();
    TestProtocolFailures();
    TestCooperativeCancellation();
    TestTimeoutKillsProcessTree();
    TestStartupFailure();
    if (g_failures == 0)
    {
        std::cout << "WorkerClientTests: PASS\n";
    }
    return g_failures == 0 ? 0 : 1;
}
