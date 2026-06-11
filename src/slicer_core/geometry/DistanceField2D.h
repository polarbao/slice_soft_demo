#pragma once

#include "slicer_core/geometry/GeometryKernelTypes.h"

namespace slicer_core
{

/**
 * @brief Build a signed 2D distance field from a binary mask.
 * @param mask Binary mask where non-zero means inside.
 * @return Signed distance field; inside pixels are negative and outside pixels are positive.
 */
DistanceField2D BuildDistanceField2D(const BinaryMask2D& mask);

/**
 * @brief Compute distance field summary statistics.
 * @param field Distance field.
 * @return Distance statistics.
 */
DistanceFieldStats ComputeDistanceFieldStats(const DistanceField2D& field);

}  // namespace slicer_core
