#include "slicer_core/materials/texture_application/SurfaceShellRealModelReport.h"

#include <map>

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

Json StringIntMapToJson(const std::map<std::string, int>& values)
{
    Json::Object object;
    for (const auto& item : values)
    {
        object.emplace(item.first, item.second);
    }
    return Json{object};
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
             {"memoryBytes", result.level_set.memory_bytes},
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
             {"nonProduction", result.non_production},
             {"fillRole", "base"},
         })},
        {"epsilon",
         Json::object({
             {"positionEpsilonMm", result.tolerance.position_epsilon_mm},
             {"areaEpsilonMm2", result.tolerance.area_epsilon_mm2},
             {"tieEpsilonMm", result.tolerance.tie_epsilon_mm},
             {"selfIntersectionEpsilonMm", result.tolerance.self_intersection_epsilon_mm},
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
        {"robustnessDiagnostics",
         Json::object({
             {"connectedComponents", static_cast<std::uint64_t>(result.robustness.connected_components)},
             {"duplicateFaces", static_cast<std::uint64_t>(result.robustness.duplicate_faces)},
             {"oppositeDuplicateFaces", static_cast<std::uint64_t>(result.robustness.opposite_duplicate_faces)},
             {"inconsistentOrientedEdges", static_cast<std::uint64_t>(result.robustness.inconsistent_oriented_edges)},
             {"selfIntersectionPairs", static_cast<std::uint64_t>(result.robustness.self_intersection_pairs)},
             {"selfIntersectionSampled", result.robustness.self_intersection_sampled},
             {"zeroVolumeComponents", static_cast<std::uint64_t>(result.robustness.zero_volume_components)},
             {"minEdgeLengthMm", result.robustness.min_edge_length_mm},
             {"maxEdgeLengthMm", result.robustness.max_edge_length_mm},
             {"minTriangleAreaMm2", result.robustness.min_triangle_area_mm2},
             {"maxTriangleAspectRatio", result.robustness.max_triangle_aspect_ratio},
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
             {"loadedTextureCount", transfer.loaded_texture_count},
             {"textureCacheHits", transfer.texture_cache_hits},
             {"textureCacheMisses", transfer.texture_cache_misses},
             {"textureCacheBytes", transfer.texture_cache_bytes},
             {"materialCount", transfer.material_count},
             {"textureCount", transfer.texture_count},
             {"perMaterialSampledVoxels", StringIntMapToJson(transfer.per_material_sampled_voxels)},
             {"perTextureSampledVoxels", StringIntMapToJson(transfer.per_texture_sampled_voxels)},
             {"nearestQueryStats",
              Json::object({
                  {"queryCount", transfer.nearest_query_stats.query_count},
                  {"visitedNodes", transfer.nearest_query_stats.visited_nodes},
                  {"testedTriangles", transfer.nearest_query_stats.tested_triangles},
                  {"maxVisitedNodes", transfer.nearest_query_stats.max_visited_nodes},
                  {"nodeCount", static_cast<std::uint64_t>(transfer.nearest_query_stats.node_count)},
                  {"estimatedBytes", static_cast<std::uint64_t>(transfer.nearest_query_stats.estimated_bytes)},
              })},
         })},
        {"performance",
         Json::object({
             {"importMs", result.performance.import_ms},
             {"adapterMs", result.performance.adapter_ms},
             {"levelSetMs", result.performance.level_set_ms},
             {"bvhBuildMs", result.performance.bvh_build_ms},
             {"transferMs", result.performance.transfer_ms},
             {"previewMs", result.performance.preview_ms},
             {"totalMs", result.performance.total_ms},
             {"peakEstimatedBytes", result.performance.peak_estimated_bytes},
         })},
        {"memory",
         Json::object({
             {"peakEstimatedBytes", result.performance.peak_estimated_bytes},
             {"meshBytes", result.performance.mesh_bytes},
             {"triangleAttributeBytes", result.performance.triangle_attribute_bytes},
             {"maskBytes", result.performance.mask_bytes},
             {"shellRgbBytes", result.performance.shell_rgb_bytes},
             {"bvhEstimatedBytes", result.performance.bvh_estimated_bytes},
             {"textureCacheBytes", result.performance.texture_cache_bytes},
             {"openVdbGridBytes", result.performance.openvdb_grid_bytes},
             {"previewBufferBytes", result.performance.preview_buffer_bytes},
             {"processPeakWorkingSetAvailable", result.performance.process_peak_working_set_available},
             {"processPeakWorkingSetBytes", result.performance.process_peak_working_set_bytes},
         })},
        {"warnings", Json{StringsToJsonArray(result.warnings)}},
        {"errors", Json{StringsToJsonArray(result.errors)}},
    });
}

}  // namespace slicer_core
