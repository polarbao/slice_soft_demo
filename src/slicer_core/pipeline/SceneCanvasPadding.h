#pragma once
#include <cmath>
#include <limits>

namespace slicer_core
{
/** @brief Integer canvas extension retaining the original placement quantization. */
struct SceneCanvasAxisPadding
{
    double originaloriginmm{0};
    int pixels{0};

    bool IsValid(double paddedOrigin, double pitch, int width) const
    {
        const double expected = originaloriginmm - pixels * pitch;
        return pixels > 0 && pixels < width && originaloriginmm > 0
            && std::isfinite(originaloriginmm) && std::isfinite(expected)
            && std::isfinite(pitch) && pitch > 0
            && paddedOrigin <= 0 && paddedOrigin > -pitch
            && std::abs(expected - paddedOrigin) <= pitch * 1e-7;
    }

    bool AddToOffset(int& offset) const
    {
        if (pixels < 0 || offset < 0 || offset > std::numeric_limits<int>::max() - pixels)
            return false;
        offset += pixels;
        return true;
    }
};
}
