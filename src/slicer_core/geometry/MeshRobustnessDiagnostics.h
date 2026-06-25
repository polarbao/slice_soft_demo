#pragma once

#include "slicer_core/geometry/MeshScaleTolerance.h"
#include "slicer_core/geometry/TriangleMeshData.h"

#include <cstddef>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Robustness diagnostics beyond closed-edge topology.
 */
struct MeshRobustnessReport
{
    std::size_t connected_components{0};
    std::size_t duplicate_faces{0};
    std::size_t opposite_duplicate_faces{0};
    std::size_t inconsistent_oriented_edges{0};
    std::size_t self_intersection_pairs{0};
    bool self_intersection_sampled{false};
    std::size_t zero_volume_components{0};
    double min_edge_length_mm{0.0};
    double max_edge_length_mm{0.0};
    double min_triangle_area_mm2{0.0};
    double max_triangle_aspect_ratio{0.0};
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

/**
 * @brief Options for robustness diagnostics.
 */
struct MeshRobustnessOptions
{
    MeshScaleTolerance tolerance;
    std::size_t max_self_intersection_pairs{128};
    std::size_t max_triangle_pair_checks{250000};
};

/**
 * @brief Analyze duplicate, winding, component, scale and intersection diagnostics.
 * @param mesh Indexed mesh.
 * @param options Diagnostic options.
 * @return Robustness report.
 */
MeshRobustnessReport AnalyzeMeshRobustness(
    const TriangleMeshData& mesh,
    const MeshRobustnessOptions& options);

/**
 * @brief Validate robustness diagnostics for strict experimental policies.
 * @param report Robustness report.
 * @param rejectSelfIntersection Whether self-intersections should reject the mesh.
 * @return Empty string when accepted; otherwise an error message.
 */
std::string ValidateMeshRobustness(const MeshRobustnessReport& report, bool rejectSelfIntersection);

}  // namespace slicer_core
