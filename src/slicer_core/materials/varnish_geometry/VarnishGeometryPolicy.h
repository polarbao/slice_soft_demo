#pragma once

#include "slicer_core/config.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Varnish geometry modes reserved by the material strategy boundary.
 */
enum class VarnishGeometryMode
{
    InPlaceTopLayers,
    AdditiveGrow,
    CompensatedShrink,
};

/**
 * @brief Varnish geometry strategy object.
 */
struct VarnishGeometryPolicy
{
    VarnishGeometryMode mode{VarnishGeometryMode::InPlaceTopLayers};
    int thickness_layers{0};
    double thickness_mm{0.0};
    std::string compensation_method;
};

/**
 * @brief Create a varnish geometry policy matching current legacy behavior.
 * @param config Legacy material policy config.
 * @return Varnish geometry policy boundary.
 */
VarnishGeometryPolicy MakeLegacyVarnishGeometryPolicy(const MaterialPolicyConfig& config);

/**
 * @brief Convert a varnish geometry mode to a stable name.
 * @param mode Varnish geometry mode.
 * @return Stable mode name.
 */
std::string VarnishGeometryModeName(VarnishGeometryMode mode);

}  // namespace slicer_core
