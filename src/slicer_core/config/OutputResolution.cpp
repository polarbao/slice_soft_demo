#include "slicer_core/config/OutputResolution.h"

#include <cmath>

namespace slicer_core {

bool IsSupportedOutputDpi(const int dpi) noexcept
{
    return dpi >= kMinimumOutputDpi && dpi <= kMaximumOutputDpi;
}

bool IsOutputPixelSizeConsistent(
    const int dpi,
    const double pixelSizeMm) noexcept
{
    if (!IsSupportedOutputDpi(dpi)
        || !std::isfinite(pixelSizeMm)
        || pixelSizeMm <= 0.0)
    {
        return false;
    }
    const double expectedPixelSizeMm =
        kMillimetersPerInch / static_cast<double>(dpi);
    return std::abs(pixelSizeMm - expectedPixelSizeMm)
        <= kOutputPixelSizeToleranceMm;
}

}  // namespace slicer_core
