#include "slicer_core/geometry/GeometryKernelReport.h"

namespace slicer_core
{
namespace
{

Json::Array StringsToJsonArray(const std::vector<std::string>& values)
{
    Json::Array result;
    for (const std::string& value : values)
    {
        result.push_back(value);
    }
    return result;
}

}  // namespace

Json MakeGeometryKernelReport(
    const std::string& caseName,
    const OpenVdbStatus& openVdbStatus,
    const BinaryMask2D& mask,
    const DistanceFieldStats& distanceStats,
    const ShellMaskResult& shellResult,
    const std::vector<std::string>& warnings)
{
    Json::Array allWarnings = StringsToJsonArray(warnings);
    for (const std::string& warning : openVdbStatus.warnings)
    {
        allWarnings.push_back(warning);
    }

    return Json::object({
        {"schema", "p0.geometry_kernel_report.1"},
        {"caseName", caseName},
        {"openvdb",
         Json::object({
             {"enabled", openVdbStatus.compiled_with_openvdb},
             {"available", openVdbStatus.runtime_available},
             {"version", openVdbStatus.version},
         })},
        {"grid",
         Json::object({
             {"widthPx", mask.width},
             {"heightPx", mask.height},
             {"pixelSizeMm", mask.pixel_size_mm},
         })},
        {"distanceStats",
         Json::object({
             {"minDistanceMm", static_cast<double>(distanceStats.min_distance_mm)},
             {"maxDistanceMm", static_cast<double>(distanceStats.max_distance_mm)},
             {"negativePixels", distanceStats.negative_pixels},
             {"positivePixels", distanceStats.positive_pixels},
             {"zeroPixels", distanceStats.zero_pixels},
         })},
        {"shellStats",
         Json::object({
             {"shellThicknessMm", shellResult.shell_thickness_mm},
             {"shellPixels", shellResult.shell_pixels},
             {"interiorPixels", shellResult.interior_pixels},
             {"boundaryPixels", shellResult.boundary_pixels},
         })},
        {"warnings", Json{allWarnings}},
        {"timings", Json::object({})},
    });
}

}  // namespace slicer_core
