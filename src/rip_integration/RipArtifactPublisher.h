#pragma once

#include "rip_integration/RipSettings.h"

#include <filesystem>

namespace slicesoft::rip
{

/** @brief Owned staging and final paths for one RIP publication. */
struct RipArtifactPublishRequest
{
    std::filesystem::path package_directory;
    std::filesystem::path staging_directory;
    std::string output_directory_name{"rip"};
};

/** @brief Result of one same-parent atomic directory publication. */
struct RipArtifactPublishResult
{
    RipStatus status;
    std::filesystem::path output_directory;
};

/**
 * @brief Rename a validated staging directory to an approved Package child.
 *
 * Approved names are `rip` for strict S2 output and `rip_diagnostic` for
 * explicitly unvalidated evidence. Existing output fails closed and is never
 * removed or replaced.
 */
[[nodiscard]] RipArtifactPublishResult PublishRipArtifact(
    const RipArtifactPublishRequest& request);

/** @brief Owned staging and operator-chosen destination for a manual run. */
struct RipManualArtifactPublishRequest
{
    std::filesystem::path staging_directory;
    std::filesystem::path output_directory;
};

/**
 * @brief Rename validated staging to an operator-chosen sibling destination.
 *
 * The manual RIP path has no Package to publish into, so the destination is
 * named by the operator instead of being fixed to `rip`. The same-parent
 * atomic rename and the never-replace-existing rule are unchanged, so a
 * failed run still leaves no partial destination behind.
 */
[[nodiscard]] RipArtifactPublishResult PublishManualRipArtifact(
    const RipManualArtifactPublishRequest& request);

}  // namespace slicesoft::rip
