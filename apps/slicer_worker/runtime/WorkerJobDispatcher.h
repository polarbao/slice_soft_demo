#pragma once

#include "slicer_worker/runtime/WorkerCapabilityExecutor.h"
#include "slicer_worker/runtime/WorkerResultEnvelope.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace slicesoft::worker
{

/** @brief Exact one-job capability registry and fail-closed dispatcher. */
class WorkerJobDispatcher final
{
public:
    /**
     * @brief Registers one executor for an exact frozen capability.
     * @param capability Exact capability name.
     * @param executor Unique production or test executor implementation.
     * @throws std::invalid_argument For unknown, null, or duplicate registration.
     */
    void Register(
        std::string capability,
        std::unique_ptr<IWorkerCapabilityExecutor> executor);

    /**
     * @brief Dispatches one request and creates an identity-closed result.
     * @param request Immutable validated request.
     * @return Success from the exact executor or a stable fail-closed result.
     */
    [[nodiscard]] WorkerResultEnvelope Dispatch(
        const WorkerRequestEnvelope& request) const;

private:
    std::unordered_map<std::string, std::unique_ptr<IWorkerCapabilityExecutor>> m_executors;
};

}  // namespace slicesoft::worker
