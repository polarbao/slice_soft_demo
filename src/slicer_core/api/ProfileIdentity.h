#pragma once

#include "slicer_core/json_value.h"

#include <string>

namespace slicer_core::api
{

/**
 * @brief Compute the canonical effective-Profile identity used by Worker requests.
 * @param profile Profile JSON with or without its self-declared profileHash field.
 * @return `sha256:` followed by the lowercase canonical document digest.
 * @throws std::invalid_argument When profile is not a JSON object.
 */
[[nodiscard]] std::string ComputeProfileDocumentHash(
    const Json& profile);

}  // namespace slicer_core::api
