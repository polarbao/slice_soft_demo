#include "slicer_core/geometry/repair/MeshCompleteSelfIntersectionAnalyzer.h"

#include "slicer_core/geometry/TriangleIntersectionQuery.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/system/ProcessMemoryStats.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

using Clock = std::chrono::steady_clock;
using TrianglePair = std::pair<std::uint64_t, std::uint64_t>;

constexpr std::size_t kLeafTriangleCount{8U};
constexpr std::size_t kInvalidNode{std::numeric_limits<std::size_t>::max()};

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

struct TriangleRecord
{
    Aabb bounds;
    Vec3 centroid;
};

struct BvhNode
{
    Aabb bounds;
    std::size_t begin{0U};
    std::size_t end{0U};
    std::size_t left{kInvalidNode};
    std::size_t right{kInvalidNode};

    bool IsLeaf() const
    {
        return left == kInvalidNode;
    }
};

double ElapsedMilliseconds(const Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
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

void Expand(Aabb& bounds, const Aabb& other)
{
    Expand(bounds, other.min);
    Expand(bounds, other.max);
}

bool Overlaps(const Aabb& left, const Aabb& right)
{
    return left.min.x <= right.max.x && left.max.x >= right.min.x
        && left.min.y <= right.max.y && left.max.y >= right.min.y
        && left.min.z <= right.max.z && left.max.z >= right.min.z;
}

double AxisValue(const Vec3& point, const std::size_t axis)
{
    if (axis == 0U)
    {
        return point.x;
    }
    if (axis == 1U)
    {
        return point.y;
    }
    return point.z;
}

std::size_t LongestAxis(const Aabb& bounds)
{
    const std::array<double, 3> extent{
        bounds.max.x - bounds.min.x,
        bounds.max.y - bounds.min.y,
        bounds.max.z - bounds.min.z};
    std::size_t axis{0U};
    for (std::size_t candidate{1U}; candidate < extent.size(); ++candidate)
    {
        if (extent.at(candidate) > extent.at(axis))
        {
            axis = candidate;
        }
    }
    return axis;
}

void ValidateInput(
    const TriangleMeshData& mesh,
    const MeshCompleteSelfIntersectionOptions& options)
{
    if (!std::isfinite(options.epsilonMm) || options.epsilonMm <= 0.0)
    {
        throw MeshRepairError(
            MeshRepairErrorCode::InputInvalid,
            "complete self-intersection analysis requires a finite positive epsilon");
    }
    for (const Vec3& vertex : mesh.vertices)
    {
        if (!std::isfinite(vertex.x)
            || !std::isfinite(vertex.y)
            || !std::isfinite(vertex.z))
        {
            throw MeshRepairError(
                MeshRepairErrorCode::InputInvalid,
                "complete self-intersection analysis requires finite vertices");
        }
    }
    for (const std::array<int, 3>& triangle : mesh.triangles)
    {
        for (const int vertexIndex : triangle)
        {
            if (vertexIndex < 0
                || static_cast<std::size_t>(vertexIndex) >= mesh.vertices.size())
            {
                throw MeshRepairError(
                    MeshRepairErrorCode::InputInvalid,
                    "complete self-intersection analysis received an invalid vertex index");
            }
        }
    }
}

std::vector<TriangleRecord> BuildTriangleRecords(
    const TriangleMeshData& mesh,
    const double epsilonMm)
{
    std::vector<TriangleRecord> records;
    records.reserve(mesh.triangles.size());
    for (const std::array<int, 3>& triangle : mesh.triangles)
    {
        TriangleRecord record;
        Vec3 sum{};
        for (const int vertexIndex : triangle)
        {
            const Vec3& vertex = mesh.vertices.at(
                static_cast<std::size_t>(vertexIndex));
            Expand(record.bounds, vertex);
            sum.x += vertex.x;
            sum.y += vertex.y;
            sum.z += vertex.z;
        }
        record.bounds.min.x -= epsilonMm;
        record.bounds.min.y -= epsilonMm;
        record.bounds.min.z -= epsilonMm;
        record.bounds.max.x += epsilonMm;
        record.bounds.max.y += epsilonMm;
        record.bounds.max.z += epsilonMm;
        record.centroid = {sum.x / 3.0, sum.y / 3.0, sum.z / 3.0};
        records.push_back(record);
    }
    return records;
}

std::size_t BuildBvhNode(
    const std::vector<TriangleRecord>& records,
    std::vector<std::size_t>& order,
    std::vector<BvhNode>& nodes,
    const std::size_t begin,
    const std::size_t end)
{
    BvhNode node;
    node.begin = begin;
    node.end = end;
    Aabb centroidBounds;
    for (std::size_t position{begin}; position < end; ++position)
    {
        const TriangleRecord& record = records.at(order.at(position));
        Expand(node.bounds, record.bounds);
        Expand(centroidBounds, record.centroid);
    }

    const std::size_t nodeIndex = nodes.size();
    nodes.push_back(node);
    if (end - begin <= kLeafTriangleCount)
    {
        return nodeIndex;
    }

    const std::size_t axis = LongestAxis(centroidBounds);
    std::stable_sort(
        order.begin() + static_cast<std::ptrdiff_t>(begin),
        order.begin() + static_cast<std::ptrdiff_t>(end),
        [&records, axis](const std::size_t left, const std::size_t right)
        {
            const double leftValue = AxisValue(records.at(left).centroid, axis);
            const double rightValue = AxisValue(records.at(right).centroid, axis);
            return leftValue < rightValue
                || (leftValue == rightValue && left < right);
        });
    const std::size_t middle = begin + (end - begin) / 2U;
    const std::size_t left = BuildBvhNode(records, order, nodes, begin, middle);
    const std::size_t right = BuildBvhNode(records, order, nodes, middle, end);
    nodes.at(nodeIndex).left = left;
    nodes.at(nodeIndex).right = right;
    return nodeIndex;
}

bool TryAppendCandidate(
    const TriangleMeshData& mesh,
    const std::vector<TriangleRecord>& records,
    const std::size_t left,
    const std::size_t right,
    const std::uint64_t maxCandidatePairs,
    std::vector<TrianglePair>& pairs)
{
    const std::size_t low = std::min(left, right);
    const std::size_t high = std::max(left, right);
    if (TrianglesShareVertexIndex(
            mesh.triangles.at(low),
            mesh.triangles.at(high))
        || !Overlaps(records.at(low).bounds, records.at(high).bounds))
    {
        return true;
    }
    pairs.emplace_back(low, high);
    return pairs.size() <= maxCandidatePairs;
}

bool CollectLeafPairs(
    const TriangleMeshData& mesh,
    const std::vector<TriangleRecord>& records,
    const std::vector<std::size_t>& order,
    const BvhNode& leftNode,
    const BvhNode& rightNode,
    const bool sameNode,
    const std::uint64_t maxCandidatePairs,
    std::vector<TrianglePair>& pairs)
{
    for (std::size_t leftPosition{leftNode.begin};
         leftPosition < leftNode.end;
         ++leftPosition)
    {
        const std::size_t rightBegin = sameNode
            ? leftPosition + 1U
            : rightNode.begin;
        for (std::size_t rightPosition{rightBegin};
             rightPosition < rightNode.end;
             ++rightPosition)
        {
            if (!TryAppendCandidate(
                    mesh,
                    records,
                    order.at(leftPosition),
                    order.at(rightPosition),
                    maxCandidatePairs,
                    pairs))
            {
                return false;
            }
        }
    }
    return true;
}

bool CollectNodePairs(
    const TriangleMeshData& mesh,
    const std::vector<TriangleRecord>& records,
    const std::vector<std::size_t>& order,
    const std::vector<BvhNode>& nodes,
    const std::size_t leftIndex,
    const std::size_t rightIndex,
    const std::uint64_t maxCandidatePairs,
    std::vector<TrianglePair>& pairs)
{
    const BvhNode& leftNode = nodes.at(leftIndex);
    const BvhNode& rightNode = nodes.at(rightIndex);
    if (!Overlaps(leftNode.bounds, rightNode.bounds))
    {
        return true;
    }
    if (leftIndex == rightIndex)
    {
        if (leftNode.IsLeaf())
        {
            return CollectLeafPairs(
                mesh,
                records,
                order,
                leftNode,
                rightNode,
                true,
                maxCandidatePairs,
                pairs);
        }
        return CollectNodePairs(
                   mesh,
                   records,
                   order,
                   nodes,
                   leftNode.left,
                   leftNode.left,
                   maxCandidatePairs,
                   pairs)
            && CollectNodePairs(
                mesh,
                records,
                order,
                nodes,
                leftNode.left,
                leftNode.right,
                maxCandidatePairs,
                pairs)
            && CollectNodePairs(
                mesh,
                records,
                order,
                nodes,
                leftNode.right,
                leftNode.right,
                maxCandidatePairs,
                pairs);
    }
    if (leftNode.IsLeaf() && rightNode.IsLeaf())
    {
        return CollectLeafPairs(
            mesh,
            records,
            order,
            leftNode,
            rightNode,
            false,
            maxCandidatePairs,
            pairs);
    }
    const std::size_t leftCount = leftNode.end - leftNode.begin;
    const std::size_t rightCount = rightNode.end - rightNode.begin;
    if (!leftNode.IsLeaf()
        && (rightNode.IsLeaf() || leftCount >= rightCount))
    {
        return CollectNodePairs(
                   mesh,
                   records,
                   order,
                   nodes,
                   leftNode.left,
                   rightIndex,
                   maxCandidatePairs,
                   pairs)
            && CollectNodePairs(
                mesh,
                records,
                order,
                nodes,
                leftNode.right,
                rightIndex,
                maxCandidatePairs,
                pairs);
    }
    return CollectNodePairs(
               mesh,
               records,
               order,
               nodes,
               leftIndex,
               rightNode.left,
               maxCandidatePairs,
               pairs)
        && CollectNodePairs(
            mesh,
            records,
            order,
            nodes,
            leftIndex,
            rightNode.right,
            maxCandidatePairs,
            pairs);
}

void CapturePerformance(
    MeshCompleteSelfIntersectionAnalysis& analysis,
    const Clock::time_point start)
{
    analysis.durationMs = ElapsedMilliseconds(start);
    const ProcessMemoryStats memory = CaptureProcessMemoryStats();
    if (memory.available)
    {
        analysis.peakWorkingSetBytes = memory.peak_working_set_bytes;
    }
}

void BlockAnalysis(
    MeshCompleteSelfIntersectionAnalysis& analysis,
    const std::string& blockerCode,
    const std::string& message)
{
    analysis.status = "budget_or_resource_blocked";
    analysis.complete = false;
    analysis.blockerCode = blockerCode;
    analysis.issues.push_back(MakeValidationIssue(
        blockerCode,
        ValidationSeverity::Warning,
        message));
}

void ClassifyPairs(
    const TriangleMeshData& mesh,
    const MeshCompleteSelfIntersectionOptions& options,
    const std::vector<TrianglePair>& pairs,
    MeshCompleteSelfIntersectionAnalysis& analysis)
{
    for (const auto& [left, right] : pairs)
    {
        const TriangleIntersectionResult intersection = TestTriangleIntersection(
            mesh,
            static_cast<std::size_t>(left),
            static_cast<std::size_t>(right),
            options.epsilonMm);
        ++analysis.testedPairCount;
        switch (intersection.kind)
        {
        case TriangleIntersectionKind::ConfirmedIntersection:
            ++analysis.confirmedIntersectionPairs;
            break;
        case TriangleIntersectionKind::CoplanarOverlap:
            ++analysis.coplanarOverlapPairs;
            break;
        case TriangleIntersectionKind::TouchingOnly:
            ++analysis.touchingOnlyPairs;
            break;
        case TriangleIntersectionKind::AabbOnly:
        case TriangleIntersectionKind::None:
            ++analysis.aabbOnlyPairs;
            break;
        }
    }

    analysis.complete = true;
    if (analysis.confirmedIntersectionPairs > 0U)
    {
        analysis.status = "confirmed_intersection";
    }
    else if (analysis.coplanarOverlapPairs > 0U)
    {
        analysis.status = "coplanar_overlap";
    }
    else if (analysis.touchingOnlyPairs > 0U)
    {
        analysis.status = "touching_only";
    }
    else
    {
        analysis.status = "complete_no_intersection";
    }
}

}  // namespace

