#include "slicer_core/geometry/OpenVdbAdapter.h"

#ifdef SLICER_CORE_USE_OPENVDB
#include <openvdb/openvdb.h>
#endif

#include <string>

namespace slicer_core
{

OpenVdbStatus GetOpenVdbStatus()
{
    OpenVdbStatus status;
#ifdef SLICER_CORE_USE_OPENVDB
    status.compiled_with_openvdb = true;
    status.runtime_available = true;
    status.version =
        std::to_string(OPENVDB_LIBRARY_MAJOR_VERSION_NUMBER) + "."
        + std::to_string(OPENVDB_LIBRARY_MINOR_VERSION_NUMBER) + "."
        + std::to_string(OPENVDB_LIBRARY_PATCH_VERSION_NUMBER);
#else
    status.compiled_with_openvdb = false;
    status.runtime_available = false;
    status.warnings.push_back("USE_OPENVDB=OFF; OpenVDB adapter is running in stub mode");
#endif
    return status;
}

OpenVdbSmokeResult RunOpenVdbSmokeCase()
{
    OpenVdbSmokeResult result;
    result.status = GetOpenVdbStatus();
#ifdef SLICER_CORE_USE_OPENVDB
    openvdb::initialize();
    openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(0.0F);
    openvdb::FloatGrid::Accessor accessor = grid->getAccessor();
    for (int z{-1}; z <= 1; ++z)
    {
        for (int y{-1}; y <= 1; ++y)
        {
            for (int x{-1}; x <= 1; ++x)
            {
                accessor.setValue(openvdb::Coord{x, y, z}, 1.0F);
            }
        }
    }
    result.executed = true;
    result.skipped = false;
    result.active_voxels = static_cast<int>(grid->activeVoxelCount());
#else
    result.executed = false;
    result.skipped = true;
    result.warnings.push_back("OpenVDB smoke case skipped because USE_OPENVDB=OFF");
#endif
    return result;
}

}  // namespace slicer_core
