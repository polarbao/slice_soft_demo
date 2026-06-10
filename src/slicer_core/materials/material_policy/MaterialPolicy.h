#pragma once

#include "slicer_core/config.h"

namespace slicer_core
{

/**
 * @brief Boundary DTO for material policy configuration.
 */
struct MaterialPolicyBoundary
{
    bool enabled{false};
    RgbPolicyConfig rgb;
    WhitePolicyConfig white;
    VarnishPolicyConfig varnish;
    std::string conflict_policy;
};

/**
 * @brief Create a material policy boundary from legacy config.
 * @param config Legacy material policy config.
 * @return Boundary representation for material policy.
 */
MaterialPolicyBoundary MakeMaterialPolicyBoundary(const MaterialPolicyConfig& config);

}  // namespace slicer_core
