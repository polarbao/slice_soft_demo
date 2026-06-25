#include "slicer_core/geometry/MeshScaleTolerance.h"

#include <algorithm>
#include <cmath>

namespace slicer_core
{

MeshScaleTolerance MakeMeshScaleTolerance(const BoundingBox& bboxMm, const double voxelSizeMm)
{
    const double extentX = bboxMm.max.x - bboxMm.min.x;
    const double extentY = bboxMm.max.y - bboxMm.min.y;
    const double extentZ = bboxMm.max.z - bboxMm.min.z;
    const double diagonalMm = std::sqrt(extentX * extentX + extentY * extentY + extentZ * extentZ);
    const double scaleMm = std::max({diagonalMm, std::abs(voxelSizeMm), 1.0});

    MeshScaleTolerance tolerance;
    tolerance.position_epsilon_mm = std::clamp(scaleMm * 1.0e-8, 1.0e-7, 1.0e-4);
    tolerance.area_epsilon_mm2 = tolerance.position_epsilon_mm * tolerance.position_epsilon_mm * 4.0;
    tolerance.tie_epsilon_mm = std::clamp(std::abs(voxelSizeMm) * 1.0e-4, 1.0e-7, 1.0e-3);
    tolerance.self_intersection_epsilon_mm = std::max(tolerance.position_epsilon_mm * 2.0, 1.0e-6);
    return tolerance;
}

}  // namespace slicer_core
