#include "slicer_core/geometry/NearestTriangleQuery.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace slicer_core
{
namespace
{

struct Aabb
{
    Vec3 min{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
    Vec3 max{-std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()};
};

Vec3 Subtract(const Vec3& a, const Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 AddScaled(const Vec3& a, const Vec3& b, const double scale)
{
    return {a.x + b.x * scale, a.y + b.y * scale, a.z + b.z * scale};
}

double Dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double DistanceSquared(const Vec3& a, const Vec3& b)
{
    const Vec3 delta = Subtract(a, b);
    return Dot(delta, delta);
}

void Expand(Aabb& bounds, const Vec3& point)
{
    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.min.z = std::min(bounds.min.z, point.z);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
    bounds.max.z = std::max(bounds.max.z, point.z);
}

double PointAabbDistanceSquared(const Vec3& point, const Aabb& bounds)
{
    const auto AxisDistance = [](const double value, const double minimum, const double maximum)
    {
        if (value < minimum)
        {
            return minimum - value;
        }
        if (value > maximum)
        {
            return value - maximum;
        }
        return 0.0;
    };
    const double dx = AxisDistance(point.x, bounds.min.x, bounds.max.x);
    const double dy = AxisDistance(point.y, bounds.min.y, bounds.max.y);
    const double dz = AxisDistance(point.z, bounds.min.z, bounds.max.z);
    return dx * dx + dy * dy + dz * dz;
}

NearestTriangleHit ClosestPointOnTriangle(
    const Vec3& point,
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    const std::size_t triangleIndex)
{
    const Vec3 ab = Subtract(b, a);
    const Vec3 ac = Subtract(c, a);
    const Vec3 ap = Subtract(point, a);
    const double d1 = Dot(ab, ap);
    const double d2 = Dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0)
    {
        return {true, triangleIndex, a, {1.0, 0.0, 0.0}, std::sqrt(DistanceSquared(point, a))};
    }

    const Vec3 bp = Subtract(point, b);
    const double d3 = Dot(ab, bp);
    const double d4 = Dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3)
    {
        return {true, triangleIndex, b, {0.0, 1.0, 0.0}, std::sqrt(DistanceSquared(point, b))};
    }

    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
    {
        const double v = d1 / (d1 - d3);
        const Vec3 closest = AddScaled(a, ab, v);
        return {true, triangleIndex, closest, {1.0 - v, v, 0.0}, std::sqrt(DistanceSquared(point, closest))};
    }

    const Vec3 cp = Subtract(point, c);
    const double d5 = Dot(ab, cp);
    const double d6 = Dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6)
    {
        return {true, triangleIndex, c, {0.0, 0.0, 1.0}, std::sqrt(DistanceSquared(point, c))};
    }

    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
    {
        const double w = d2 / (d2 - d6);
        const Vec3 closest = AddScaled(a, ac, w);
        return {true, triangleIndex, closest, {1.0 - w, 0.0, w}, std::sqrt(DistanceSquared(point, closest))};
    }

    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0)
    {
        const Vec3 bc = Subtract(c, b);
        const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        const Vec3 closest = AddScaled(b, bc, w);
        return {true, triangleIndex, closest, {0.0, 1.0 - w, w}, std::sqrt(DistanceSquared(point, closest))};
    }

    const double denominator = 1.0 / (va + vb + vc);
    const double v = vb * denominator;
    const double w = vc * denominator;
    const Vec3 closest = AddScaled(AddScaled(a, ab, v), ac, w);
    return {true, triangleIndex, closest, {1.0 - v - w, v, w}, std::sqrt(DistanceSquared(point, closest))};
}

NearestTriangleHit QueryTriangle(const TriangleMeshData& mesh, const std::size_t triangleIndex, const Vec3& point)
{
    const std::array<int, 3>& triangle = mesh.triangles.at(triangleIndex);
    return ClosestPointOnTriangle(
        point,
        mesh.vertices.at(static_cast<std::size_t>(triangle.at(0))),
        mesh.vertices.at(static_cast<std::size_t>(triangle.at(1))),
        mesh.vertices.at(static_cast<std::size_t>(triangle.at(2))),
        triangleIndex);
}

double BarycentricInteriorMargin(const NearestTriangleHit& hit)
{
    return std::min({hit.barycentric.at(0), hit.barycentric.at(1), hit.barycentric.at(2)});
}

