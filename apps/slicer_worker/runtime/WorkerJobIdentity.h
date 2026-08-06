#pragma once

#include <filesystem>
#include <string>

namespace slicesoft::worker
{

/** @brief Immutable identity and owned paths for one file-contract worker job. */
class WorkerJobIdentity final
{
public:
    /**
     * @brief Creates a validated worker job identity.
     * @param jobId Stable job identifier from request.json.
     * @param correlationId Caller correlation identifier.
     * @param capability Exact worker capability name.
     * @param requestPath Normalized absolute request path.
     */
    WorkerJobIdentity(
        std::string jobId,
        std::string correlationId,
        std::string capability,
        std::filesystem::path requestPath);

    /** @brief Returns the stable job identifier. */
    [[nodiscard]] const std::string& JobId() const noexcept;

    /** @brief Returns the caller correlation identifier. */
    [[nodiscard]] const std::string& CorrelationId() const noexcept;

    /** @brief Returns the exact requested capability. */
    [[nodiscard]] const std::string& Capability() const noexcept;

    /** @brief Returns the normalized absolute request file path. */
    [[nodiscard]] const std::filesystem::path& RequestPath() const noexcept;

    /** @brief Returns the normalized directory owned by this job. */
    [[nodiscard]] const std::filesystem::path& JobDirectory() const noexcept;

    /** @brief Returns the final result document path. */
    [[nodiscard]] const std::filesystem::path& ResultPath() const noexcept;

    /** @brief Returns the temporary result document path. */
    [[nodiscard]] const std::filesystem::path& ResultTemporaryPath() const noexcept;

    /** @brief Returns the cooperative cancellation marker path. */
    [[nodiscard]] const std::filesystem::path& CancelPath() const noexcept;

private:
    std::string m_jobId;
    std::string m_correlationId;
    std::string m_capability;
    std::filesystem::path m_requestPath;
    std::filesystem::path m_jobDirectory;
    std::filesystem::path m_resultPath;
    std::filesystem::path m_resultTemporaryPath;
    std::filesystem::path m_cancelPath;
};

}  // namespace slicesoft::worker
