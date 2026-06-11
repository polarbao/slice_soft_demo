#pragma once

#include "slicer_core/geometry/DistanceField2D.h"
#include "slicer_core/geometry/ShellMask.h"
#include "slicer_core/json_value.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Build geometry kernel report JSON.
 * @param caseName Demo case name.
 * @param openVdbStatus OpenVDB dependency status.
 * @param mask Input binary mask.
 * @param distanceStats Distance field statistics.
 * @param shellResult Shell mask result.
 * @param warnings Case warnings.
 * @return Geometry kernel report JSON.
 */
Json MakeGeometryKernelReport(
    const std::string& caseName,
    const OpenVdbStatus& openVdbStatus,
    const BinaryMask2D& mask,
    const DistanceFieldStats& distanceStats,
    const ShellMaskResult& shellResult,
    const std::vector<std::string>& warnings);

}  // namespace slicer_core
