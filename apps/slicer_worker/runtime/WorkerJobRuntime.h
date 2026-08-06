#pragma once

#include "slicer_worker/runtime/WorkerJobDispatcher.h"

#include <filesystem>
#include <string>

namespace slicesoft::worker
{

/** @brief Complete one-process/one-job runtime outcome for the command shell. */
struct WorkerJobRuntimeResult
{
    int processexitcode{1};
    std::string stablecode{"PM-SLICER-INTERNAL-0099"};
    std::string message;
    bool trustedidentity{false};
    bool resultwritten{false};
};

/** @brief Shared parser-dispatcher-result pipeline used by every Worker entry. */
class WorkerJobRuntime final
{
public:
    /**
     * @brief Executes one absolute request path through the shared runtime.
     * @param requestPath Absolute request.json path.
     * @param dispatcher Exact capability registry for this process.
     * @return Stable process outcome and result publication evidence.
     */
    [[nodiscard]] static WorkerJobRuntimeResult Run(
        const std::filesystem::path& requestPath,
        const WorkerJobDispatcher& dispatcher) noexcept;
};

}  // namespace slicesoft::worker
