#include "slicer_core/materials/varnish_geometry/VarnishGeometryPolicy.h"

namespace slicer_core
{

VarnishGeometryPolicy MakeLegacyVarnishGeometryPolicy(const MaterialPolicyConfig& config)
{
    VarnishGeometryPolicy policy;
    policy.mode = VarnishGeometryMode::InPlaceTopLayers;
    policy.thickness_layers = config.varnish.top_layers;
    return policy;
}

std::string VarnishGeometryModeName(const VarnishGeometryMode mode)
{
    switch (mode)
    {
    case VarnishGeometryMode::InPlaceTopLayers:
        return "in_place_top_layers";
    case VarnishGeometryMode::AdditiveGrow:
        return "additive_grow";
    case VarnishGeometryMode::CompensatedShrink:
        return "compensated_shrink";
    }
    return "unknown";
}

}  // namespace slicer_core
