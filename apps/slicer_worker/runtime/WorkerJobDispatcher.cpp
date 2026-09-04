#include "slicer_worker/runtime/WorkerJobDispatcher.h"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

namespace slicesoft::worker
{
namespace
{

constexpr std::string_view SliceCapability{"slice.rgbwsv"};
constexpr std::string_view TransferSliceCapability{"slice.rgbwsvt"};
constexpr std::string_view PreflightCapability{"geometry.preflight.full"};
constexpr std::string_view RepairCapability{"geometry.repair"};
constexpr std::string_view EngineVersion{"0.1.0"};

class FileCancellationToken final : public slicer_core::api::ICancelToken
{
public:
    explicit FileCancellationToken(std::filesystem::path cancelPath)
        : m_cancelPath(std::move(cancelPath))
    {
    }

    [[nodiscard]] bool IsCancelRequested() const noexcept override
    {
        std::error_code error;
        const bool exists = std::filesystem::is_regular_file(m_cancelPath, error);
        return !error && exists;
    }

private:
    std::filesystem::path m_cancelPath;
};

bool IsKnownCapability(const std::string_view capability)
{
    return capability == SliceCapability
        || capability == TransferSliceCapability
        || capability == PreflightCapability
        || capability == RepairCapability;
}

WorkerResultEnvelope InternalFailure(
    const WorkerRequestEnvelope& request,
    const std::chrono::steady_clock::time_point startedAt,
    std::string message,
    std::optional<std::string> detail = std::nullopt)
{
    return WorkerResultEnvelope::Failure(
        request,
        "PM-SLICER-INTERNAL-0099",
        std::move(message),
        std::move(detail),
        std::string{EngineVersion},
        std::chrono::steady_clock::now() - startedAt);
}

}  // namespace

void WorkerJobDispatcher::Register(
    std::string capability,
    std::unique_ptr<IWorkerCapabilityExecutor> executor)
{
    if (!IsKnownCapability(capability))
    {
        throw std::invalid_argument("cannot register an unknown worker capability");
    }
    if (executor == nullptr)
    {
        throw std::invalid_argument("cannot register a null worker executor");
    }
    if (m_executors.contains(capability))
    {
        throw std::invalid_argument("worker capability already has an executor");
    }
    m_executors.emplace(std::move(capability), std::move(executor));
}

WorkerResultEnvelope WorkerJobDispatcher::Dispatch(
    const WorkerRequestEnvelope& request) const
{
    const auto startedAt = std::chrono::steady_clock::now();
    const FileCancellationToken cancelToken{request.Identity().CancelPath()};
    if (cancelToken.IsCancelRequested())
    {
        return WorkerResultEnvelope::Failure(
            request,
            "PM-SLICER-CANCELLED-0070",
            "worker cancellation was requested before dispatch",
            std::nullopt,
            std::string{EngineVersion},
            std::chrono::steady_clock::now() - startedAt,
            WorkerResultCleanup{true, false});
    }

    const auto found = m_executors.find(request.Identity().Capability());
    if (found == m_executors.end())
    {
        return InternalFailure(
            request,
            startedAt,
            "requested worker capability has no installed production executor");
    }

    try
    {
        WorkerCapabilityExecutionResult execution =
            found->second->Execute(request, cancelToken);
        const auto elapsed = std::chrono::steady_clock::now() - startedAt;
        if (execution.Ok())
        {
            return WorkerResultEnvelope::Success(
                request,
                execution.Output(),
                std::string{EngineVersion},
                elapsed);
        }
        return WorkerResultEnvelope::Failure(
            request,
            execution.Code(),
            execution.Message(),
            execution.Detail(),
            std::string{EngineVersion},
            elapsed,
            execution.Cleanup());
    }
    catch (const std::exception& error)
    {
        return InternalFailure(
            request,
            startedAt,
            "worker capability executor failed",
            std::string{error.what()});
    }
    catch (...)
    {
        return InternalFailure(
            request,
            startedAt,
            "worker capability executor failed with an unknown exception");
    }
}

}  // namespace slicesoft::worker
