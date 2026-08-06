#pragma once

#include <filesystem>
#include <functional>
#include <string>
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
 * @brief Report whether a package path names staging, backup, lease, tmp, or bak data.
 * @param path Candidate package path.
 * @return True when the filename has a reserved temporary-artifact marker.
 */
[[nodiscard]] bool IsTemporaryPackagePath(
    const std::filesystem::path& path) noexcept;

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
