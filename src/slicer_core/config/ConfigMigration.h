#pragma once

#include "slicer_core/config/NormalizedConfig.h"
#include "slicer_core/json_value.h"

namespace slicer_core
{

/**
 * @brief Normalize a config JSON root into the legacy shape consumed by SliceConfig.
 * @param root Config JSON root.
 * @return Normalized config containing schema metadata and legacy-compatible JSON.
 */
NormalizedConfig NormalizeConfigRoot(const Json& root);

/**
 * @brief Normalize a config JSON root and return only the legacy-compatible JSON.
 * @param root Config JSON root.
 * @return Legacy-compatible config JSON.
 */
Json NormalizeConfigJson(const Json& root);

}  // namespace slicer_core
