#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace slicesoft::module
{

/** @brief Stable process-exit categories frozen by file_contract_v1. */
enum class WorkerExitCategory
{
    Ok,
    Internal,
    Input,
    Profile,
    Topology,
    Resource,
    Output,
    Contract,
    Cancelled
};

/** @brief Describes how ownership of the worker process ended. */
enum class WorkerStopReason
{
    Exited,
    StartupFailed,
    ContractViolation,
    Cancelled,
    TimedOut
};

/** @brief Strictly parsed SLICE_PROGRESS payload. */
struct WorkerProgressEvent
{
    std::string phase;
    std::uint64_t current{0};
    std::uint64_t total{0};
    std::uint32_t percent{0};
    double elapsedMs{0.0};
};

/** @brief Required fields from a strictly parsed SLICE_TIMING payload. */
struct WorkerTimingEvent
{
    std::string engine;
    double totalMs{0.0};
    std::uint64_t workingSetBytes{0};
    std::uint64_t peakWorkingSetBytes{0};
};

/** @brief Receives parsed progress on the blocking Run() caller thread. */
using WorkerProgressSink = std::function<void(const WorkerProgressEvent&)>;

/** @brief Inputs for one isolated worker-process launch. */
struct WorkerLaunchOptions
{
    std::filesystem::path executablePath;
    std::vector<std::string> arguments;
    std::filesystem::path workingDirectory;
    std::filesystem::path cancellationMarkerPath;
    std::chrono::milliseconds timeout{5000};
    std::chrono::milliseconds cancelGracePeriod{2000};
    bool requireTerminalProgress{true};
    WorkerProgressSink progressSink;
};

/** @brief Complete process, protocol, and diagnostic outcome for one Run(). */
struct WorkerRunResult
{
    bool started{false};
    bool forcedTermination{false};
    std::uint32_t processId{0};
    std::uint32_t processExitCode{0};
    WorkerExitCategory exitCategory{WorkerExitCategory::Internal};
    WorkerStopReason stopReason{WorkerStopReason::StartupFailed};
    std::string errorCode;
    std::string errorMessage;
    std::vector<WorkerProgressEvent> progressEvents;
    std::vector<WorkerTimingEvent> timingEvents;
    std::vector<std::string> stdoutLogLines;
    std::vector<std::string> stderrLogLines;
};

/**
 * @brief Owns one Windows worker process tree and the file-contract text transport.
 *
 * One client executes at most one command at a time. Run() blocks the calling
 * thread while RequestCancel() may be called from another thread.
 */
class WorkerClient final
{
public:
    /** @brief Creates an idle worker client. */
    WorkerClient();

    /** @brief Requests cancellation and waits for an active Run() to finish. */
    ~WorkerClient();

    WorkerClient(const WorkerClient&) = delete;
    WorkerClient& operator=(const WorkerClient&) = delete;
    WorkerClient(WorkerClient&&) = delete;
    WorkerClient& operator=(WorkerClient&&) = delete;

    /**
     * @brief Starts a command in a kill-on-close Windows Job Object and waits for it.
     * @param options Executable, UTF-8 arguments, timeout, and optional cancel marker.
     * @return Process, protocol, progress, timing, and diagnostic outcome.
     */
    [[nodiscard]] WorkerRunResult Run(const WorkerLaunchOptions& options);

    /**
     * @brief Requests cooperative cancellation of the active command.
     * @return True when a running command accepted the request.
     */
    bool RequestCancel() noexcept;

    /** @brief Reports whether Run() currently owns a process attempt. */
    [[nodiscard]] bool IsRunning() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace slicesoft::module
