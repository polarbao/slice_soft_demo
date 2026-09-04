#include "slicer_worker/runtime/WorkerResultEnvelope.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace slicesoft::worker
{
namespace
{

constexpr std::string_view SuccessCode{"PM-SLICER-OK-0000"};
constexpr std::string_view CancelledCode{"PM-SLICER-CANCELLED-0070"};
constexpr std::array<std::string_view, 17> StableCodes{
    SuccessCode,
    "PM-SLICER-INPUT-0001",
    "PM-SLICER-INPUT-0002",
    "PM-SLICER-TOPOLOGY-0010",
    "PM-SLICER-TOPOLOGY-0011",
    "PM-SLICER-LAYOUT-0020",
    "PM-SLICER-LAYOUT-0021",
    "PM-SLICER-LAYOUT-0022",
    "PM-SLICER-LAYOUT-0023",
    "PM-SLICER-PROFILE-0030",
    "PM-SLICER-PROFILE-0031",
    "PM-SLICER-RESOURCE-0040",
    "PM-SLICER-RESOURCE-0041",
    "PM-SLICER-OUTPUT-0050",
    "PM-SLICER-CONTRACT-0060",
    CancelledCode,
    "PM-SLICER-INTERNAL-0099"};

bool IsStableCode(const std::string& code)
{
    for (const std::string_view stableCode : StableCodes)
    {
        if (code == stableCode)
        {
            return true;
        }
    }
    return false;
}

int ExitCodeFor(const std::string_view code) noexcept
{
    if (code == SuccessCode)
    {
        return 0;
    }
    if (code.starts_with("PM-SLICER-INPUT-"))
    {
        return 2;
    }
    if (code.starts_with("PM-SLICER-PROFILE-"))
    {
        return 3;
    }
    if (code.starts_with("PM-SLICER-TOPOLOGY-"))
    {
        return 4;
    }
    if (code.starts_with("PM-SLICER-RESOURCE-"))
    {
        return 5;
    }
    if (code.starts_with("PM-SLICER-OUTPUT-"))
    {
        return 6;
    }
    if (code.starts_with("PM-SLICER-CONTRACT-"))
    {
        return 7;
    }
    if (code.starts_with("PM-SLICER-CANCELLED-"))
    {
        return 8;
    }
    return 1;
}

void ValidateCommon(
    const std::string& engineVersion,
    const double elapsedMs)
{
    if (engineVersion.empty())
    {
        throw std::invalid_argument("worker result engineVersion must not be empty");
    }
    if (!std::isfinite(elapsedMs) || elapsedMs < 0.0)
    {
        throw std::invalid_argument("worker result elapsedMs must be finite and non-negative");
    }
}

}  // namespace

WorkerResultCleanup::WorkerResultCleanup(
    const bool stagingRemoved,
    const bool published) noexcept
    : m_stagingRemoved(stagingRemoved), m_published(published)
{
}

bool WorkerResultCleanup::StagingRemoved() const noexcept
{
    return m_stagingRemoved;
}

bool WorkerResultCleanup::Published() const noexcept
{
    return m_published;
}

WorkerResultEnvelope WorkerResultEnvelope::Success(
    const WorkerRequestEnvelope& request,
    slicer_core::Json output,
    std::string engineVersion,
    const std::chrono::duration<double, std::milli> elapsed)
{
    ValidateCommon(engineVersion, elapsed.count());
    if (!output.is_object() || output.size() == 0U)
    {
        throw std::invalid_argument(
            "successful worker result requires a non-empty output object");
    }
    return WorkerResultEnvelope(
        request.Identity(),
        request.Minor(),
        true,
        std::string{SuccessCode},
        std::move(output),
        {},
        std::nullopt,
        std::move(engineVersion),
        elapsed.count(),
        std::nullopt);
}

WorkerResultEnvelope WorkerResultEnvelope::Failure(
    const WorkerRequestEnvelope& request,
    std::string code,
    std::string message,
    std::optional<std::string> detail,
    std::string engineVersion,
    const std::chrono::duration<double, std::milli> elapsed,
    std::optional<WorkerResultCleanup> cleanup)
{
    ValidateCommon(engineVersion, elapsed.count());
    if (!IsStableCode(code) || code == SuccessCode)
    {
        throw std::invalid_argument(
            "failed worker result requires a frozen non-success PM-SLICER code");
    }
    if (message.empty())
    {
        throw std::invalid_argument("failed worker result requires a non-empty message");
    }
    if (detail.has_value() && detail->empty())
    {
        throw std::invalid_argument("worker result detail must be absent or non-empty");
    }
    if (code == CancelledCode
        && (!cleanup.has_value() || !cleanup->StagingRemoved() || cleanup->Published()))
    {
        throw std::invalid_argument(
            "cancelled worker result requires stagingRemoved=true and published=false");
    }
    return WorkerResultEnvelope(
        request.Identity(),
        request.Minor(),
        false,
        std::move(code),
        nullptr,
        std::move(message),
        std::move(detail),
        std::move(engineVersion),
        elapsed.count(),
        std::move(cleanup));
}

WorkerResultEnvelope::WorkerResultEnvelope(
    WorkerJobIdentity identity,
    const std::uint32_t minor,
    const bool ok,
    std::string code,
    slicer_core::Json output,
    std::string message,
    std::optional<std::string> detail,
    std::string engineVersion,
    const double elapsedMs,
    std::optional<WorkerResultCleanup> cleanup)
    : m_identity(std::move(identity)),
      m_minor(minor),
      m_ok(ok),
      m_code(std::move(code)),
      m_output(std::move(output)),
      m_message(std::move(message)),
      m_detail(std::move(detail)),
      m_engineVersion(std::move(engineVersion)),
      m_elapsedMs(elapsedMs),
      m_cleanup(std::move(cleanup))
{
}

const WorkerJobIdentity& WorkerResultEnvelope::Identity() const noexcept
{
    return m_identity;
}

bool WorkerResultEnvelope::Ok() const noexcept
{
    return m_ok;
}

const std::string& WorkerResultEnvelope::Code() const noexcept
{
    return m_code;
}

int WorkerResultEnvelope::ProcessExitCode() const noexcept
{
    return ExitCodeFor(m_code);
}

slicer_core::Json WorkerResultEnvelope::ToJson() const
{
    slicer_core::Json::Object document{
        {"contract", "file_contract"},
        {"major", 1},
        {"minor", static_cast<double>(m_minor)},
        {"jobId", m_identity.JobId()},
        {"correlationId", m_identity.CorrelationId()},
        {"capability", m_identity.Capability()},
        {"ok", m_ok},
        {"code", m_code},
        {"engineVersion", m_engineVersion},
        {"elapsedMs", m_elapsedMs}};
    if (m_ok)
    {
        document.emplace("output", m_output);
    }
    else
    {
        slicer_core::Json::Object error{{"message", m_message}};
        if (m_detail.has_value())
        {
            error.emplace("detail", *m_detail);
        }
        document.emplace("error", slicer_core::Json{std::move(error)});
    }
    if (m_cleanup.has_value())
    {
        document.emplace("cleanup", slicer_core::Json::object({
            {"stagingRemoved", m_cleanup->StagingRemoved()},
            {"published", m_cleanup->Published()}}));
    }
    return slicer_core::Json{std::move(document)};
}

}  // namespace slicesoft::worker
