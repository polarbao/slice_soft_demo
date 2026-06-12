#include "slicer_core/geometry/OpenVdbSurfaceShell.h"

#ifdef SLICER_CORE_USE_OPENVDB
#include <openvdb/openvdb.h>
#endif

#include <cmath>

namespace slicer_core
{

std::size_t MaskIndex3D(const int width, const int height, const int x, const int y, const int z)
{
    return (static_cast<std::size_t>(z) * static_cast<std::size_t>(height) * static_cast<std::size_t>(width))
        + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width))
        + static_cast<std::size_t>(x);
}

OpenVdbSurfaceShellResult ClassifyOpenVdbSurfaceShell(
    const OpenVdbLevelSetResult& levelSet,
    const OpenVdbSurfaceShellOptions& options)
{
    OpenVdbSurfaceShellResult result;
    result.shell_thickness_mm = options.shell_thickness_mm;
    result.bounds = levelSet.scan_bounds;

    if (!(options.shell_thickness_mm > 0.0) || !std::isfinite(options.shell_thickness_mm))
    {
        result.error = "shell thickness must be positive";
        return result;
    }
    if (!levelSet.available || !levelSet.generated || levelSet.grid == nullptr)
    {
        result.error = levelSet.error.empty() ? "OpenVDB level set unavailable" : levelSet.error;
        result.warnings = levelSet.warnings;
        return result;
    }

    result.width = result.bounds.max_x - result.bounds.min_x + 1;
    result.height = result.bounds.max_y - result.bounds.min_y + 1;
    result.depth = result.bounds.max_z - result.bounds.min_z + 1;
    if (result.width <= 0 || result.height <= 0 || result.depth <= 0)
    {
        result.error = "OpenVDB scan bounds are invalid";
        return result;
    }

    const std::size_t voxelCount =
        static_cast<std::size_t>(result.width) * static_cast<std::size_t>(result.height) * static_cast<std::size_t>(result.depth);
    result.inside_mask.assign(voxelCount, 0);
    result.shell_mask.assign(voxelCount, 0);
    result.interior_mask.assign(voxelCount, 0);

#ifdef SLICER_CORE_USE_OPENVDB
    const std::shared_ptr<openvdb::FloatGrid> grid = std::static_pointer_cast<openvdb::FloatGrid>(levelSet.grid);
    openvdb::FloatGrid::ConstAccessor accessor = grid->getConstAccessor();

    for (int z{result.bounds.min_z}; z <= result.bounds.max_z; ++z)
    {
        for (int y{result.bounds.min_y}; y <= result.bounds.max_y; ++y)
        {
            for (int x{result.bounds.min_x}; x <= result.bounds.max_x; ++x)
            {
                const int localX = x - result.bounds.min_x;
                const int localY = y - result.bounds.min_y;
                const int localZ = z - result.bounds.min_z;
                const std::size_t index = MaskIndex3D(result.width, result.height, localX, localY, localZ);
                const float phi = accessor.getValue(openvdb::Coord{x, y, z});
                if (phi < 0.0F)
                {
                    result.inside_mask.at(index) = 1;
                    ++result.inside_voxels;
                    if (static_cast<double>(phi) >= -options.shell_thickness_mm)
                    {
                        result.shell_mask.at(index) = 1;
                        ++result.shell_voxels;
                    }
                    else
                    {
                        result.interior_mask.at(index) = 1;
                        ++result.interior_voxels;
                    }
                }
                else
                {
                    ++result.outside_voxels;
                }
            }
        }
    }
#else
    result.error = "USE_OPENVDB=OFF; surface shell classifier unavailable";
    result.warnings.push_back(result.error);
#endif

    if (result.shell_voxels + result.interior_voxels != result.inside_voxels)
    {
        result.unclassified_voxels =
            result.inside_voxels - result.shell_voxels - result.interior_voxels;
        result.warnings.push_back("shell + interior does not equal inside");
    }
    return result;
}

}  // namespace slicer_core
