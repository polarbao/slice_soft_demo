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
 * @brief Rename a validated staging directory to Package/rip.
 *
 * Existing output fails closed and is never removed or replaced.
 */
[[nodiscard]] RipArtifactPublishResult PublishRipArtifact(
    const RipArtifactPublishRequest& request);

}  // namespace slicesoft::rip
