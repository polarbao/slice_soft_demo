#include "slicer_core/geometry/MeshRobustnessDiagnostics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

struct FaceKey
{
    std::array<int, 3> vertices{};

    bool operator<(const FaceKey& other) const
    {
        return vertices < other.vertices;
    }
};

struct DirectedEdgeUse
{
    int from{0};
    int to{0};
};

double Distance(const Vec3& a, const Vec3& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Vec3 Subtract(const Vec3& a, const Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

double Dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Cross(const Vec3& a, const Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

double TriangleArea(const TriangleMeshData& mesh, const std::array<int, 3>& triangle)
{
    const Vec3& a = mesh.vertices.at(static_cast<std::size_t>(triangle.at(0)));
    const Vec3& b = mesh.vertices.at(static_cast<std::size_t>(triangle.at(1)));
    const Vec3& c = mesh.vertices.at(static_cast<std::size_t>(triangle.at(2)));
    const Vec3 cross = Cross(Subtract(b, a), Subtract(c, a));
    return 0.5 * std::sqrt(Dot(cross, cross));
}

double SignedVolume(const TriangleMeshData& mesh, const std::vector<std::size_t>& triangles)
{
    double volume{0.0};
    for (const std::size_t triangleIndex : triangles)
    {
        const std::array<int, 3>& triangle = mesh.triangles.at(triangleIndex);
        const Vec3& a = mesh.vertices.at(static_cast<std::size_t>(triangle.at(0)));
        const Vec3& b = mesh.vertices.at(static_cast<std::size_t>(triangle.at(1)));
        const Vec3& c = mesh.vertices.at(static_cast<std::size_t>(triangle.at(2)));
        volume +=
            (a.x * (b.y * c.z - b.z * c.y)
             - a.y * (b.x * c.z - b.z * c.x)
             + a.z * (b.x * c.y - b.y * c.x))
            / 6.0;
    }
    return volume;
}

FaceKey MakeFaceKey(std::array<int, 3> triangle)
{
    std::sort(triangle.begin(), triangle.end());
    return {triangle};
}

bool IsOppositeOrientation(const std::array<int, 3>& a, const std::array<int, 3>& b)
{
    return (a.at(0) == b.at(0) && a.at(1) == b.at(2) && a.at(2) == b.at(1))
        || (a.at(0) == b.at(1) && a.at(1) == b.at(0) && a.at(2) == b.at(2))
        || (a.at(0) == b.at(2) && a.at(1) == b.at(1) && a.at(2) == b.at(0));
}

std::array<std::pair<int, int>, 3> DirectedEdges(const std::array<int, 3>& triangle)
{
    return {
        std::make_pair(triangle.at(0), triangle.at(1)),
        std::make_pair(triangle.at(1), triangle.at(2)),
        std::make_pair(triangle.at(2), triangle.at(0)),
    };
}

std::pair<int, int> UndirectedEdge(const int a, const int b)
{
    return std::minmax(a, b);
}

bool SharesVertex(const std::array<int, 3>& a, const std::array<int, 3>& b)
{
    for (const int left : a)
    {
        for (const int right : b)
        {
            if (left == right)
            {
                return true;
            }
        }
    }
    return false;
}

struct Aabb
{
    Vec3 min{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
    Vec3 max{-std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()};
};

void Expand(Aabb& bounds, const Vec3& point)
{
    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.min.z = std::min(bounds.min.z, point.z);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
    bounds.max.z = std::max(bounds.max.z, point.z);
}

Aabb TriangleBounds(const TriangleMeshData& mesh, const std::array<int, 3>& triangle, const double padding)
{
    Aabb bounds;
    for (const int vertexIndex : triangle)
    {
        const Vec3& vertex = mesh.vertices.at(static_cast<std::size_t>(vertexIndex));
        Expand(bounds, vertex);
    }
    bounds.min.x -= padding;
    bounds.min.y -= padding;
    bounds.min.z -= padding;
    bounds.max.x += padding;
    bounds.max.y += padding;
    bounds.max.z += padding;
    return bounds;
}

bool AabbOverlap(const Aabb& a, const Aabb& b)
{
    return a.min.x <= b.max.x && a.max.x >= b.min.x
        && a.min.y <= b.max.y && a.max.y >= b.min.y
        && a.min.z <= b.max.z && a.max.z >= b.min.z;
}

std::vector<std::size_t> BuildComponents(
    const TriangleMeshData& mesh,
    std::vector<std::vector<std::size_t>>& componentTriangles)
{
    std::map<std::pair<int, int>, std::vector<std::size_t>> trianglesByEdge;
    for (std::size_t triangleIndex{0}; triangleIndex < mesh.triangles.size(); ++triangleIndex)
    {
        for (const auto& edge : DirectedEdges(mesh.triangles.at(triangleIndex)))
        {
            trianglesByEdge[UndirectedEdge(edge.first, edge.second)].push_back(triangleIndex);
        }
    }

    std::vector<std::vector<std::size_t>> adjacency(mesh.triangles.size());
    for (const auto& item : trianglesByEdge)
    {
        const std::vector<std::size_t>& owners = item.second;
        for (std::size_t left{0}; left < owners.size(); ++left)
        {
            for (std::size_t right{left + 1U}; right < owners.size(); ++right)
            {
                adjacency.at(owners.at(left)).push_back(owners.at(right));
                adjacency.at(owners.at(right)).push_back(owners.at(left));
            }
        }
    }

    std::vector<std::size_t> componentByTriangle(mesh.triangles.size(), static_cast<std::size_t>(-1));
    for (std::size_t seed{0}; seed < mesh.triangles.size(); ++seed)
    {
        if (componentByTriangle.at(seed) != static_cast<std::size_t>(-1))
        {
            continue;
        }
        const std::size_t componentIndex = componentTriangles.size();
        componentTriangles.push_back({});
        std::vector<std::size_t> stack{seed};
        componentByTriangle.at(seed) = componentIndex;
        while (!stack.empty())
        {
            const std::size_t current = stack.back();
            stack.pop_back();
            componentTriangles.back().push_back(current);
            for (const std::size_t next : adjacency.at(current))
            {
                if (componentByTriangle.at(next) == static_cast<std::size_t>(-1))
                {
                    componentByTriangle.at(next) = componentIndex;
                    stack.push_back(next);
                }
            }
        }
    }
    return componentByTriangle;
}

}  // namespace

MeshRobustnessReport AnalyzeMeshRobustness(
    const TriangleMeshData& mesh,
    const MeshRobustnessOptions& options)
{
    MeshRobustnessReport report;
    if (mesh.triangles.empty())
    {
        return report;
    }

    report.min_edge_length_mm = std::numeric_limits<double>::max();
    report.min_triangle_area_mm2 = std::numeric_limits<double>::max();

    std::map<FaceKey, std::array<int, 3>> firstFaceByKey;
    std::map<std::pair<int, int>, std::vector<DirectedEdgeUse>> edgeUses;
    for (const std::array<int, 3>& triangle : mesh.triangles)
    {
        const FaceKey key = MakeFaceKey(triangle);
        const auto foundFace = firstFaceByKey.find(key);
        if (foundFace == firstFaceByKey.end())
        {
            firstFaceByKey.emplace(key, triangle);
        }
        else
        {
            ++report.duplicate_faces;
            if (IsOppositeOrientation(foundFace->second, triangle))
            {
                ++report.opposite_duplicate_faces;
            }
        }

        for (const auto& edge : DirectedEdges(triangle))
        {
            edgeUses[UndirectedEdge(edge.first, edge.second)].push_back({edge.first, edge.second});
            const double length = Distance(
                mesh.vertices.at(static_cast<std::size_t>(edge.first)),
                mesh.vertices.at(static_cast<std::size_t>(edge.second)));
            report.min_edge_length_mm = std::min(report.min_edge_length_mm, length);
            report.max_edge_length_mm = std::max(report.max_edge_length_mm, length);
        }

        const double area = TriangleArea(mesh, triangle);
        report.min_triangle_area_mm2 = std::min(report.min_triangle_area_mm2, area);
        const double a = Distance(mesh.vertices.at(static_cast<std::size_t>(triangle.at(0))), mesh.vertices.at(static_cast<std::size_t>(triangle.at(1))));
        const double b = Distance(mesh.vertices.at(static_cast<std::size_t>(triangle.at(1))), mesh.vertices.at(static_cast<std::size_t>(triangle.at(2))));
        const double c = Distance(mesh.vertices.at(static_cast<std::size_t>(triangle.at(2))), mesh.vertices.at(static_cast<std::size_t>(triangle.at(0))));
        const double longest = std::max({a, b, c});
        const double altitude = area > 0.0 ? 2.0 * area / longest : 0.0;
        if (altitude > 0.0)
        {
            report.max_triangle_aspect_ratio = std::max(report.max_triangle_aspect_ratio, longest / altitude);
        }
    }

    for (const auto& item : edgeUses)
    {
        const std::vector<DirectedEdgeUse>& uses = item.second;
        if (uses.size() == 2U)
        {
            if (uses.at(0).from == uses.at(1).from && uses.at(0).to == uses.at(1).to)
            {
                ++report.inconsistent_oriented_edges;
            }
        }
    }

    std::vector<std::vector<std::size_t>> componentTriangles;
    (void)BuildComponents(mesh, componentTriangles);
    report.connected_components = componentTriangles.size();
    for (const std::vector<std::size_t>& component : componentTriangles)
    {
        if (std::abs(SignedVolume(mesh, component)) <= options.tolerance.area_epsilon_mm2)
        {
            ++report.zero_volume_components;
        }
    }

    const std::size_t triangleCount = mesh.triangles.size();
    std::vector<Aabb> bounds;
    bounds.reserve(triangleCount);
    for (const std::array<int, 3>& triangle : mesh.triangles)
    {
        bounds.push_back(TriangleBounds(mesh, triangle, options.tolerance.self_intersection_epsilon_mm));
    }
    std::size_t checkedPairs{0};
    for (std::size_t left{0}; left < triangleCount; ++left)
    {
        for (std::size_t right{left + 1U}; right < triangleCount; ++right)
        {
            if (++checkedPairs > options.max_triangle_pair_checks)
            {
                report.self_intersection_sampled = true;
                break;
            }
            if (SharesVertex(mesh.triangles.at(left), mesh.triangles.at(right)))
            {
                continue;
            }
            if (AabbOverlap(bounds.at(left), bounds.at(right)))
            {
                ++report.self_intersection_pairs;
                if (report.self_intersection_pairs >= options.max_self_intersection_pairs)
                {
                    report.self_intersection_sampled = true;
                    break;
                }
            }
        }
        if (report.self_intersection_sampled)
        {
            break;
        }
    }

    if (report.min_edge_length_mm <= options.tolerance.position_epsilon_mm * 4.0)
    {
        report.warnings.push_back("thin feature warning: edge length near tolerance");
    }
    if (report.min_triangle_area_mm2 <= options.tolerance.area_epsilon_mm2 * 4.0)
    {
        report.warnings.push_back("thin feature warning: triangle area near tolerance");
    }
    if (report.max_triangle_aspect_ratio > 1000.0)
    {
        report.warnings.push_back("thin feature warning: high triangle aspect ratio");
    }
    if (report.min_edge_length_mm == std::numeric_limits<double>::max())
    {
        report.min_edge_length_mm = 0.0;
    }
    if (report.min_triangle_area_mm2 == std::numeric_limits<double>::max())
    {
        report.min_triangle_area_mm2 = 0.0;
    }
    return report;
}

std::string ValidateMeshRobustness(const MeshRobustnessReport& report, const bool rejectSelfIntersection)
{
    if (report.duplicate_faces > 0)
    {
        return "strict_closed rejected mesh with duplicate faces";
    }
    if (report.inconsistent_oriented_edges > 0)
    {
        return "strict_closed rejected mesh with local winding inconsistency";
    }
    if (rejectSelfIntersection && report.self_intersection_pairs > 0)
    {
        return "strict_closed rejected mesh with self-intersection candidates";
    }
    return {};
}

}  // namespace slicer_core
