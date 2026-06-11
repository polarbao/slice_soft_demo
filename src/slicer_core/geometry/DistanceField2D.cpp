#include "slicer_core/geometry/DistanceField2D.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace slicer_core
{
namespace
{

std::size_t MaskIndex(const int width, const int x, const int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

bool IsInside(const BinaryMask2D& mask, const int x, const int y)
{
    return mask.inside.at(MaskIndex(mask.width, x, y)) != 0;
}

bool IsBoundaryPixel(const BinaryMask2D& mask, const int x, const int y)
{
    const bool currentInside = IsInside(mask, x, y);
    constexpr int offsets[4][2]{{0, -1}, {-1, 0}, {1, 0}, {0, 1}};
    for (const auto& offset : offsets)
    {
        const int nx{x + offset[0]};
        const int ny{y + offset[1]};
        if (nx < 0 || nx >= mask.width || ny < 0 || ny >= mask.height)
        {
            return true;
        }
        if (IsInside(mask, nx, ny) != currentInside)
        {
            return true;
        }
    }
    return false;
}

}  // namespace

DistanceField2D BuildDistanceField2D(const BinaryMask2D& mask)
{
    if (mask.width <= 0 || mask.height <= 0)
    {
        throw std::runtime_error("BinaryMask2D width and height must be positive");
    }
    const std::size_t expectedSize = static_cast<std::size_t>(mask.width) * static_cast<std::size_t>(mask.height);
    if (mask.inside.size() != expectedSize)
    {
        throw std::runtime_error("BinaryMask2D inside size does not match dimensions");
    }
    if (mask.pixel_size_mm <= 0.0)
    {
        throw std::runtime_error("BinaryMask2D pixel_size_mm must be positive");
    }

    std::vector<std::pair<int, int>> boundaryPixels;
    for (int y{0}; y < mask.height; ++y)
    {
        for (int x{0}; x < mask.width; ++x)
        {
            if (IsBoundaryPixel(mask, x, y))
            {
                boundaryPixels.emplace_back(x, y);
            }
        }
    }

    DistanceField2D field;
    field.width = mask.width;
    field.height = mask.height;
    field.pixel_size_mm = mask.pixel_size_mm;
    field.distance_mm.resize(expectedSize, 0.0F);

    for (int y{0}; y < mask.height; ++y)
    {
        for (int x{0}; x < mask.width; ++x)
        {
            double minDistanceSquared = std::numeric_limits<double>::max();
            for (const auto& boundaryPixel : boundaryPixels)
            {
                const double dx = static_cast<double>(x - boundaryPixel.first);
                const double dy = static_cast<double>(y - boundaryPixel.second);
                minDistanceSquared = std::min(minDistanceSquared, dx * dx + dy * dy);
            }
            const double distance = std::sqrt(minDistanceSquared) * mask.pixel_size_mm;
            const float signedDistance =
                static_cast<float>(IsInside(mask, x, y) ? -distance : distance);
            field.distance_mm.at(MaskIndex(mask.width, x, y)) = signedDistance;
        }
    }

    return field;
}

DistanceFieldStats ComputeDistanceFieldStats(const DistanceField2D& field)
{
    if (field.distance_mm.empty())
    {
        return DistanceFieldStats{};
    }

    DistanceFieldStats stats;
    stats.min_distance_mm = field.distance_mm.front();
    stats.max_distance_mm = field.distance_mm.front();
    for (const float value : field.distance_mm)
    {
        stats.min_distance_mm = std::min(stats.min_distance_mm, value);
        stats.max_distance_mm = std::max(stats.max_distance_mm, value);
        if (value < 0.0F)
        {
            ++stats.negative_pixels;
        }
        else if (value > 0.0F)
        {
            ++stats.positive_pixels;
        }
        else
        {
            ++stats.zero_pixels;
        }
    }
    return stats;
}

}  // namespace slicer_core
