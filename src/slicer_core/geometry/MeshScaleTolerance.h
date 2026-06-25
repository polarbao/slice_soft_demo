#pragma once

#include "slicer_core/model.h"

namespace slicer_core
{

/**
 * @brief Scale-aware tolerances for experimental mesh robustness checks.
 */
struct MeshScaleTolerance
{
    double position_epsilon_mm{1.0e-6};
    double area_epsilon_mm2{1.0e-12};
    double tie_epsilon_mm{1.0e-7};
    double self_intersection_epsilon_mm{1.0e-6};
};

/**
 * @brief Create scale-aware tolerances from model bounds and voxel size.
 * @param bboxMm Model bounding box in millimeters.
 * @param voxelSizeMm Voxel size in millimeters.
 * @return Derived tolerance values.
 */
MeshScaleTolerance MakeMeshScaleTolerance(const BoundingBox& bboxMm, double voxelSizeMm);

}  // namespace slicer_core
