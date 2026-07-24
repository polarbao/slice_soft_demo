#pragma once

#include "slicer_core/config.h"

namespace slicer_core
{

/**
 * @brief Physical-to-raster conversion for an outside varnish shell.
 */
struct OuterVarnishDiscretization
{
    bool enabled{false};
    double requested_thickness_mm{0.0};
    double pixel_size_x_mm{0.0};
    double pixel_size_y_mm{0.0};
    int radius_x_px{0};
    int radius_y_px{0};
    double effective_thickness_x_mm{0.0};
    double effective_thickness_y_mm{0.0};
};

/**
 * @brief Convert an outside varnish thickness to independent X/Y raster radii.
 * @param config Validated outside varnish configuration.
 * @param pixelSizeXmm Physical X size of one raster pixel in millimeters.
 * @param pixelSizeYmm Physical Y size of one raster pixel in millimeters.
 * @return Disabled zero values or the independent-axis discretization.
 * @throws std::invalid_argument When an enabled shell receives an invalid pixel size.
 */
OuterVarnishDiscretization ComputeOuterVarnishDiscretization(
    const OuterVarnishShellConfig& config,
    double pixelSizeXmm,
    double pixelSizeYmm);

/**
 * @brief Test whether a raster offset is inside the requested physical shell radius.
 * @param discretization Valid outside varnish discretization.
 * @param offsetX X-axis pixel offset.
 * @param offsetY Y-axis pixel offset.
 * @return True when the offset center is within the requested physical thickness.
 */
bool IsOuterVarnishOffsetWithinThickness(
    const OuterVarnishDiscretization& discretization,
    int offsetX,
    int offsetY) noexcept;

}  // namespace slicer_core
