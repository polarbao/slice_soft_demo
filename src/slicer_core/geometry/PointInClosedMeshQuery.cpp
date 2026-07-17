#include "slicer_core/geometry/PointInClosedMeshQuery.h"

#include <algorithm>
#include <array>
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
    Vec3 min{
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max()};
    Vec3 max{
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max()};
};

enum class RayTriangleKind
{
    None,
    Hit,
    Boundary,
    Ambiguous,
};

struct RayEvaluation
{
    bool boundary{false};
    bool ambiguous{false};
    std::uint64_t intersections{0U};
};

Vec3 Subtract(const Vec3& first, const Vec3& second)
{
    return {
        first.x - second.x,
        first.y - second.y,
        first.z - second.z};
}

Vec3 Cross(const Vec3& first, const Vec3& second)
{
    return {
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x};
}

double Dot(const Vec3& first, const Vec3& second)
{
    return first.x * second.x + first.y * second.y + first.z * second.z;
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

bool RayIntersectsAabb(
    const Vec3& origin,
    const Vec3& direction,
    const Aabb& bounds,
    const double epsilonMm)
{
    double minimumDistance{0.0};
    double maximumDistance{std::numeric_limits<double>::max()};
    const std::array<double, 3> origins{origin.x, origin.y, origin.z};
    const std::array<double, 3> directions{direction.x, direction.y, direction.z};
    const std::array<double, 3> minimums{bounds.min.x, bounds.min.y, bounds.min.z};
    const std::array<double, 3> maximums{bounds.max.x, bounds.max.y, bounds.max.z};
    for (std::size_t axis{0U}; axis < 3U; ++axis)
    {
        if (std::abs(directions.at(axis)) <= 1.0e-15)
        {
            if (origins.at(axis) < minimums.at(axis) - epsilonMm
                || origins.at(axis) > maximums.at(axis) + epsilonMm)
            {
                return false;
            }
            continue;
        }

        double nearDistance =
            (minimums.at(axis) - origins.at(axis)) / directions.at(axis);
        double farDistance =
            (maximums.at(axis) - origins.at(axis)) / directions.at(axis);
        if (nearDistance > farDistance)
        {
            std::swap(nearDistance, farDistance);
        }
        minimumDistance = std::max(minimumDistance, nearDistance);
        maximumDistance = std::min(maximumDistance, farDistance);
        if (maximumDistance + epsilonMm < minimumDistance)
        {
            return false;
        }
    }
    return maximumDistance >= -epsilonMm;
}

RayTriangleKind IntersectTriangle(
    const TriangleMeshData& mesh,
    const std::size_t triangleIndex,
    const Vec3& origin,
    const Vec3& direction,
    const PointInClosedMeshQueryOptions& options)
{
    const std::array<int, 3>& triangle = mesh.triangles.at(triangleIndex);
    const Vec3& a = mesh.vertices.at(static_cast<std::size_t>(triangle.at(0)));
    const Vec3& b = mesh.vertices.at(static_cast<std::size_t>(triangle.at(1)));
    const Vec3& c = mesh.vertices.at(static_cast<std::size_t>(triangle.at(2)));
    const Vec3 edgeAb = Subtract(b, a);
    const Vec3 edgeAc = Subtract(c, a);
    const Vec3 cross = Cross(direction, edgeAc);
    const double determinant = Dot(edgeAb, cross);
    if (std::abs(determinant) <= 1.0e-14)
    {
        return RayTriangleKind::None;
    }

    const double inverseDeterminant = 1.0 / determinant;
    const Vec3 fromA = Subtract(origin, a);
    const double u = Dot(fromA, cross) * inverseDeterminant;
    const Vec3 q = Cross(fromA, edgeAb);
    const double v = Dot(direction, q) * inverseDeterminant;
    const double w = 1.0 - u - v;
    if (u < -options.barycentricEpsilon
        || v < -options.barycentricEpsilon
        || w < -options.barycentricEpsilon)
    {
        return RayTriangleKind::None;
    }

    const double distance = Dot(edgeAc, q) * inverseDeterminant;
    if (distance < -options.boundaryEpsilonMm)
    {
        return RayTriangleKind::None;
    }
    if (std::abs(distance) <= options.boundaryEpsilonMm)
    {
        return RayTriangleKind::Boundary;
    }
    if (u <= options.barycentricEpsilon
        || v <= options.barycentricEpsilon
        || w <= options.barycentricEpsilon)
    {
        return RayTriangleKind::Ambiguous;
    }
    return RayTriangleKind::Hit;
}

PointInClosedMeshResult MakeClassification(
    const RayEvaluation& evaluation,
    const bool usedFallbackRay)
{
    PointInClosedMeshResult result;
    result.boundary = evaluation.boundary;
    result.ambiguous = evaluation.ambiguous;
    result.usedFallbackRay = usedFallbackRay;
    result.intersectionCount = evaluation.intersections;
    result.inside = result.boundary
        || (!result.ambiguous && (result.intersectionCount % 2U) == 1U);
    return result;
}

const std::array<Vec3, 5>& StableRayDirections()
{
    static const std::array<Vec3, 5> directions{
        Vec3{1.0, 0.3713906763541037, 0.1547322982293498},
        Vec3{0.2193817272381391, 1.0, 0.4137839182371407},
        Vec3{0.5129381723819273, 0.2719381723819273, 1.0},
        Vec3{1.0, 0.6180339887498948, 0.4142135623730950},
        Vec3{0.7320508075688772, 1.0, 0.2360679774997897},
    };
    return directions;
}

RayEvaluation EvaluateBruteForceRay(
    const TriangleMeshData& mesh,
    const Vec3& pointMm,
    const Vec3& direction,
    const PointInClosedMeshQueryOptions& options)
{
    RayEvaluation evaluation;
    for (std::size_t triangleIndex{0U};
         triangleIndex < mesh.triangles.size();
         ++triangleIndex)
    {
        const RayTriangleKind kind = IntersectTriangle(
            mesh,
            triangleIndex,
            pointMm,
            direction,
            options);
        if (kind == RayTriangleKind::Boundary)
        {
            evaluation.boundary = true;
            return evaluation;
        }
        if (kind == RayTriangleKind::Ambiguous)
        {
            evaluation.ambiguous = true;
        }
        else if (kind == RayTriangleKind::Hit)
        {
            ++evaluation.intersections;
        }
    }
    return evaluation;
}

}  // namespace

