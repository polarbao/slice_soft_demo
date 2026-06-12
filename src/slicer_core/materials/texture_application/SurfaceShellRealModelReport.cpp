#include "slicer_core/materials/texture_application/SurfaceShellRealModelReport.h"

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

Json MakeSurfaceShellRealModelReport(const SurfaceShellRealModelResult& result)
{
    const MeshTopologyReport& topology = result.adapted_mesh.topology;
    const SurfaceTextureTransferStats& transfer = result.transfer.stats;
    return Json::object({
        {"schema", "p0.surface_shell_texture_report.2"},
        {"caseName", result.case_name},
        {"input",
         Json::object({
             {"format", result.input_format},
             {"modelPath", result.model_path},
             {"configPath", result.config_path},
         })},
        {"openvdb",
         Json::object({
             {"enabled", result.level_set.status.compiled_with_openvdb},
             {"available", result.level_set.status.runtime_available},
             {"version", result.level_set.status.version},
             {"gridName", result.level_set.status.grid_name},
             {"gridClass", result.level_set.status.grid_class},
             {"activeVoxels", result.level_set.active_voxels},
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
             {"meshPolicy", MeshValidationPolicyName(result.options.mesh_policy)},
             {"maxTransferDistanceMm", result.options.max_transfer_distance_mm},
             {"fillRole", "base"},
         })},
        {"meshDiagnostics",
         Json::object({
             {"sourceTriangles", static_cast<std::uint64_t>(topology.source_triangles)},
             {"acceptedTriangles", static_cast<std::uint64_t>(topology.accepted_triangles)},
             {"degenerateTriangles", static_cast<std::uint64_t>(topology.degenerate_triangles)},
             {"uniqueVertices", static_cast<std::uint64_t>(topology.unique_vertices)},
             {"boundaryEdges", static_cast<std::uint64_t>(topology.boundary_edges)},
             {"nonManifoldEdges", static_cast<std::uint64_t>(topology.non_manifold_edges)},
             {"signedVolumeMm3", topology.signed_volume_mm3},
             {"orientationFlipped", topology.orientation_flipped},
         })},
        {"stats",
         Json::object({
             {"insideVoxels", result.shell.inside_voxels},
             {"shellVoxels", result.shell.shell_voxels},
             {"interiorVoxels", result.shell.interior_voxels},
             {"outsideVoxels", result.shell.outside_voxels},
             {"outsideColoredVoxels", result.transfer.outside_colored_voxels},
             {"unclassifiedVoxels", result.shell.unclassified_voxels},
             {"shellPlusInteriorEqualsInside",
              result.shell.shell_voxels + result.shell.interior_voxels == result.shell.inside_voxels},
         })},
        {"transferStats",
         Json::object({
             {"sampledTextureVoxels", transfer.sampled_texture_voxels},
             {"materialDiffuseVoxels", transfer.material_diffuse_voxels},
             {"fallbackVoxels", transfer.fallback_voxels},
             {"missingUvVoxels", transfer.missing_uv_voxels},
             {"missingTextureVoxels", transfer.missing_texture_voxels},
             {"uvOutOfRangeVoxels", transfer.uv_out_of_range_voxels},
             {"transferDistanceExceededVoxels", transfer.transfer_distance_exceeded_voxels},
             {"queryFailedVoxels", transfer.query_failed_voxels},
             {"maxObservedDistanceMm", transfer.max_observed_distance_mm},
             {"uniqueColorCount", transfer.unique_color_count},
         })},
        {"performance",
         Json::object({
             {"importMs", result.performance.import_ms},
             {"adapterMs", result.performance.adapter_ms},
             {"levelSetMs", result.performance.level_set_ms},
             {"bvhBuildMs", result.performance.bvh_build_ms},
             {"transferMs", result.performance.transfer_ms},
             {"peakEstimatedBytes", result.performance.peak_estimated_bytes},
         })},
        {"warnings", Json{StringsToJsonArray(result.warnings)}},
        {"errors", Json{StringsToJsonArray(result.errors)}},
    });
}

}  // namespace slicer_core
