#include "slicer_core/materials/texture_application/SurfaceShellTextureReport.h"

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

Json BoundsToJson(const IndexBounds3D& bounds)
{
    return Json::object({
        {"minX", bounds.min_x},
        {"minY", bounds.min_y},
        {"minZ", bounds.min_z},
        {"maxX", bounds.max_x},
        {"maxY", bounds.max_y},
        {"maxZ", bounds.max_z},
    });
}

}  // namespace

Json MakeSurfaceShellTextureReport(const SurfaceShellTextureResult& result)
{
    return Json::object({
        {"schema", "p0.surface_shell_texture_report.1"},
        {"caseName", result.options.case_name},
        {"openvdb",
         Json::object({
             {"enabled", result.level_set.status.compiled_with_openvdb},
             {"available", result.level_set.status.runtime_available},
             {"version", result.level_set.status.version},
             {"gridName", result.level_set.status.grid_name},
             {"gridClass", result.level_set.status.grid_class},
             {"activeVoxels", result.level_set.active_voxels},
             {"voxelSizeMm", result.level_set.voxel_size_mm},
         })},
        {"grid",
         Json::object({
             {"width", result.shell.width},
             {"height", result.shell.height},
             {"depth", result.shell.depth},
             {"voxelSizeMm", result.options.voxel_size_mm},
             {"scanBounds", BoundsToJson(result.level_set.scan_bounds)},
             {"activeBounds", BoundsToJson(result.level_set.active_bounds)},
         })},
        {"policy",
         Json::object({
             {"mode", "surface_shell"},
             {"shellThicknessMm", result.options.shell_thickness_mm},
             {"shellRegion", "outer_surface"},
             {"fillRole", result.options.fill_role},
             {"textureSource", SurfaceShellTextureSourceName(result.options.texture_source)},
         })},
        {"stats",
         Json::object({
             {"insideVoxels", result.shell.inside_voxels},
             {"shellVoxels", result.shell.shell_voxels},
             {"interiorVoxels", result.shell.interior_voxels},
             {"outsideVoxels", result.shell.outside_voxels},
             {"coloredShellVoxels", result.colored_shell_voxels},
             {"outsideColoredVoxels", result.outside_colored_voxels},
             {"unclassifiedVoxels", result.shell.unclassified_voxels},
             {"shellPlusInteriorEqualsInside",
              result.shell.shell_voxels + result.shell.interior_voxels == result.shell.inside_voxels},
         })},
        {"warnings", Json{StringsToJsonArray(result.warnings)}},
        {"timings", Json::object({})},
    });
}

}  // namespace slicer_core
