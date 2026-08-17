#include "rip_integration/RipCommandBuilder.h"

#include <algorithm>
#include <cwctype>
#include <system_error>

namespace slicesoft::rip
{
namespace
{

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

bool TryCanonicalAbsolute(
    const std::filesystem::path& source,
    std::filesystem::path* destination)
{
    std::error_code error;
    *destination = CanonicalAbsolute(source, error);
    return !error;
}

bool ComponentEqual(
    const std::filesystem::path& left,
    const std::filesystem::path& right)
{
#ifdef _WIN32
    std::wstring leftValue = left.native();
    std::wstring rightValue = right.native();
    std::transform(
        leftValue.begin(), leftValue.end(), leftValue.begin(),
        [](const wchar_t value)
        {
            return static_cast<wchar_t>(std::towlower(value));
        });
    std::transform(
        rightValue.begin(), rightValue.end(), rightValue.begin(),
        [](const wchar_t value)
        {
            return static_cast<wchar_t>(std::towlower(value));
        });
    return leftValue == rightValue;
#else
    return left == right;
#endif
}

bool IsContained(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate,
    const bool allowRoot = false)
{
    std::error_code rootError;
    std::error_code candidateError;
    const std::filesystem::path normalizedRoot =
        CanonicalAbsolute(root, rootError);
    const std::filesystem::path normalizedCandidate =
        CanonicalAbsolute(candidate, candidateError);
    if (rootError || candidateError)
    {
        return false;
    }
    auto rootPart = normalizedRoot.begin();
    auto candidatePart = normalizedCandidate.begin();
    for (; rootPart != normalizedRoot.end(); ++rootPart, ++candidatePart)
    {
        if (candidatePart == normalizedCandidate.end()
            || !ComponentEqual(*rootPart, *candidatePart))
        {
            return false;
        }
    }
    return allowRoot || candidatePart != normalizedCandidate.end();
}

bool IsRegularFile(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

bool IsDirectory(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_directory(path, error) && !error;
}

std::string PathArgument(const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    return std::string{
        reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

}  // namespace

RipStatus BuildRipCommand(
    const RipCommandRequest& request,
    RipCommand* command)
{
    if (command == nullptr)
    {
        return RipStatus::Failure(
            "RIP_COMMAND_OUTPUT_MISSING",
            "a command output object is required");
    }
    *command = {};
    const RipStatus settingsStatus = ValidateRipSettings(request.settings);
    if (!settingsStatus.ok)
    {
        return settingsStatus;
    }

    const RipRuntimePaths& runtime = request.runtime;
    if (!runtime.module_directory.is_absolute()
        || !request.package_directory.is_absolute()
        || !request.input_directory.is_absolute()
        || !request.staging_output_directory.is_absolute()
        || !runtime.cli_path.is_absolute()
        || !runtime.dll_path.is_absolute()
        || !runtime.resource_directory.is_absolute()
        || !request.settings.input_icc_path.is_absolute()
        || !request.settings.output_icc_path.is_absolute())
    {
        return RipStatus::Failure(
            "RIP_COMMAND_PATH_NOT_ABSOLUTE",
            "all RIP command and resource paths must be absolute");
    }
    if (!IsContained(runtime.module_directory, runtime.cli_path)
        || !IsContained(runtime.module_directory, runtime.dll_path)
        || !IsContained(
            runtime.module_directory, runtime.resource_directory)
        || !IsContained(
            runtime.resource_directory, request.settings.input_icc_path)
        || !IsContained(
            runtime.resource_directory, request.settings.output_icc_path))
    {
        return RipStatus::Failure(
            "RIP_COMMAND_RUNTIME_PATH_ESCAPE",
            "RIP binaries and resources must remain under the module directory");
    }
    if (!IsContained(request.package_directory, request.input_directory)
        || !IsContained(
            request.package_directory,
            request.staging_output_directory))
    {
        return RipStatus::Failure(
            "RIP_COMMAND_PACKAGE_PATH_ESCAPE",
            "RIP input and staging output must remain under the Package directory");
    }
    if (!IsDirectory(runtime.module_directory)
        || !IsRegularFile(runtime.cli_path)
        || !IsRegularFile(runtime.dll_path)
        || !IsDirectory(runtime.resource_directory)
        || !IsRegularFile(request.settings.input_icc_path)
        || !IsRegularFile(request.settings.output_icc_path)
        || !IsDirectory(request.package_directory)
        || !IsDirectory(request.input_directory))
    {
        return RipStatus::Failure(
            "RIP_COMMAND_REQUIRED_PATH_MISSING",
            "one or more RIP binaries, resources, ICC files, or input directories are missing");
    }

    std::filesystem::path packagePath;
    std::filesystem::path stagingPath;
    std::filesystem::path cliPath;
    std::filesystem::path dllPath;
    std::filesystem::path inputPath;
    std::filesystem::path resourcePath;
    std::filesystem::path inputIccPath;
    std::filesystem::path outputIccPath;
    if (!TryCanonicalAbsolute(request.package_directory, &packagePath)
        || !TryCanonicalAbsolute(
            request.staging_output_directory, &stagingPath)
        || !TryCanonicalAbsolute(runtime.cli_path, &cliPath)
        || !TryCanonicalAbsolute(runtime.dll_path, &dllPath)
        || !TryCanonicalAbsolute(request.input_directory, &inputPath)
        || !TryCanonicalAbsolute(
            runtime.resource_directory, &resourcePath)
        || !TryCanonicalAbsolute(
            request.settings.input_icc_path, &inputIccPath)
        || !TryCanonicalAbsolute(
            request.settings.output_icc_path, &outputIccPath))
    {
        return RipStatus::Failure(
            "RIP_COMMAND_ARGUMENT_PATH_INVALID",
            "a RIP command argument path cannot be canonicalized");
    }
    const std::filesystem::path stageParent = stagingPath.parent_path();
    if (!ComponentEqual(stageParent, packagePath)
        || !stagingPath.filename().string().starts_with(
            ".rip.staging."))
    {
        return RipStatus::Failure(
            "RIP_COMMAND_STAGING_PATH_INVALID",
            "the staging directory must be a direct Package child named .rip.staging.<attempt>");
    }

    command->program = cliPath;
    command->working_directory = cliPath.parent_path();
    command->arguments = {
        "--dll", PathArgument(dllPath),
        "-i", PathArgument(inputPath),
        "-o", PathArgument(stagingPath),
        "-r", PathArgument(resourcePath),
        "--rgb-icc", PathArgument(inputIccPath),
        "--cmyk-icc", PathArgument(outputIccPath),
        "--intent", std::to_string(request.settings.intent),
        "--transparent", request.settings.transparent ? "1" : "0",
        "--colormode", std::to_string(request.settings.color_mode)};
    if (request.settings.continue_on_layer_error)
    {
        command->arguments.emplace_back("-k");
    }
    return RipStatus::Success();
}

}  // namespace slicesoft::rip
