#include "slicer_worker/runtime/WorkerCapabilityExecutor.h"

#include <utility>

namespace slicesoft::worker
{

WorkerCapabilityExecutionResult WorkerCapabilityExecutionResult::Success(
    slicer_core::Json output)
{
    return WorkerCapabilityExecutionResult(
        true, std::move(output), {}, {}, std::nullopt, std::nullopt);
}

WorkerCapabilityExecutionResult WorkerCapabilityExecutionResult::Failure(
    std::string code,
    std::string message,
    std::optional<std::string> detail,
    std::optional<WorkerResultCleanup> cleanup)
{
    return WorkerCapabilityExecutionResult(
        false,
        nullptr,
        std::move(code),
        std::move(message),
        std::move(detail),
        std::move(cleanup));
}

WorkerCapabilityExecutionResult::WorkerCapabilityExecutionResult(
    const bool ok,
    slicer_core::Json output,
    std::string code,
    std::string message,
    std::optional<std::string> detail,
    std::optional<WorkerResultCleanup> cleanup)
    : m_ok(ok),
      m_output(std::move(output)),
      m_code(std::move(code)),
      m_message(std::move(message)),
      m_detail(std::move(detail)),
      m_cleanup(std::move(cleanup))
{
}

bool WorkerCapabilityExecutionResult::Ok() const noexcept
{
    return m_ok;
}

const slicer_core::Json& WorkerCapabilityExecutionResult::Output() const noexcept
{
    return m_output;
}

const std::string& WorkerCapabilityExecutionResult::Code() const noexcept
{
    return m_code;
}

const std::string& WorkerCapabilityExecutionResult::Message() const noexcept
{
    return m_message;
}

const std::optional<std::string>& WorkerCapabilityExecutionResult::Detail() const noexcept
{
    return m_detail;
}

const std::optional<WorkerResultCleanup>& WorkerCapabilityExecutionResult::Cleanup() const noexcept
{
    return m_cleanup;
}

}  // namespace slicesoft::worker