void AccumulateNearestHit(
    const NearestTriangleHit& candidate,
    const double tieEpsilonMm,
    NearestTriangleHit& best)
{
    if (!candidate.found)
    {
        return;
    }
    if (!best.found)
    {
        best = candidate;
        return;
    }

    const bool distanceTie = std::abs(candidate.distance_mm - best.distance_mm)
        <= tieEpsilonMm;
    const std::uint64_t tieCandidateCount = best.tie_candidate_count
        + (distanceTie ? 1U : 0U);
    if (IsBetterNearestTriangleHit(candidate, best, tieEpsilonMm))
    {
        best = candidate;
        best.tie_candidate_count = distanceTie ? tieCandidateCount : 0U;
    }
    else if (distanceTie)
    {
        best.tie_candidate_count = tieCandidateCount;
    }
}

}  // namespace

struct NearestTriangleQuery::Impl
{
    struct Node
    {
        Aabb bounds;
        int left{-1};
        int right{-1};
        std::size_t begin{0};
        std::size_t count{0};
    };

    TriangleMeshData mesh;
    std::vector<std::size_t> triangle_indices;
    std::vector<Node> nodes;

    explicit Impl(const TriangleMeshData& source) : mesh(source)
    {
        triangle_indices.resize(mesh.triangles.size());
        std::iota(triangle_indices.begin(), triangle_indices.end(), 0U);
        if (!triangle_indices.empty())
        {
            BuildNode(0, triangle_indices.size());
        }
    }

    Aabb TriangleBounds(const std::size_t triangleIndex) const
    {
        Aabb bounds;
        const std::array<int, 3>& triangle = mesh.triangles.at(triangleIndex);
        for (const int vertexIndex : triangle)
        {
            Expand(bounds, mesh.vertices.at(static_cast<std::size_t>(vertexIndex)));
        }
        return bounds;
    }

    Vec3 TriangleCentroid(const std::size_t triangleIndex) const
    {
        const std::array<int, 3>& triangle = mesh.triangles.at(triangleIndex);
        const Vec3& a = mesh.vertices.at(static_cast<std::size_t>(triangle.at(0)));
        const Vec3& b = mesh.vertices.at(static_cast<std::size_t>(triangle.at(1)));
        const Vec3& c = mesh.vertices.at(static_cast<std::size_t>(triangle.at(2)));
        return {(a.x + b.x + c.x) / 3.0, (a.y + b.y + c.y) / 3.0, (a.z + b.z + c.z) / 3.0};
    }

    int BuildNode(const std::size_t begin, const std::size_t end)
    {
        Node node;
        node.begin = begin;
        node.count = end - begin;
        Aabb centroidBounds;
        for (std::size_t index{begin}; index < end; ++index)
        {
            const Aabb triangleBounds = TriangleBounds(triangle_indices.at(index));
            Expand(node.bounds, triangleBounds.min);
            Expand(node.bounds, triangleBounds.max);
            Expand(centroidBounds, TriangleCentroid(triangle_indices.at(index)));
        }

        const int nodeIndex = static_cast<int>(nodes.size());
        nodes.push_back(node);
        if (node.count <= 4)
        {
            return nodeIndex;
        }

        const double extentX = centroidBounds.max.x - centroidBounds.min.x;
        const double extentY = centroidBounds.max.y - centroidBounds.min.y;
        const double extentZ = centroidBounds.max.z - centroidBounds.min.z;
        int axis{0};
        if (extentY > extentX && extentY >= extentZ)
        {
            axis = 1;
        }
        else if (extentZ > extentX && extentZ > extentY)
        {
            axis = 2;
        }
        const auto AxisValue = [this, axis](const std::size_t triangleIndex)
        {
            const Vec3 centroid = TriangleCentroid(triangleIndex);
            return axis == 0 ? centroid.x : (axis == 1 ? centroid.y : centroid.z);
        };
        const std::size_t middle = begin + node.count / 2U;
        std::nth_element(
            triangle_indices.begin() + static_cast<std::ptrdiff_t>(begin),
            triangle_indices.begin() + static_cast<std::ptrdiff_t>(middle),
            triangle_indices.begin() + static_cast<std::ptrdiff_t>(end),
            [&](const std::size_t left, const std::size_t right)
            {
                return AxisValue(left) < AxisValue(right);
            });

        nodes.at(static_cast<std::size_t>(nodeIndex)).left = BuildNode(begin, middle);
        nodes.at(static_cast<std::size_t>(nodeIndex)).right = BuildNode(middle, end);
        nodes.at(static_cast<std::size_t>(nodeIndex)).count = 0;
        return nodeIndex;
    }

