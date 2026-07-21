#pragma once

#include "slicer_core/preflight/ModelPreflightTypes.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Compute the deterministic cache key for a model preflight identity.
 * @param identity Source, resource, transform, options and algorithm identity.
 * @return Lowercase 64-character SHA-256 key.
 */
std::string ComputeModelPreflightCacheKey(
    const ModelPreflightCacheIdentity& identity);

}  // namespace slicer_core
