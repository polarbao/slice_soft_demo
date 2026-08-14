#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace slicesoft::module
{

/** @brief 由 file_contract_v1 冻结的稳定进程退出分类。 */
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

/** @brief 描述 Worker 进程本次运行的结束原因。 */
enum class WorkerStopReason
{
    Exited,
    StartupFailed,
    ContractViolation,
    Cancelled,
    TimedOut,
    ArtifactCleanupFailed
};

/** @brief 模块清理阶段所需的作业专属生产包标识。 */
struct WorkerPackageArtifactContext
{
    std::filesystem::path packageDirectory;
    std::string jobId;
    std::string attemptId;
};

/** @brief 严格解析后的 SLICE_PROGRESS 载荷。 */
struct WorkerProgressEvent
{
    std::string phase;
    std::uint64_t current{0};
    std::uint64_t total{0};
    std::uint32_t percent{0};
    double elapsedMs{0.0};
};

/** @brief 严格解析后的 SLICE_TIMING 载荷必需字段。 */
struct WorkerTimingEvent
{
    std::string engine;
    double totalMs{0.0};
    std::uint64_t workingSetBytes{0};
    std::uint64_t peakWorkingSetBytes{0};
};

/** @brief 在 Run() 所阻塞的调用线程上接收解析后的进度。 */
using WorkerProgressSink = std::function<void(const WorkerProgressEvent&)>;

/** @brief 启动一个隔离 Worker 进程所需的输入。 */
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
    std::optional<WorkerPackageArtifactContext> packageArtifacts;
};

/** @brief 一次 Run() 的完整进程、协议和诊断结果。 */
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
    bool artifactCleanupAttempted{false};
    bool artifactCleanupSucceeded{false};
    bool artifactTargetRestored{false};
    std::vector<std::filesystem::path> residualArtifactPaths;
};

/**
 * @brief 管理一个 Windows Worker 进程树及文件合同文本传输。
 *
 * 单个客户端同一时刻最多执行一条命令。Run() 阻塞调用线程，
 * RequestCancel() 可由其他线程调用。
 */
class WorkerClient final
{
public:
    /** @brief 创建空闲的 Worker 客户端。 */
    WorkerClient();

    /** @brief 请求取消并等待正在执行的 Run() 结束。 */
    ~WorkerClient();

    WorkerClient(const WorkerClient&) = delete;
    WorkerClient& operator=(const WorkerClient&) = delete;
    WorkerClient(WorkerClient&&) = delete;
    WorkerClient& operator=(WorkerClient&&) = delete;

    /**
     * @brief 在关闭即终止的 Windows Job Object 中启动命令并等待完成。
     * @param options 可执行文件、UTF-8 参数、超时和可选取消标记。
     * @return 进程、协议、进度、计时和诊断结果。
     */
    [[nodiscard]] WorkerRunResult Run(const WorkerLaunchOptions& options);

    /**
     * @brief 请求协作式取消当前命令。
     * @return 运行中的命令接受请求时返回 true。
     */
    bool RequestCancel() noexcept;

    /** @brief 报告 Run() 当前是否正在执行一次进程启动尝试。 */
    [[nodiscard]] bool IsRunning() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace slicesoft::module
