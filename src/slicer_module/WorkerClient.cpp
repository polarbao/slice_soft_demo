#include "WorkerClient.h"
#include "WorkerProcessWindows.h"
#include "WorkerProtocol.h"

#include "slicer_core/api/artifacts/PackageArtifactSafety.h"
#include "slicer_core/rip_reader.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <string_view>

// 文件职责：独占管理一个 Windows Worker 进程树，并解析文件合同文本传输；
// 边界：超时或取消后必须终止整个作业对象，并保留明确的停止原因和清理证据。
namespace slicesoft::module
{
namespace
{

constexpr DWORD PollIntervalMs{10};
constexpr DWORD CancelledProcessExitCode{8};
constexpr std::size_t PipeDrainBudgetBytes{256U * 1024U};

using worker_detail::BuildCommandLine;
using worker_detail::CreatePipePair;
using worker_detail::ProcessAttributeList;
using worker_detail::UniqueHandle;
using worker_detail::WindowsError;
using worker_detail::WriteCancellationMarker;

void ReadAvailablePipe(
    const HANDLE pipe,
    const bool parseProtocol,
    bool* pipeOpen,
    worker_detail::WorkerProtocolParser* parser)
{
    char bytes[4096];
    std::size_t drained{0};
    while (*pipeOpen && drained < PipeDrainBudgetBytes)
    {
        DWORD available{0};
        if (PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) == FALSE)
        {
            *pipeOpen = false;
            break;
        }
        if (available == 0)
        {
            break;
        }
        DWORD bytesRead{0};
        const DWORD requested = (std::min)(available, static_cast<DWORD>(sizeof(bytes)));
        if (ReadFile(pipe, bytes, requested, &bytesRead, nullptr) == FALSE || bytesRead == 0)
        {
            *pipeOpen = false;
            break;
        }
        drained += bytesRead;
        if (parseProtocol)
        {
            parser->ProcessStdoutChunk(std::string_view{bytes, bytesRead});
        }
        else
        {
            parser->ProcessStderrChunk(std::string_view{bytes, bytesRead});
        }
    }
}

WorkerExitCategory MapExitCategory(const DWORD exitCode)
{
    switch (exitCode)
    {
    case 0: return WorkerExitCategory::Ok;
    case 2: return WorkerExitCategory::Input;
    case 3: return WorkerExitCategory::Profile;
    case 4: return WorkerExitCategory::Topology;
    case 5: return WorkerExitCategory::Resource;
    case 6: return WorkerExitCategory::Output;
    case 7: return WorkerExitCategory::Contract;
    case 8: return WorkerExitCategory::Cancelled;
    default: return WorkerExitCategory::Internal;
    }
}

std::string StableExitCode(const WorkerExitCategory category)
{
    switch (category)
    {
    case WorkerExitCategory::Ok: return "PM-SLICER-OK-0000";
    case WorkerExitCategory::Input: return "PM-SLICER-INPUT-0002";
    case WorkerExitCategory::Profile: return "PM-SLICER-PROFILE-0030";
    case WorkerExitCategory::Topology: return "PM-SLICER-TOPOLOGY-0010";
    case WorkerExitCategory::Resource: return "PM-SLICER-RESOURCE-0040";
    case WorkerExitCategory::Output: return "PM-SLICER-OUTPUT-0050";
    case WorkerExitCategory::Contract: return "PM-SLICER-CONTRACT-0060";
    case WorkerExitCategory::Cancelled: return "PM-SLICER-CANCELLED-0070";
    default: return "PM-SLICER-INTERNAL-0099";
    }
}

bool IsValidPublishedPackage(const std::filesystem::path& packageDirectory)
{
    try
    {
        (void)slicer_core::internal::ValidateSlicePackageArtifact(
            packageDirectory);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void ApplyModuleArtifactRecovery(
    const WorkerLaunchOptions& options,
    WorkerRunResult& result)
{
    if (!options.packageArtifacts.has_value())
    {
        return;
    }

    result.artifactCleanupAttempted = true;
    try
    {
        const WorkerPackageArtifactContext& context =
            *options.packageArtifacts;
        const auto identity =
            slicer_core::api::artifacts::MakePackageArtifactIdentity(
                context.packageDirectory,
                context.jobId,
                context.attemptId);
        const auto recovery =
            slicer_core::api::artifacts::RecoverPackageArtifacts(
                identity,
                IsValidPublishedPackage);
        result.artifactCleanupSucceeded = recovery.success;
        result.artifactTargetRestored = recovery.target_restored;
        result.residualArtifactPaths = recovery.residual_paths;
        if (recovery.success)
        {
            return;
        }
        result.exitCategory = WorkerExitCategory::Output;
        result.stopReason = WorkerStopReason::ArtifactCleanupFailed;
        result.errorCode = "PM-SLICER-OUTPUT-0050";
        result.errorMessage = "module package artifact cleanup failed";
        if (!recovery.error.empty())
        {
            result.errorMessage += ": " + recovery.error;
        }
    }
    catch (const std::exception& error)
    {
        result.artifactCleanupSucceeded = false;
        result.exitCategory = WorkerExitCategory::Output;
        result.stopReason = WorkerStopReason::ArtifactCleanupFailed;
        result.errorCode = "PM-SLICER-OUTPUT-0050";
        result.errorMessage =
            std::string{"module package artifact cleanup failed: "}
            + error.what();
    }
}

}  // namespace

class WorkerClient::Impl final
{
public:
    WorkerRunResult Run(const WorkerLaunchOptions& options);
    bool RequestCancel() noexcept;
    [[nodiscard]] bool IsRunning() const noexcept
    {
        return m_running.load(std::memory_order_acquire);
    }
    void WaitUntilIdle();

private:
    WorkerRunResult RunProcess(const WorkerLaunchOptions& options);
    void FinishRun();

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cancelRequested{false};
    std::mutex m_idleMutex;
    std::condition_variable m_idleCondition;
};

WorkerRunResult WorkerClient::Impl::Run(const WorkerLaunchOptions& options)
{
    WorkerRunResult result;
    bool expected{false};
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        result.errorCode = "PM-SLICER-INTERNAL-0099";
        result.errorMessage = "WorkerClient already has an active Run";
        return result;
    }
    struct FinishGuard final
    {
        Impl* owner;
        ~FinishGuard()
        {
            owner->FinishRun();
        }
    } finishGuard{this};
    m_cancelRequested.store(false, std::memory_order_release);

    result = RunProcess(options);
    ApplyModuleArtifactRecovery(options, result);
    return result;
}

WorkerRunResult WorkerClient::Impl::RunProcess(
    const WorkerLaunchOptions& options)
{
    WorkerRunResult result;

    if (options.executablePath.empty() || options.timeout.count() <= 0
        || options.cancelGracePeriod.count() < 0 || options.cancelGracePeriod.count() > 2000)
    {
        result.errorCode = "PM-SLICER-PROFILE-0031";
        result.errorMessage = "invalid executable, timeout, or cancel grace period";
        return result;
    }

    UniqueHandle stdoutRead;
    UniqueHandle stdoutWrite;
    UniqueHandle stderrRead;
    UniqueHandle stderrWrite;
    if (!CreatePipePair(&stdoutRead, &stdoutWrite) || !CreatePipePair(&stderrRead, &stderrWrite))
    {
        result.errorCode = "PM-SLICER-INTERNAL-0099";
        result.errorMessage = WindowsError("CreatePipe");
        return result;
    }

    UniqueHandle job{CreateJobObjectW(nullptr, nullptr)};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job.IsValid() || SetInformationJobObject(job.Get(), JobObjectExtendedLimitInformation,
            &limits, sizeof(limits)) == FALSE)
    {
        result.errorCode = "PM-SLICER-INTERNAL-0099";
        result.errorMessage = WindowsError("configure Job Object");
        return result;
    }

