#pragma once

#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Boundary description for available raster modes.
 */
struct RasterBoundary
{
    std::vector<std::string> modes;
};

/**
 * @brief Return the current legacy raster mode boundary.
 * @return Raster boundary containing supported legacy mode names.
 */
RasterBoundary LegacyRasterBoundary();

}  // namespace slicer_core
