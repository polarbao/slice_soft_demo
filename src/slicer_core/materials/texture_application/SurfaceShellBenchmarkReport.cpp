#include "slicer_core/materials/texture_application/SurfaceShellBenchmarkReport.h"

namespace slicer_core
{

Json MakeSurfaceShellBenchmarkReport(
    const SurfaceShellRealModelResult& result,
    const std::string& fixtureId,
    const std::string& buildConfig)
{
    const SurfaceTextureTransferStats& transfer = result.transfer.stats;
    return Json::object({
        {"schema", "p0.surface_shell_benchmark_report.1"},
        {"fixtureId", fixtureId},
        {"buildConfig", buildConfig},
        {"input",
         Json::object({
             {"format", result.input_format},
             {"modelPath", result.model_path},
             {"configPath", result.config_path},
         })},
        {"mesh",
         Json::object({
             {"sourceTriangles", static_cast<std::uint64_t>(result.adapted_mesh.topology.source_triangles)},
             {"acceptedTriangles", static_cast<std::uint64_t>(result.adapted_mesh.topology.accepted_triangles)},
             {"uniqueVertices", static_cast<std::uint64_t>(result.adapted_mesh.topology.unique_vertices)},
         })},
        {"grid",
         Json::object({
             {"voxelSizeMm", result.options.voxel_size_mm},
             {"shellThicknessMm", result.options.shell_thickness_mm},
             {"insideVoxels", result.shell.inside_voxels},
             {"shellVoxels", result.shell.shell_voxels},
             {"interiorVoxels", result.shell.interior_voxels},
         })},
        {"timingsMs",
         Json::object({
             {"import", result.performance.import_ms},
             {"adapter", result.performance.adapter_ms},
             {"levelSet", result.performance.level_set_ms},
             {"bvhBuild", result.performance.bvh_build_ms},
             {"transfer", result.performance.transfer_ms},
             {"preview", result.performance.preview_ms},
             {"total", result.performance.total_ms},
         })},
        {"memory",
         Json::object({
             {"peakEstimatedBytes", result.performance.peak_estimated_bytes},
             {"openVdbGridBytes", result.performance.openvdb_grid_bytes},
             {"bvhEstimatedBytes", result.performance.bvh_estimated_bytes},
             {"textureCacheBytes", result.performance.texture_cache_bytes},
             {"maskBytes", result.performance.mask_bytes},
             {"processPeakWorkingSetAvailable", result.performance.process_peak_working_set_available},
             {"processWorkingSetBytes", result.performance.process_working_set_bytes},
             {"processPeakWorkingSetBytes", result.performance.process_peak_working_set_bytes},
         })},
        {"bvh",
         Json::object({
             {"queryCount", transfer.nearest_query_stats.query_count},
             {"visitedNodes", transfer.nearest_query_stats.visited_nodes},
             {"testedTriangles", transfer.nearest_query_stats.tested_triangles},
             {"maxVisitedNodes", transfer.nearest_query_stats.max_visited_nodes},
             {"nodeCount", static_cast<std::uint64_t>(transfer.nearest_query_stats.node_count)},
         })},
    });
}

}  // namespace slicer_core
