#include "slicer_core/materials/varnish_geometry/OuterVarnishDiscretization.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace slicer_core
{

OuterVarnishDiscretization ComputeOuterVarnishDiscretization(
    const OuterVarnishShellConfig& config,
    const double pixelSizeXmm,
    const double pixelSizeYmm)
{
    OuterVarnishDiscretization result;
    if (!config.enabled || config.thickness_mm <= 0.0)
    {
        return result;
    }
    if (!std::isfinite(pixelSizeXmm)
        || !std::isfinite(pixelSizeYmm)
        || pixelSizeXmm <= 0.0
        || pixelSizeYmm <= 0.0)
    {
        throw std::invalid_argument(
            "outer varnish pixel sizes must be finite and positive");
    }

    result.enabled = true;
    result.requested_thickness_mm = config.thickness_mm;
    result.pixel_size_x_mm = pixelSizeXmm;
    result.pixel_size_y_mm = pixelSizeYmm;
    result.radius_x_px = std::max(
        1,
        static_cast<int>(std::ceil(
            config.thickness_mm / pixelSizeXmm)));
    result.radius_y_px = std::max(
        1,
        static_cast<int>(std::ceil(
            config.thickness_mm / pixelSizeYmm)));
    result.effective_thickness_x_mm =
        static_cast<double>(result.radius_x_px) * pixelSizeXmm;
    result.effective_thickness_y_mm =
        static_cast<double>(result.radius_y_px) * pixelSizeYmm;
    return result;
}

bool IsOuterVarnishOffsetWithinThickness(
    const OuterVarnishDiscretization& discretization,
    const int offsetX,
    const int offsetY) noexcept
{
    if (!discretization.enabled
        || std::abs(offsetX) > discretization.radius_x_px
        || std::abs(offsetY) > discretization.radius_y_px)
    {
        return false;
    }

    const double normalizedX =
        static_cast<double>(offsetX)
        / static_cast<double>(discretization.radius_x_px);
    const double normalizedY =
        static_cast<double>(offsetY)
        / static_cast<double>(discretization.radius_y_px);
    return normalizedX * normalizedX + normalizedY * normalizedY
        <= 1.0 + 1.0e-12;
}

}  // namespace slicer_core
