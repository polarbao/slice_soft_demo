#include "rip_integration/RipArtifactPublisher.h"

#include <chrono>
#include <system_error>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace slicesoft::rip
{
namespace
{

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

std::filesystem::path CanonicalAbsolute(
    const std::filesystem::path& path,
    std::error_code& error)
{
    error.clear();
    if (!path.is_absolute())
    {
        error = std::make_error_code(std::errc::invalid_argument);
        return {};
    }
    return std::filesystem::weakly_canonical(path, error).lexically_normal();
}

bool PathsEqual(
    const std::filesystem::path& left,
    const std::filesystem::path& right)
{
#ifdef _WIN32
    return _wcsicmp(left.c_str(), right.c_str()) == 0;
#else
    return left == right;
#endif
}

RipArtifactPublishResult RenameStagingToOutput(
    const std::filesystem::path& stagingDirectory,
    const std::filesystem::path& outputDirectory)
{
    RipArtifactPublishResult result;
    std::error_code error;
    constexpr int maximumAttempts{5};
    for (int attempt{0}; attempt < maximumAttempts; ++attempt)
    {
        error.clear();
        std::filesystem::rename(stagingDirectory, outputDirectory, error);
        if (!error)
        {
            result.status = RipStatus::Success();
            result.output_directory = outputDirectory;
            return result;
        }
        if (std::filesystem::exists(outputDirectory))
        {
            result.status = RipStatus::Failure(
                "RIP_PUBLISH_OUTPUT_ALREADY_EXISTS",
                "another publisher created the RIP output concurrently");
            return result;
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds{20 * (attempt + 1)});
    }
    result.status = RipStatus::Failure(
        "RIP_PUBLISH_RENAME_FAILED",
        "the validated RIP staging directory could not be atomically published");
    return result;
}

}  // namespace

RipArtifactPublishResult PublishRipArtifact(
    const RipArtifactPublishRequest& request)
{
    RipArtifactPublishResult result;
    if (request.output_directory_name != "rip"
        && request.output_directory_name != "rip_diagnostic")
    {
        result.status = RipStatus::Failure(
            "RIP_PUBLISH_OUTPUT_DIRECTORY_INVALID",
            "the published RIP directory name must be 'rip' or 'rip_diagnostic'");
        return result;
    }
    if (!request.package_directory.is_absolute()
        || !request.staging_directory.is_absolute())
    {
        result.status = RipStatus::Failure(
            "RIP_PUBLISH_PATH_NOT_ABSOLUTE",
            "RIP publication paths must be absolute");
        return result;
    }
    std::error_code error;
    const std::filesystem::path packageDirectory =
        CanonicalAbsolute(request.package_directory, error);
    const std::filesystem::path stagingDirectory =
        CanonicalAbsolute(request.staging_directory, error);
    if (error
        || !std::filesystem::is_directory(packageDirectory, error)
        || error
        || !std::filesystem::is_directory(stagingDirectory, error)
        || error
        || IsReparsePoint(packageDirectory)
        || IsReparsePoint(stagingDirectory)
        || !PathsEqual(stagingDirectory.parent_path(), packageDirectory)
        || !stagingDirectory.filename().string().starts_with(
            ".rip.staging."))
    {
        result.status = RipStatus::Failure(
            "RIP_PUBLISH_STAGING_PATH_INVALID",
            "RIP staging must be a real direct Package child named .rip.staging.<attempt>");
        return result;
    }
    const std::filesystem::path outputDirectory =
        packageDirectory / request.output_directory_name;
    if (!PathsEqual(outputDirectory.parent_path(), packageDirectory))
    {
        result.status = RipStatus::Failure(
            "RIP_PUBLISH_OUTPUT_PATH_ESCAPE",
            "the RIP output path escaped its Package directory");
        return result;
    }
    if (std::filesystem::exists(outputDirectory, error) || error)
    {
        result.status = RipStatus::Failure(
            "RIP_PUBLISH_OUTPUT_ALREADY_EXISTS",
            "an existing RIP output is never replaced automatically");
        return result;
    }

    return RenameStagingToOutput(stagingDirectory, outputDirectory);
}

RipArtifactPublishResult PublishManualRipArtifact(
    const RipManualArtifactPublishRequest& request)
{
    RipArtifactPublishResult result;
    if (!request.staging_directory.is_absolute()
        || !request.output_directory.is_absolute())
    {
        result.status = RipStatus::Failure(
            "RIP_PUBLISH_PATH_NOT_ABSOLUTE",
            "RIP publication paths must be absolute");
        return result;
    }
    std::error_code error;
    const std::filesystem::path stagingDirectory =
        CanonicalAbsolute(request.staging_directory, error);
    if (error
        || !std::filesystem::is_directory(stagingDirectory, error)
        || error
        || IsReparsePoint(stagingDirectory)
        || !stagingDirectory.filename().string().starts_with(
            ".rip.staging.")
        || stagingDirectory.filename().string().size()
            <= sizeof(".rip.staging.") - 1U)
    {
        result.status = RipStatus::Failure(
            "RIP_PUBLISH_STAGING_PATH_INVALID",
            "RIP staging must be a real directory named .rip.staging.<attempt>");
        return result;
    }
    error.clear();
    const std::filesystem::path outputDirectory =
        request.output_directory.lexically_normal();
    const std::filesystem::path outputParent =
        CanonicalAbsolute(outputDirectory.parent_path(), error);
    if (error
        || outputDirectory.filename().empty()
        || !PathsEqual(outputParent, stagingDirectory.parent_path())
        || IsReparsePoint(outputParent))
    {
        result.status = RipStatus::Failure(
            "RIP_PUBLISH_MANUAL_OUTPUT_PATH_INVALID",
            "a manual RIP destination must be a named child of the staging parent");
        return result;
    }
    error.clear();
    if (std::filesystem::exists(outputDirectory, error) || error)
    {
        result.status = RipStatus::Failure(
            "RIP_PUBLISH_OUTPUT_ALREADY_EXISTS",
            "an existing RIP output is never replaced automatically");
        return result;
    }
    return RenameStagingToOutput(stagingDirectory, outputDirectory);
}

}  // namespace slicesoft::rip
