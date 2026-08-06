#pragma once

#include "slicer_worker/runtime/WorkerRequestEnvelope.h"
#include "slicer_worker/runtime/WorkerResultEnvelope.h"

#include "slicer_core/api/Cancellation.h"
#include "slicer_core/json_value.h"

#include <optional>
#include <string>

namespace slicesoft::worker
{

/** @brief Algorithm result without authority to rewrite worker job identity. */
class WorkerCapabilityExecutionResult final
{
public:
    /** @brief Creates an executor success with a non-empty business output object. */
    [[nodiscard]] static WorkerCapabilityExecutionResult Success(
        slicer_core::Json output);

    /** @brief Creates an executor failure with a stable code and message. */
    [[nodiscard]] static WorkerCapabilityExecutionResult Failure(
        std::string code,
        std::string message,
        std::optional<std::string> detail = std::nullopt,
        std::optional<WorkerResultCleanup> cleanup = std::nullopt);

    /** @brief Returns whether execution succeeded. */
    [[nodiscard]] bool Ok() const noexcept;

    /** @brief Returns business output without an identity envelope. */
    [[nodiscard]] const slicer_core::Json& Output() const noexcept;

    /** @brief Returns the stable failure code. */
    [[nodiscard]] const std::string& Code() const noexcept;

    /** @brief Returns the failure message. */
    [[nodiscard]] const std::string& Message() const noexcept;

    /** @brief Returns optional diagnostic detail. */
    [[nodiscard]] const std::optional<std::string>& Detail() const noexcept;

    /** @brief Returns optional cleanup evidence. */
    [[nodiscard]] const std::optional<WorkerResultCleanup>& Cleanup() const noexcept;

private:
    WorkerCapabilityExecutionResult(
        bool ok,
        slicer_core::Json output,
        std::string code,
        std::string message,
        std::optional<std::string> detail,
        std::optional<WorkerResultCleanup> cleanup);

    bool m_ok{false};
    slicer_core::Json m_output;
    std::string m_code;
    std::string m_message;
    std::optional<std::string> m_detail;
    std::optional<WorkerResultCleanup> m_cleanup;
};

/** @brief Worker-private execution port for one exact heavy capability. */
class IWorkerCapabilityExecutor
{
public:
    virtual ~IWorkerCapabilityExecutor() = default;

    /**
     * @brief Executes one validated request without owning result identity.
     * @param request Immutable validated request.
     * @param cancelToken Cooperative cancellation token owned by the dispatcher.
     * @return Business result used by the dispatcher to build the result envelope.
     */
    [[nodiscard]] virtual WorkerCapabilityExecutionResult Execute(
        const WorkerRequestEnvelope& request,
        const slicer_core::api::ICancelToken& cancelToken) = 0;
};

}  // namespace slicesoft::worker