    std::wstring commandLine;
    if (!BuildCommandLine(options, &commandLine))
    {
        result.errorCode = "PM-SLICER-INPUT-0002";
        result.errorMessage = "one worker argument is not valid UTF-8";
        return result;
    }
    const std::array<HANDLE, 2> inheritedHandles{stdoutWrite.Get(), stderrWrite.Get()};
    ProcessAttributeList attributes;
    if (!attributes.Initialize(inheritedHandles))
    {
        result.errorCode = "PM-SLICER-INTERNAL-0099";
        result.errorMessage = WindowsError("initialize process handle list");
        return result;
    }
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = nullptr;
    startup.StartupInfo.hStdOutput = stdoutWrite.Get();
    startup.StartupInfo.hStdError = stderrWrite.Get();
    startup.lpAttributeList = attributes.Get();
    PROCESS_INFORMATION processInformation{};
    const wchar_t* workingDirectory = options.workingDirectory.empty()
        ? nullptr : options.workingDirectory.c_str();
    if (CreateProcessW(options.executablePath.c_str(), commandLine.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT
                | EXTENDED_STARTUPINFO_PRESENT,
            nullptr, workingDirectory, &startup.StartupInfo, &processInformation) == FALSE)
    {
        result.errorCode = "PM-SLICER-INTERNAL-0099";
        result.errorMessage = WindowsError("CreateProcessW");
        return result;
    }
    UniqueHandle process{processInformation.hProcess};
    UniqueHandle processThread{processInformation.hThread};
    result.started = true;
    result.processId = processInformation.dwProcessId;
    stdoutWrite.Reset();
    stderrWrite.Reset();

