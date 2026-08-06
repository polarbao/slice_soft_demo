#pragma once

#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace slicer_core::api::artifacts
{

/** @brief Job-owned paths used by one package publication attempt. */
struct PackageArtifactIdentity
{
    std::filesystem::path package_directory;
    std::filesystem::path staging_directory;
    std::filesystem::path backup_directory;
    std::filesystem::path lease_directory;
    std::string job_id;
    std::string attempt_id;
};

/** @brief Outcome of one idempotent cleanup or crash-recovery pass. */
struct PackageArtifactRecoveryResult
{
    bool success{false};
    bool target_restored{false};
    bool staging_removed{false};
    bool backup_removed{false};
    bool lease_removed{false};
    std::string error;
    std::vector<std::filesystem::path> residual_paths;
};

/** @brief Callback that accepts only a complete published RGBWSV package. */
using PackageArtifactValidator =
    std::function<bool(const std::filesystem::path&)>;

/** @brief Result of acquiring or releasing the single-target filesystem lease. */
struct PackageArtifactLeaseResult
{
    bool success{false};
    bool conflict{false};
    std::string error;
};

/** @brief Stable output failure raised by job-owned publication handling. */
class PackageArtifactOutputError final : public std::runtime_error
{
public:
    /**
     * @brief Construct one stable publication failure.
     * @param message Diagnostic message without user-controlled formatting.
     */
    explicit PackageArtifactOutputError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

/** @brief Stable conflict raised when another job owns the package target. */
class PackageArtifactLeaseConflict final : public std::runtime_error
{
public:
    /**
     * @brief Construct one target lease conflict.
     * @param message Diagnostic message without user-controlled formatting.
     */
    explicit PackageArtifactLeaseConflict(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

/**
 * @brief Build normalized job-owned package artifact paths.
 * @param packageDirectory Absolute final package directory.
 * @param jobId File-contract job identifier.
 * @param attemptId Unique identifier for this package attempt.
 * @return Validated target, staging, backup, and lease paths.
 * @throws std::invalid_argument When a path or identity is unsafe.
 */
[[nodiscard]] PackageArtifactIdentity MakePackageArtifactIdentity(
    const std::filesystem::path& packageDirectory,
    const std::string& jobId,
    const std::string& attemptId);

/**
 * @brief Derive the deterministic filesystem-safe attempt ID for a request.
 * @param correlationId Non-empty file-contract correlation identity.
 * @return Stable lowercase SHA-256 based attempt token.
 */
[[nodiscard]] std::string MakePackageAttemptId(
    std::string_view correlationId);

/**
 * @brief Report whether a package path names staging, backup, lease, tmp, or bak data.
 * @param path Candidate package path.
 * @return True when the filename has a reserved temporary-artifact marker.
 */
[[nodiscard]] bool IsTemporaryPackagePath(
    const std::filesystem::path& path) noexcept;

/**
 * @brief Acquire the target-wide publication lease for one owner.
 * @param identity Validated job-owned artifact identity.
 * @return Lease result; conflict is true when another owner already holds it.
 */
[[nodiscard]] PackageArtifactLeaseResult AcquirePackageArtifactLease(
    const PackageArtifactIdentity& identity) noexcept;

/**
 * @brief Release the target-wide lease only when its owner matches the identity.
 * @param identity Validated job-owned artifact identity.
 * @return Lease result; a foreign or malformed owner fails closed.
 */
[[nodiscard]] PackageArtifactLeaseResult ReleasePackageArtifactLease(
    const PackageArtifactIdentity& identity) noexcept;

/**
 * @brief Recover or clean only the exact artifacts owned by one attempt.
 * @param identity Validated job-owned artifact identity.
 * @param validator Strict package validator used before backup removal or restoration.
 * @return Idempotent recovery evidence; failures preserve uncertain artifacts.
 */
[[nodiscard]] PackageArtifactRecoveryResult RecoverPackageArtifacts(
    const PackageArtifactIdentity& identity,
    const PackageArtifactValidator& validator) noexcept;

}  // namespace slicer_core::api::artifacts
