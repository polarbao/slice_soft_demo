#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace slicesoft::module
{

/**
 * @brief Worker 不自行退出时，模块在强杀前等待的优雅退出宽限期。
 *
 * 这个值是**取消到终态的时间下界**：`WorkerClient.cpp` 的运行循环先等满它，
 * 到点才 `TerminateJobObject`；而作业状态要等 `Run()` 返回才转终态
 * （`WorkerJobService.cpp` 的 Finalize 路径）。
 *
 * 因此**任何取消期限门禁都必须大于它**，否则「worker 未自行退出」这条路径
 * 必然判失败。2026-09-18 的 F-53 正是这么来的：门禁期限与本值同为 2000ms，
 * 余量为负。不变式由 `tests/contracts/ValidateCancelDeadlineInvariant.py` 看守。
 */
inline constexpr std::chrono::milliseconds kDefaultCancelGracePeriod{2000};

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
    std::chrono::milliseconds cancelGracePeriod{kDefaultCancelGracePeriod};
    bool requireTerminalProgress{true};
    WorkerProgressSink progressSink;
    // Optional telemetry; callback exceptions never affect the Worker contract.
    std::function<void(std::string_view, std::uint32_t)> diagnosticSink;
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
    // 以下四个容器有上界，超限后丢弃【中段】条目并计数，首段与末段始终保留。
    // 末段必须保留：file_contract_v1 §4 要求终态成功前发出 percent=100，
    // 且 EngineConformanceGate / EngineConformanceSupport / stage16 用例共三处
    // 断言 progressEvents.back().percent == 100。丢末尾会直接打断这条不变量。
    std::vector<WorkerProgressEvent> progressEvents;
    std::vector<WorkerTimingEvent> timingEvents;
    std::vector<std::string> stdoutLogLines;
    std::vector<std::string> stderrLogLines;
    std::uint64_t droppedProgressEvents{0};
    std::uint64_t droppedTimingEvents{0};
    std::uint64_t droppedStdoutLogLines{0};
    std::uint64_t droppedStderrLogLines{0};
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