    if (AssignProcessToJobObject(job.Get(), process.Get()) == FALSE)
    {
        const DWORD error = GetLastError();
        TerminateProcess(process.Get(), 1);
        WaitForSingleObject(process.Get(), INFINITE);
        result.errorCode = "PM-SLICER-INTERNAL-0099";
        result.errorMessage = WindowsError("AssignProcessToJobObject", error);
        return result;
    }
    if (ResumeThread(processThread.Get()) == std::numeric_limits<DWORD>::max())
    {
        const DWORD error = GetLastError();
        TerminateJobObject(job.Get(), 1);
        WaitForSingleObject(process.Get(), INFINITE);
        result.errorCode = "PM-SLICER-INTERNAL-0099";
        result.errorMessage = WindowsError("ResumeThread", error);
        return result;
    }
    processThread.Reset();

    worker_detail::WorkerProtocolParser protocol{&result, options.progressSink};
    bool stdoutOpen{true};
    bool stderrOpen{true};
    const auto startedAt = std::chrono::steady_clock::now();
    std::optional<std::chrono::steady_clock::time_point> cancellationStarted;
    bool timedOut{false};
    bool markerWritten{true};
    DWORD waitError{ERROR_SUCCESS};
    DWORD waitResult{WAIT_TIMEOUT};
    while ((waitResult = WaitForSingleObject(process.Get(), PollIntervalMs)) == WAIT_TIMEOUT)
    {
        ReadAvailablePipe(stdoutRead.Get(), true, &stdoutOpen, &protocol);
        ReadAvailablePipe(stderrRead.Get(), false, &stderrOpen, &protocol);
        const auto now = std::chrono::steady_clock::now();
        timedOut = timedOut || now - startedAt >= options.timeout;
        const bool mustCancel = timedOut || m_cancelRequested.load(std::memory_order_acquire)
            || protocol.HasContractError() || protocol.HasCallbackError();
        if (mustCancel && !cancellationStarted.has_value())
        {
            cancellationStarted = now;
            markerWritten = WriteCancellationMarker(options.cancellationMarkerPath);
        }
        if (cancellationStarted.has_value() && now - *cancellationStarted >= options.cancelGracePeriod)
        {
            result.forcedTermination = true;
            TerminateJobObject(job.Get(), CancelledProcessExitCode);
        }
    }
    if (waitResult == WAIT_FAILED)
    {
        waitError = GetLastError();
        result.forcedTermination = true;
        TerminateJobObject(job.Get(), 1);
        WaitForSingleObject(process.Get(), INFINITE);
    }
    DWORD processExitCode{0};
    GetExitCodeProcess(process.Get(), &processExitCode);
    result.processExitCode = processExitCode;
    job.Reset();
    for (int attempt = 0; attempt < 10 && (stdoutOpen || stderrOpen); ++attempt)
    {
        ReadAvailablePipe(stdoutRead.Get(), true, &stdoutOpen, &protocol);
        ReadAvailablePipe(stderrRead.Get(), false, &stderrOpen, &protocol);
        Sleep(1);
    }
    protocol.Finish();

