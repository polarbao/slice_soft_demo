#include "slicer_core/materials/process_profile/MaterialProcessProfile.h"

namespace slicer_core
{

MaterialProcessProfileBoundary MakeMaterialProcessProfileBoundary(const MaterialProcessProfileConfig& config)
{
    return MaterialProcessProfileBoundary{
        config.enabled,
        config.name,
        config.target,
        config.rgb,
        config.white,
        config.varnish,
        config.support,
        config.validation,
    };
}

}  // namespace slicer_core
