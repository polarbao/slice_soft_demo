#include "slicer_core/materials/material_policy/MaterialPolicy.h"

namespace slicer_core
{

MaterialPolicyBoundary MakeMaterialPolicyBoundary(const MaterialPolicyConfig& config)
{
    return MaterialPolicyBoundary{
        config.enabled,
        config.rgb,
        config.white,
        config.varnish,
        config.conflict_policy,
    };
}

}  // namespace slicer_core
