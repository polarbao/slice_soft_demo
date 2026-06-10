#include "slicer_core/support/SupportShapePolicy.h"

namespace slicer_core
{

SupportShapePolicy MakeSupportShapePolicy(const SupportConfig& config)
{
    return SupportShapePolicy{
        config.shape_enabled,
        config.shape_min_component_area_px,
        config.shape_xy_dilation_px,
        config.shape_closing_radius_px,
        config.shape_bridge_gap_px,
        config.shape_preserve_model_priority,
        config.shape_max_added_support_ratio,
    };
}

}  // namespace slicer_core