MeshCompleteSelfIntersectionAnalysis AnalyzeCompleteMeshSelfIntersections(
    const TriangleMeshData& mesh,
    const MeshCompleteSelfIntersectionOptions& options)
{
    ValidateInput(mesh, options);
    const Clock::time_point start = Clock::now();
    MeshCompleteSelfIntersectionAnalysis analysis;
    analysis.triangleCount = static_cast<std::uint64_t>(mesh.triangles.size());
    try
    {
        const std::vector<TriangleRecord> records = BuildTriangleRecords(
            mesh,
            options.epsilonMm);
        std::vector<std::size_t> order(mesh.triangles.size());
        std::iota(order.begin(), order.end(), 0U);
        std::vector<BvhNode> nodes;
        if (!order.empty())
        {
            nodes.reserve(order.size() * 2U);
            static_cast<void>(BuildBvhNode(
                records,
                order,
                nodes,
                0U,
                order.size()));
        }
        analysis.bvhNodeCount = static_cast<std::uint64_t>(nodes.size());

        std::vector<TrianglePair> pairs;
        const std::uint64_t reserveCount = std::min<std::uint64_t>(
            options.maxCandidatePairs,
            static_cast<std::uint64_t>(mesh.triangles.size()) * 4U);
        pairs.reserve(static_cast<std::size_t>(reserveCount));
        const bool withinBudget = nodes.empty()
            || CollectNodePairs(
                mesh,
                records,
                order,
                nodes,
                0U,
                0U,
                options.maxCandidatePairs,
                pairs);
        analysis.candidatePairCount = static_cast<std::uint64_t>(pairs.size());
        if (!withinBudget)
        {
            BlockAnalysis(
                analysis,
                "MESH_SELF_INTERSECTION_CANDIDATE_BUDGET_EXCEEDED",
                "complete self-intersection candidate pair budget was exceeded");
            CapturePerformance(analysis, start);
            return analysis;
        }

        std::sort(pairs.begin(), pairs.end());
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
        analysis.candidatePairCount = static_cast<std::uint64_t>(pairs.size());
        analysis.candidatePairHash = ComputeMeshRepairTrianglePairHash(pairs);
        ClassifyPairs(mesh, options, pairs, analysis);
    }
    catch (const std::bad_alloc&)
    {
        BlockAnalysis(
            analysis,
            "MESH_SELF_INTERSECTION_RESOURCE_BLOCKED",
            "complete self-intersection analysis exceeded available memory");
    }
    CapturePerformance(analysis, start);
    return analysis;
}

}  // namespace slicer_core