struct PointInClosedMeshQuery::Impl
{
    struct Node
    {
        Aabb bounds;
        int left{-1};
        int right{-1};
        std::size_t begin{0U};
        std::size_t count{0U};
    };

    TriangleMeshData mesh;
    std::vector<std::size_t> triangleIndices;
    std::vector<Node> nodes;

    explicit Impl(const TriangleMeshData& source)
        : mesh(source)
    {
        triangleIndices.resize(mesh.triangles.size());
        std::iota(triangleIndices.begin(), triangleIndices.end(), 0U);
        if (!triangleIndices.empty())
        {
            BuildNode(0U, triangleIndices.size());
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
        return {
            (a.x + b.x + c.x) / 3.0,
            (a.y + b.y + c.y) / 3.0,
            (a.z + b.z + c.z) / 3.0};
    }

    int BuildNode(const std::size_t begin, const std::size_t end)
    {
        Node node;
        node.begin = begin;
        node.count = end - begin;
        Aabb centroidBounds;
        for (std::size_t index{begin}; index < end; ++index)
        {
            const Aabb triangleBounds = TriangleBounds(triangleIndices.at(index));
            Expand(node.bounds, triangleBounds.min);
            Expand(node.bounds, triangleBounds.max);
            Expand(centroidBounds, TriangleCentroid(triangleIndices.at(index)));
        }

        const int nodeIndex = static_cast<int>(nodes.size());
        nodes.push_back(node);
        if (node.count <= 4U)
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
        const auto axisValue = [this, axis](const std::size_t triangleIndex)
        {
            const Vec3 centroid = TriangleCentroid(triangleIndex);
            return axis == 0 ? centroid.x : (axis == 1 ? centroid.y : centroid.z);
        };
        const std::size_t middle = begin + node.count / 2U;
        std::nth_element(
            triangleIndices.begin() + static_cast<std::ptrdiff_t>(begin),
            triangleIndices.begin() + static_cast<std::ptrdiff_t>(middle),
            triangleIndices.begin() + static_cast<std::ptrdiff_t>(end),
            [&](const std::size_t first, const std::size_t second)
            {
                return axisValue(first) < axisValue(second);
            });

        nodes.at(static_cast<std::size_t>(nodeIndex)).left =
            BuildNode(begin, middle);
        nodes.at(static_cast<std::size_t>(nodeIndex)).right =
            BuildNode(middle, end);
        nodes.at(static_cast<std::size_t>(nodeIndex)).count = 0U;
        return nodeIndex;
    }

    void EvaluateNode(
        const int nodeIndex,
        const Vec3& pointMm,
        const Vec3& direction,
        const PointInClosedMeshQueryOptions& options,
        PointInClosedMeshQueryStats* stats,
        RayEvaluation& evaluation) const
    {
        if (evaluation.boundary)
        {
            return;
        }
        const Node& node = nodes.at(static_cast<std::size_t>(nodeIndex));
        if (stats != nullptr)
        {
            ++stats->visitedNodes;
        }
        if (!RayIntersectsAabb(
                pointMm,
                direction,
                node.bounds,
                options.boundaryEpsilonMm))
        {
            return;
        }
        if (node.left < 0 && node.right < 0)
        {
            for (std::size_t offset{0U}; offset < node.count; ++offset)
            {
                if (stats != nullptr)
                {
                    ++stats->testedTriangles;
                }
                const RayTriangleKind kind = IntersectTriangle(
                    mesh,
                    triangleIndices.at(node.begin + offset),
                    pointMm,
                    direction,
                    options);
                if (kind == RayTriangleKind::Boundary)
                {
                    evaluation.boundary = true;
                    return;
                }
                if (kind == RayTriangleKind::Ambiguous)
                {
                    evaluation.ambiguous = true;
                }
                else if (kind == RayTriangleKind::Hit)
                {
                    ++evaluation.intersections;
                }
            }
            return;
        }
        EvaluateNode(
            node.left,
            pointMm,
            direction,
            options,
            stats,
            evaluation);
        EvaluateNode(
            node.right,
            pointMm,
            direction,
            options,
            stats,
            evaluation);
    }
};

PointInClosedMeshQuery::PointInClosedMeshQuery(const TriangleMeshData& mesh)
    : m_impl(std::make_unique<Impl>(mesh))
{
    if (mesh.triangles.empty())
    {
        throw std::runtime_error("point-in-closed-mesh query requires triangles");
    }
}

PointInClosedMeshQuery::~PointInClosedMeshQuery() = default;
PointInClosedMeshQuery::PointInClosedMeshQuery(
    PointInClosedMeshQuery&& other) noexcept = default;
PointInClosedMeshQuery& PointInClosedMeshQuery::operator=(
    PointInClosedMeshQuery&& other) noexcept = default;

PointInClosedMeshResult PointInClosedMeshQuery::Classify(
    const Vec3& pointMm,
    const PointInClosedMeshQueryOptions& options,
    PointInClosedMeshQueryStats& stats) const
{
    if (!std::isfinite(options.boundaryEpsilonMm)
        || options.boundaryEpsilonMm < 0.0
        || !std::isfinite(options.barycentricEpsilon)
        || options.barycentricEpsilon < 0.0)
    {
        throw std::runtime_error(
            "point-in-closed-mesh tolerances must be finite and non-negative");
    }
    ++stats.queryCount;
    const std::array<Vec3, 5>& directions = StableRayDirections();
    for (std::size_t directionIndex{0U};
         directionIndex < directions.size();
         ++directionIndex)
    {
        RayEvaluation evaluation;
        m_impl->EvaluateNode(
            0,
            pointMm,
            directions.at(directionIndex),
            options,
            &stats,
            evaluation);
        if (evaluation.boundary)
        {
            ++stats.boundaryPointCount;
            if (directionIndex > 0U)
            {
                ++stats.fallbackRayCount;
            }
            return MakeClassification(evaluation, directionIndex > 0U);
        }
        if (!evaluation.ambiguous)
        {
            if (directionIndex > 0U)
            {
                ++stats.fallbackRayCount;
            }
            return MakeClassification(evaluation, directionIndex > 0U);
        }
        ++stats.ambiguousRayCount;
    }

    RayEvaluation ambiguous;
    ambiguous.ambiguous = true;
    ++stats.fallbackRayCount;
    return MakeClassification(ambiguous, true);
}

PointInClosedMeshQueryStats PointInClosedMeshQuery::GetBuildStats() const
{
    PointInClosedMeshQueryStats stats;
    stats.nodeCount = m_impl->nodes.size();
    stats.estimatedBytes =
        sizeof(Impl)
        + m_impl->nodes.size() * sizeof(Impl::Node)
        + m_impl->triangleIndices.size() * sizeof(std::size_t)
        + m_impl->mesh.vertices.size() * sizeof(Vec3)
        + m_impl->mesh.triangles.size() * sizeof(std::array<int, 3>);
    return stats;
}

PointInClosedMeshResult ClassifyPointInClosedMeshBruteForce(
    const TriangleMeshData& mesh,
    const Vec3& pointMm,
    const PointInClosedMeshQueryOptions& options)
{
    if (!std::isfinite(options.boundaryEpsilonMm)
        || options.boundaryEpsilonMm < 0.0
        || !std::isfinite(options.barycentricEpsilon)
        || options.barycentricEpsilon < 0.0)
    {
        throw std::runtime_error(
            "point-in-closed-mesh tolerances must be finite and non-negative");
    }
    const std::array<Vec3, 5>& directions = StableRayDirections();
    for (std::size_t directionIndex{0U};
         directionIndex < directions.size();
         ++directionIndex)
    {
        const RayEvaluation evaluation = EvaluateBruteForceRay(
            mesh,
            pointMm,
            directions.at(directionIndex),
            options);
        if (evaluation.boundary || !evaluation.ambiguous)
        {
            return MakeClassification(evaluation, directionIndex > 0U);
        }
    }

    RayEvaluation ambiguous;
    ambiguous.ambiguous = true;
    return MakeClassification(ambiguous, true);
}

}  // namespace slicer_core
