#pragma once

#include "slicer_core/geometry/TriangleMeshData.h"

#include <cstddef>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Mesh validation policy for the real-model shell prototype.
 */
enum class MeshValidationPolicy
{
    StrictClosed,
    WarnAndAttempt
};

/**
 * @brief Topology and orientation diagnostics for an indexed triangle mesh.
 */
struct MeshTopologyReport
{
    std::size_t source_triangles{0};
    std::size_t accepted_triangles{0};
    std::size_t degenerate_triangles{0};
    std::size_t unique_vertices{0};
    std::size_t boundary_edges{0};
    std::size_t non_manifold_edges{0};
    double signed_volume_mm3{0.0};
    bool orientation_flipped{false};
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

/**
 * @brief Analyze topology and signed volume for an indexed mesh.
 * @param mesh Indexed mesh.
 * @return Topology report.
 */
MeshTopologyReport AnalyzeMeshTopology(const TriangleMeshData& mesh);

/**
 * @brief Validate a topology report against the selected policy.
 * @param report Topology report.
 * @param policy Validation policy.
 * @return Empty string when accepted; otherwise an error message.
 */
std::string ValidateMeshTopology(const MeshTopologyReport& report, MeshValidationPolicy policy);

/**
 * @brief Parse a mesh validation policy name.
 * @param value Policy name.
 * @return Parsed policy.
 */
MeshValidationPolicy ParseMeshValidationPolicy(const std::string& value);

/**
 * @brief Return the stable policy name.
 * @param policy Policy value.
 * @return Policy name.
 */
std::string MeshValidationPolicyName(MeshValidationPolicy policy);

}  // namespace slicer_core
