#include "HostImportPlacementPolicy.h"

bool HostImportPlacementPolicy::RequiresGridLayout(
    const int instanceCount,
    const bool autoLayoutEnabled) noexcept
{
    return autoLayoutEnabled && instanceCount > 0;
}
