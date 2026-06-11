#pragma once

#include "slicer_core/geometry/GeometryKernelTypes.h"

namespace slicer_core
{

/**
 * @brief Build shell, interior, and boundary masks from a signed distance field.
 * @param field Signed distance field.
 * @param shellThicknessMm Shell thickness in millimeters.
 * @return Shell mask result.
 */
ShellMaskResult BuildShellMask(const DistanceField2D& field, double shellThicknessMm);

}  // namespace slicer_core
