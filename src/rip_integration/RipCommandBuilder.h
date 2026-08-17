#pragma once

#include "rip_integration/RipSettings.h"

#include <filesystem>
#include <string>
#include <vector>

namespace slicesoft::rip
{

/** @brief Relocatable external RIP runtime files resolved by its manifest. */
struct RipRuntimePaths
{
    std::filesystem::path module_directory;
    std::filesystem::path cli_path;
    std::filesystem::path dll_path;
    std::filesystem::path resource_directory;
};

/** @brief Inputs needed to construct one shell-free external RIP command. */
struct RipCommandRequest
{
    RipRuntimePaths runtime;
    std::filesystem::path package_directory;
    std::filesystem::path input_directory;
    std::filesystem::path staging_output_directory;
    RipSettings settings;
};

/** @brief Program, raw argv entries, and working directory for QProcess. */
struct RipCommand
{
    std::filesystem::path program;
    std::filesystem::path working_directory;
    std::vector<std::string> arguments;
};

/**
 * @brief Build an absolute-path command without shell quoting or expansion.
 * @param request Runtime, Package, staging and processing settings.
 * @param command Receives the validated command.
 * @return Stable success or fail-closed validation status.
 */
[[nodiscard]] RipStatus BuildRipCommand(
    const RipCommandRequest& request,
    RipCommand* command);

}  // namespace slicesoft::rip
