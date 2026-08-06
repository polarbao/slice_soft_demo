#include "slicer_core/api/artifacts/PackageArtifactSafety.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace slicer_core::api::artifacts
{
namespace
{

bool IsSafeToken(const std::string& value)
{
    return !value.empty() && std::all_of(
        value.begin(),
        value.end(),
        [](const unsigned char character)
        {
            return std::isalnum(character) != 0
                || character == '-'
                || character == '_';
        });
}

bool IsReparsePoint(const std::filesystem::path& path)
{
    std::error_code error;
    if (std::filesystem::is_symlink(
            std::filesystem::symlink_status(path, error)))
    {
        return true;
    }
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
#else
    return false;
#endif
}

bool RemoveOwnedDirectory(
    const std::filesystem::path& path,
    bool& removed,
    std::string& errorMessage)
{
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error)
    {
        errorMessage = "failed to inspect owned artifact: " + path.generic_string();
        return false;
    }
    if (!exists)
    {
        removed = true;
        return true;
    }
    if (IsReparsePoint(path))
    {
        errorMessage = "owned artifact is a reparse point: " + path.generic_string();
        return false;
    }
    std::filesystem::remove_all(path, error);
    if (error || std::filesystem::exists(path))
    {
        errorMessage = "failed to remove owned artifact: " + path.generic_string();
        return false;
    }
    removed = true;
    return true;
}

void AddResidual(
    const std::filesystem::path& path,
    std::vector<std::filesystem::path>& residuals)
{
    std::error_code error;
    if (std::filesystem::exists(path, error) || error)
    {
        residuals.push_back(path);
    }
}

}  // namespace

PackageArtifactIdentity MakePackageArtifactIdentity(
    const std::filesystem::path& packageDirectory,
    const std::string& jobId,
    const std::string& attemptId)
{
    if (!packageDirectory.is_absolute()
        || packageDirectory.filename().empty()
        || !IsSafeToken(jobId)
        || !IsSafeToken(attemptId)
        || IsTemporaryPackagePath(packageDirectory))
    {
        throw std::invalid_argument(
            "package artifact identity contains an unsafe path or token");
    }
    const std::filesystem::path target =
        packageDirectory.lexically_normal();
    if (target.parent_path().empty())
    {
        throw std::invalid_argument(
            "package artifact target requires a parent directory");
    }
    const std::string prefix = target.filename().string();
    const std::string suffix = jobId + "." + attemptId;

    PackageArtifactIdentity identity;
    identity.package_directory = target;
    identity.staging_directory = target.parent_path()
        / (prefix + ".staging." + suffix);
    identity.backup_directory = target.parent_path()
        / (prefix + ".backup." + suffix);
    identity.lease_directory = target.parent_path()
        / (prefix + ".lease");
    identity.job_id = jobId;
    identity.attempt_id = attemptId;
    return identity;
}

bool IsTemporaryPackagePath(const std::filesystem::path& path) noexcept
{
    std::string filename = path.filename().string();
    std::transform(
        filename.begin(),
        filename.end(),
        filename.begin(),
        [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    constexpr std::string_view markers[]{
        ".staging.", ".backup.", ".lease", ".tmp", ".bak"};
    return std::any_of(
        std::begin(markers),
        std::end(markers),
        [&filename](const std::string_view marker)
        {
            return filename.find(marker) != std::string::npos;
        });
}

PackageArtifactRecoveryResult RecoverPackageArtifacts(
    const PackageArtifactIdentity& identity,
    const PackageArtifactValidator& validator) noexcept
{
    PackageArtifactRecoveryResult result;
    try
    {
        const PackageArtifactIdentity validated = MakePackageArtifactIdentity(
            identity.package_directory,
            identity.job_id,
            identity.attempt_id);
        if (validated.staging_directory != identity.staging_directory
            || validated.backup_directory != identity.backup_directory
            || validated.lease_directory != identity.lease_directory
            || !validator)
        {
            result.error = "package artifact identity or validator is invalid";
            return result;
        }

        std::error_code targetError;
        const bool targetExists = std::filesystem::exists(
            identity.package_directory,
            targetError);
        std::error_code backupError;
        const bool backupExists = std::filesystem::exists(
            identity.backup_directory,
            backupError);
        if (targetError || backupError)
        {
            result.error = "failed to inspect package publication state";
            return result;
        }

        if (!targetExists && backupExists)
        {
            if (IsReparsePoint(identity.backup_directory)
                || !validator(identity.backup_directory))
            {
                result.error = "owned backup is not a valid recoverable package";
                return result;
            }
            std::error_code renameError;
            std::filesystem::rename(
                identity.backup_directory,
                identity.package_directory,
                renameError);
            if (renameError)
            {
                result.error = "failed to restore owned package backup";
                return result;
            }
            result.target_restored = true;
            result.backup_removed = true;
        }
        else if (targetExists && backupExists)
        {
            if (IsReparsePoint(identity.package_directory)
                || !validator(identity.package_directory))
            {
                result.error = "published target is not valid; backup was preserved";
                return result;
            }
            if (!RemoveOwnedDirectory(
                    identity.backup_directory,
                    result.backup_removed,
                    result.error))
            {
                return result;
            }
        }
        else
        {
            result.backup_removed = true;
        }

        if (!RemoveOwnedDirectory(
                identity.staging_directory,
                result.staging_removed,
                result.error)
            || !RemoveOwnedDirectory(
                identity.lease_directory,
                result.lease_removed,
                result.error))
        {
            return result;
        }
        result.success = true;
        return result;
    }
    catch (const std::exception& error)
    {
        result.error = error.what();
    }
    catch (...)
    {
        result.error = "unknown package artifact recovery failure";
    }
    AddResidual(identity.staging_directory, result.residual_paths);
    AddResidual(identity.backup_directory, result.residual_paths);
    AddResidual(identity.lease_directory, result.residual_paths);
    return result;
}

}  // namespace slicer_core::api::artifacts
