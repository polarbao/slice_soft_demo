#pragma once

#include "slicer_worker/runtime/WorkerRequestEnvelope.h"

#include "slicer_core/json_value.h"

#include <chrono>
#include <optional>
#include <string>

namespace slicesoft::worker
{

/** @brief Frozen cleanup evidence used by cancelled worker results. */
class WorkerResultCleanup final
{
public:
    /**
     * @brief Creates cleanup evidence.
     * @param stagingRemoved Whether current staging was removed.
     * @param published Whether a new package was published.
     */
    WorkerResultCleanup(bool stagingRemoved, bool published) noexcept;

    /** @brief Returns whether current staging was removed. */
    [[nodiscard]] bool StagingRemoved() const noexcept;

    /** @brief Returns whether a new package was published. */
    [[nodiscard]] bool Published() const noexcept;

private:
    bool m_stagingRemoved{false};
    bool m_published{false};
};

/** @brief Identity-closed file_contract_v1 result document. */
class WorkerResultEnvelope final
{
public:
    /**
     * @brief Creates a successful result from a trusted request identity.
     * @param request Validated immutable request envelope.
     * @param output Non-empty output object from a real executor.
     * @param engineVersion Non-empty engine version.
     * @param elapsed Non-negative elapsed duration.
     * @return Valid success result with PM-SLICER-OK-0000.
     */
    [[nodiscard]] static WorkerResultEnvelope Success(
        const WorkerRequestEnvelope& request,
        slicer_core::Json output,
        std::string engineVersion,
        std::chrono::duration<double, std::milli> elapsed);

    /**
     * @brief Creates a handled failure from a trusted request identity.
     * @param request Validated immutable request envelope.
     * @param code Frozen non-success PM-SLICER code.
     * @param message Non-empty public failure message.
     * @param detail Optional diagnostic detail.
     * @param engineVersion Non-empty engine version.
     * @param elapsed Non-negative elapsed duration.
     * @param cleanup Optional cleanup evidence; mandatory for cancellation.
     * @return Valid identity-closed failure result.
     */
    [[nodiscard]] static WorkerResultEnvelope Failure(
        const WorkerRequestEnvelope& request,
        std::string code,
        std::string message,
        std::optional<std::string> detail,
        std::string engineVersion,
        std::chrono::duration<double, std::milli> elapsed,
        std::optional<WorkerResultCleanup> cleanup = std::nullopt);

    /** @brief Returns the immutable request identity copied into the result. */
    [[nodiscard]] const WorkerJobIdentity& Identity() const noexcept;

    /** @brief Returns whether the result is successful. */
    [[nodiscard]] bool Ok() const noexcept;

    /** @brief Returns the stable PM-SLICER result code. */
    [[nodiscard]] const std::string& Code() const noexcept;

    /** @brief Maps the stable code to the frozen worker process exit category. */
    [[nodiscard]] int ProcessExitCode() const noexcept;

    /** @brief Serializes the result as a file_contract_v1 JSON object. */
    [[nodiscard]] slicer_core::Json ToJson() const;

private:
    WorkerResultEnvelope(
        WorkerJobIdentity identity,
        bool ok,
        std::string code,
        slicer_core::Json output,
        std::string message,
        std::optional<std::string> detail,
        std::string engineVersion,
        double elapsedMs,
        std::optional<WorkerResultCleanup> cleanup);

    WorkerJobIdentity m_identity;
    bool m_ok{false};
    std::string m_code;
    slicer_core::Json m_output;
    std::string m_message;
    std::optional<std::string> m_detail;
    std::string m_engineVersion;
    double m_elapsedMs{0.0};
    std::optional<WorkerResultCleanup> m_cleanup;
};

}  // namespace slicesoft::worker
