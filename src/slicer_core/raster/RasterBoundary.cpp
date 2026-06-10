#include "slicer_core/raster/RasterBoundary.h"

namespace slicer_core
{

RasterBoundary LegacyRasterBoundary()
{
    return RasterBoundary{{"closed_mesh_scanline", "relief_heightfield"}};
}

}  // namespace slicer_core
