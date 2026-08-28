#include "HostImportPlacementPolicy.h"

bool HostImportPlacementPolicy::RequiresGridLayout(
    const int instanceCount) noexcept
{
    return instanceCount > 0;
}
