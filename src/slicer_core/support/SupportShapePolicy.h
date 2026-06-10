#pragma once

#include "slicer_core/config.h"

namespace slicer_core
{

/**
 * @brief Support shape optimization policy.
 */
struct SupportShapePolicy
{
    bool enabled{false};
    int min_component_area_px{0};
    int xy_dilation_px{0};
    int closing_radius_px{0};
    int bridge_gap_px{0};
    bool preserve_model_priority{true};
    double max_added_support_ratio{0.25};
};

/**
 * @brief Create a support shape policy from support config.
 * @param config Support config.
 * @return Support shape policy.
 */
SupportShapePolicy MakeSupportShapePolicy(const SupportConfig& config);

}  // namespace slicer_core