    result.exitCategory = MapExitCategory(result.processExitCode);
    result.stopReason = WorkerStopReason::Exited;
    result.errorCode = StableExitCode(result.exitCategory);
    if (waitResult == WAIT_FAILED || protocol.HasCallbackError())
    {
        result.exitCategory = WorkerExitCategory::Internal;
        result.stopReason = WorkerStopReason::StartupFailed;
        result.errorCode = "PM-SLICER-INTERNAL-0099";
        result.errorMessage = waitResult == WAIT_FAILED
            ? WindowsError("WaitForSingleObject", waitError) : protocol.ErrorMessage();
    }
    else if (protocol.HasContractError()
        || (result.processExitCode == 0
            && options.requireTerminalProgress
            && !protocol.HasTerminalProgress()))
    {
        result.exitCategory = WorkerExitCategory::Contract;
        result.stopReason = WorkerStopReason::ContractViolation;
        result.errorCode = "PM-SLICER-CONTRACT-0060";
        result.errorMessage = protocol.HasContractError()
            ? protocol.ErrorMessage() : "successful worker exit did not emit terminal percent=100";
    }
    else if (timedOut)
    {
        result.exitCategory = WorkerExitCategory::Cancelled;
        result.stopReason = WorkerStopReason::TimedOut;
        result.errorCode = "PM-SLICER-CANCELLED-0070";
        result.errorMessage = "worker exceeded its finite timeout";
    }
    else if (m_cancelRequested.load(std::memory_order_acquire))
    {
        result.exitCategory = WorkerExitCategory::Cancelled;
        result.stopReason = WorkerStopReason::Cancelled;
        result.errorCode = "PM-SLICER-CANCELLED-0070";
        result.errorMessage = "worker cancellation was requested";
    }
    if (!markerWritten && !result.errorMessage.empty())
    {
        result.errorMessage += "; cancellation marker could not be created atomically";
    }
    return result;
}

bool WorkerClient::Impl::RequestCancel() noexcept
{
    if (!m_running.load(std::memory_order_acquire))
    {
        return false;
    }
    m_cancelRequested.store(true, std::memory_order_release);
    return true;
}

void WorkerClient::Impl::FinishRun()
{
    m_running.store(false, std::memory_order_release);
    m_idleCondition.notify_all();
}

void WorkerClient::Impl::WaitUntilIdle()
{
    std::unique_lock lock{m_idleMutex};
    m_idleCondition.wait(lock, [this]()
    {
        return !IsRunning();
    });
}

WorkerClient::WorkerClient() : m_impl(std::make_unique<Impl>())
{
}

WorkerClient::~WorkerClient()
{
    m_impl->RequestCancel();
    m_impl->WaitUntilIdle();
}

WorkerRunResult WorkerClient::Run(const WorkerLaunchOptions& options)
{
    return m_impl->Run(options);
}

bool WorkerClient::RequestCancel() noexcept
{
    return m_impl->RequestCancel();
}

bool WorkerClient::IsRunning() const noexcept
{
    return m_impl->IsRunning();
}

}  // namespace slicesoft::module
