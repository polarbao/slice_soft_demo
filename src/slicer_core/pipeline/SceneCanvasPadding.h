#pragma once
#include <cmath>
#include <limits>

namespace slicer_core
{
/** @brief Integer canvas extension retaining the original placement quantization. */
struct SceneCanvasXPadding
{
    double originaloriginxmm{0};
    int columns{0};

    bool IsValid(double paddedOrigin, double pitch, int width) const
    {
        const double expected = originaloriginxmm - columns * pitch;
        return columns > 0 && columns < width && originaloriginxmm > 0
            && std::isfinite(originaloriginxmm) && std::isfinite(expected)
            && paddedOrigin <= 0 && paddedOrigin > -pitch
            && std::abs(expected - paddedOrigin) <= pitch * 1e-7;
    }

    bool AddToOffset(int& offset) const
    {
        if (columns < 0 || offset < 0 || offset > std::numeric_limits<int>::max() - columns)
            return false;
        offset += columns;
        return true;
    }
};
}
