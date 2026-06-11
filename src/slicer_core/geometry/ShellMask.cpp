#include "slicer_core/geometry/ShellMask.h"

#include <cmath>
#include <stdexcept>

namespace slicer_core
{

ShellMaskResult BuildShellMask(const DistanceField2D& field, const double shellThicknessMm)
{
    if (field.width <= 0 || field.height <= 0)
    {
        throw std::runtime_error("DistanceField2D width and height must be positive");
    }
    if (shellThicknessMm <= 0.0)
    {
        throw std::runtime_error("shellThicknessMm must be positive");
    }

    const std::size_t pixelCount = static_cast<std::size_t>(field.width) * static_cast<std::size_t>(field.height);
    if (field.distance_mm.size() != pixelCount)
    {
        throw std::runtime_error("DistanceField2D distance size does not match dimensions");
    }

    ShellMaskResult result;
    result.width = field.width;
    result.height = field.height;
    result.pixel_size_mm = field.pixel_size_mm;
    result.shell_thickness_mm = shellThicknessMm;
    result.shell_mask.resize(pixelCount, 0);
    result.interior_mask.resize(pixelCount, 0);
    result.boundary_mask.resize(pixelCount, 0);

    const double boundaryEpsilon = std::max(field.pixel_size_mm * 0.5, 0.000001);
    for (std::size_t i{0}; i < pixelCount; ++i)
    {
        const double distance = static_cast<double>(field.distance_mm.at(i));
        const double absoluteDistance = std::abs(distance);
        if (distance <= 0.0 && absoluteDistance <= shellThicknessMm)
        {
            result.shell_mask.at(i) = 1;
            ++result.shell_pixels;
        }
        if (distance < -shellThicknessMm)
        {
            result.interior_mask.at(i) = 1;
            ++result.interior_pixels;
        }
        if (absoluteDistance <= boundaryEpsilon)
        {
            result.boundary_mask.at(i) = 1;
            ++result.boundary_pixels;
        }
    }

    return result;
}

}  // namespace slicer_core
