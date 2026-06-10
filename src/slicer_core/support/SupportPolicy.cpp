#include "slicer_core/support/SupportPolicy.h"

namespace slicer_core
{

SupportPolicy MakeSupportPolicy(const SupportConfig& config)
{
    return SupportPolicy{
        config.enabled,
        config.mode,
        config.connectivity,
        config.xy_dilation_px,
    };
}

}  // namespace slicer_core
