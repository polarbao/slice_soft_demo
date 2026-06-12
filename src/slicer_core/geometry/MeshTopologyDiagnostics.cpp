#include "slicer_core/geometry/MeshTopologyDiagnostics.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace slicer_core
{

MeshTopologyReport AnalyzeMeshTopology(const TriangleMeshData& mesh)
{
    MeshTopologyReport report;
    report.accepted_triangles = mesh.triangles.size();
    report.unique_vertices = mesh.vertices.size();

    std::map<std::pair<int, int>, int> edgeIncidence;
    for (const std::array<int, 3>& triangle : mesh.triangles)
    {
        const std::array<std::pair<int, int>, 3> edges{
            std::minmax(triangle.at(0), triangle.at(1)),
            std::minmax(triangle.at(1), triangle.at(2)),
            std::minmax(triangle.at(2), triangle.at(0)),
        };
        for (const auto& edge : edges)
        {
            ++edgeIncidence[edge];
        }

        const Vec3& a = mesh.vertices.at(static_cast<std::size_t>(triangle.at(0)));
        const Vec3& b = mesh.vertices.at(static_cast<std::size_t>(triangle.at(1)));
        const Vec3& c = mesh.vertices.at(static_cast<std::size_t>(triangle.at(2)));
        report.signed_volume_mm3 +=
            (a.x * (b.y * c.z - b.z * c.y)
             - a.y * (b.x * c.z - b.z * c.x)
             + a.z * (b.x * c.y - b.y * c.x))
            / 6.0;
    }

    for (const auto& [edge, incidence] : edgeIncidence)
    {
        (void)edge;
        if (incidence == 1)
        {
            ++report.boundary_edges;
        }
        else if (incidence > 2)
        {
            ++report.non_manifold_edges;
        }
    }
    return report;
}

std::string ValidateMeshTopology(const MeshTopologyReport& report, const MeshValidationPolicy policy)
{
    if (report.accepted_triangles == 0)
    {
        return "mesh has no accepted triangles";
    }
    if (policy == MeshValidationPolicy::StrictClosed)
    {
        if (report.boundary_edges > 0)
        {
            return "strict_closed rejected mesh with boundary edges";
        }
        if (report.non_manifold_edges > 0)
        {
            return "strict_closed rejected mesh with non-manifold edges";
        }
    }
    return {};
}

MeshValidationPolicy ParseMeshValidationPolicy(const std::string& value)
{
    if (value == "strict_closed")
    {
        return MeshValidationPolicy::StrictClosed;
    }
    if (value == "warn_and_attempt")
    {
        return MeshValidationPolicy::WarnAndAttempt;
    }
    throw std::runtime_error("unsupported mesh validation policy: " + value);
}

std::string MeshValidationPolicyName(const MeshValidationPolicy policy)
{
    return policy == MeshValidationPolicy::StrictClosed ? "strict_closed" : "warn_and_attempt";
}

}  // namespace slicer_core
