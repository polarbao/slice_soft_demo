#pragma once

#include "slicer_core/config.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"

namespace slicer_core
{

/**
 * @brief Test whether a configuration requests the Stage 12E global partition mode.
 * @param config Slice configuration.
 * @return True when global_surface_shell is enabled.
 */
bool IsGlobalTextureFillPartitionRequested(const SliceConfig& config);

/**
 * @brief Block Stage 12E slicing until a global partition backend is implemented.
 * @param config Slice configuration.
 * @throws TextureFillPartitionError when global partition is requested.
 */
void EnsureGlobalTextureFillPartitionBackendAvailable(const SliceConfig& config);

}  // namespace slicer_core
