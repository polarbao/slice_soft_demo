#pragma once

#include "slicer_worker/runtime/WorkerJobDispatcher.h"

#include <filesystem>
#include <string>

namespace slicesoft::worker
{

/** @brief 命令行外壳的一进程一作业完整运行结果。 */
struct WorkerJobRuntimeResult
{
    int processexitcode{1};
    std::string stablecode{"PM-SLICER-INTERNAL-0099"};
    std::string message;
    bool trustedidentity{false};
    bool resultwritten{false};
};

/** @brief 所有 Worker 入口共用的解析、分派与结果流水线。 */
class WorkerJobRuntime final
{
public:
    /**
     * @brief 通过共享运行时执行一个请求绝对路径。
     * @param requestPath request.json 的绝对路径。
     * @param dispatcher 本进程的精确能力注册表。
     * @return 稳定进程结果与结果发布证据。
     */
    [[nodiscard]] static WorkerJobRuntimeResult Run(
        const std::filesystem::path& requestPath,
        const WorkerJobDispatcher& dispatcher) noexcept;
};

}  // namespace slicesoft::worker