    void QueryNode(
        const int nodeIndex,
        const Vec3& point,
        const NearestTriangleQueryOptions& options,
        NearestTriangleHit& best,
        NearestTriangleQueryStats* stats,
        std::uint64_t& visitedThisQuery) const
    {
        ++visitedThisQuery;
        if (stats != nullptr)
        {
            ++stats->visited_nodes;
            stats->max_visited_nodes = std::max(stats->max_visited_nodes, visitedThisQuery);
        }
        const Node& node = nodes.at(static_cast<std::size_t>(nodeIndex));
        const double bestSquared = best.found ? best.distance_mm * best.distance_mm : std::numeric_limits<double>::max();
        if (PointAabbDistanceSquared(point, node.bounds) > bestSquared)
        {
            return;
        }
        if (node.left < 0 && node.right < 0)
        {
            for (std::size_t offset{0}; offset < node.count; ++offset)
            {
                if (stats != nullptr)
                {
                    ++stats->tested_triangles;
                }
                const NearestTriangleHit hit = QueryTriangle(mesh, triangle_indices.at(node.begin + offset), point);
                AccumulateNearestHit(hit, options.tie_epsilon_mm, best);
            }
            return;
        }

        const Node& left = nodes.at(static_cast<std::size_t>(node.left));
        const Node& right = nodes.at(static_cast<std::size_t>(node.right));
        const double leftDistance = PointAabbDistanceSquared(point, left.bounds);
        const double rightDistance = PointAabbDistanceSquared(point, right.bounds);
        if (leftDistance <= rightDistance)
        {
            QueryNode(node.left, point, options, best, stats, visitedThisQuery);
            QueryNode(node.right, point, options, best, stats, visitedThisQuery);
        }
        else
        {
            QueryNode(node.right, point, options, best, stats, visitedThisQuery);
            QueryNode(node.left, point, options, best, stats, visitedThisQuery);
        }
    }
};

NearestTriangleQuery::NearestTriangleQuery(const TriangleMeshData& mesh) : m_impl(std::make_unique<Impl>(mesh))
{
    if (mesh.triangles.empty())
    {
        throw std::runtime_error("nearest triangle query requires triangles");
    }
}

NearestTriangleQuery::~NearestTriangleQuery() = default;
NearestTriangleQuery::NearestTriangleQuery(NearestTriangleQuery&&) noexcept = default;
NearestTriangleQuery& NearestTriangleQuery::operator=(NearestTriangleQuery&&) noexcept = default;

NearestTriangleHit NearestTriangleQuery::FindNearest(const Vec3& pointMm) const
{
    NearestTriangleHit result;
    if (!m_impl->nodes.empty())
    {
        NearestTriangleQueryStats stats;
        std::uint64_t visitedThisQuery{0};
        m_impl->QueryNode(0, pointMm, {}, result, &stats, visitedThisQuery);
    }
    return result;
}

NearestTriangleHit NearestTriangleQuery::FindNearestWithStats(
    const Vec3& pointMm,
    const NearestTriangleQueryOptions& options,
    NearestTriangleQueryStats& stats) const
{
    ++stats.query_count;
    NearestTriangleHit result;
    if (!m_impl->nodes.empty())
    {
        std::uint64_t visitedThisQuery{0};
        m_impl->QueryNode(0, pointMm, options, result, &stats, visitedThisQuery);
    }
    return result;
}

NearestTriangleQueryStats NearestTriangleQuery::GetBuildStats() const
{
    NearestTriangleQueryStats stats;
    stats.node_count = m_impl->nodes.size();
    stats.estimated_bytes =
        sizeof(Impl)
        + m_impl->nodes.size() * sizeof(Impl::Node)
        + m_impl->triangle_indices.size() * sizeof(std::size_t)
        + m_impl->mesh.vertices.size() * sizeof(Vec3)
        + m_impl->mesh.triangles.size() * sizeof(std::array<int, 3>);
    return stats;
}

NearestTriangleHit FindNearestTriangleBruteForce(const TriangleMeshData& mesh, const Vec3& pointMm)
{
    NearestTriangleHit best;
    for (std::size_t index{0}; index < mesh.triangles.size(); ++index)
    {
        const NearestTriangleHit hit = QueryTriangle(mesh, index, pointMm);
        AccumulateNearestHit(hit, 1.0e-7, best);
    }
    return best;
}

bool IsBetterNearestTriangleHit(
    const NearestTriangleHit& candidate,
    const NearestTriangleHit& best,
    const double tieEpsilonMm)
{
    if (!candidate.found)
    {
        return false;
    }
    if (!best.found)
    {
        return true;
    }
    if (candidate.distance_mm < best.distance_mm - tieEpsilonMm)
    {
        return true;
    }
    if (candidate.distance_mm > best.distance_mm + tieEpsilonMm)
    {
        return false;
    }

    const double candidateMargin = BarycentricInteriorMargin(candidate);
    const double bestMargin = BarycentricInteriorMargin(best);
    if (candidateMargin > bestMargin + 1.0e-12)
    {
        return true;
    }
    if (candidateMargin < bestMargin - 1.0e-12)
    {
        return false;
    }
    return candidate.triangle_index < best.triangle_index;
}

}  // namespace slicer_core
