#include "slicer_core/geometry/repair/MeshRepairBoundaryOperations.h"

#include "slicer_core/geometry/MeshTopologyDiagnostics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

using EdgeKey = std::pair<int, int>;

struct DirectedEdgeUse
{
    std::size_t triangleIndex{0U};
    int from{0};
    int to{0};
};

struct Point2
{
    double x{0.0};
    double y{0.0};
};

struct BoundaryLoop
{
    std::vector<std::size_t> vertices;
    std::vector<std::size_t> adjacentTriangleIndices;
    double diameterMm{0.0};
    double perimeterMm{0.0};
    double planarityErrorMm{0.0};
    double areaMm2{0.0};
};

struct BoundaryAnalysis
{
    std::vector<BoundaryLoop> loops;
    std::size_t boundaryEdgeCount{0U};
    bool blocked{false};
    std::string attributeStatus;
    std::string blockerCode;
};

EdgeKey MakeEdgeKey(const int left, const int right)
{
    return std::minmax(left, right);
}

std::array<std::pair<int, int>, 3> DirectedEdges(
    const std::array<int, 3>& triangle)
{
    return {
        std::make_pair(triangle.at(0U), triangle.at(1U)),
        std::make_pair(triangle.at(1U), triangle.at(2U)),
        std::make_pair(triangle.at(2U), triangle.at(0U)),
    };
}

std::map<EdgeKey, std::vector<DirectedEdgeUse>> BuildEdgeUses(
    const TriangleMeshData& mesh)
{
    std::map<EdgeKey, std::vector<DirectedEdgeUse>> edgeUses;
    for (std::size_t triangleIndex{0U}; triangleIndex < mesh.triangles.size(); ++triangleIndex)
    {
        for (const std::pair<int, int>& edge : DirectedEdges(mesh.triangles.at(triangleIndex)))
        {
            edgeUses[MakeEdgeKey(edge.first, edge.second)].push_back(
                {triangleIndex, edge.first, edge.second});
        }
    }
    return edgeUses;
}

double Distance(const Vec3& left, const Vec3& right)
{
    const double dx = left.x - right.x;
    const double dy = left.y - right.y;
    const double dz = left.z - right.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Point2 ProjectPoint(const Vec3& point, const std::size_t droppedAxis)
{
    if (droppedAxis == 0U)
    {
        return {point.y, point.z};
    }
    if (droppedAxis == 1U)
    {
        return {point.x, point.z};
    }
    return {point.x, point.y};
}

double Cross2(const Point2& a, const Point2& b, const Point2& c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool PointOnSegment(
    const Point2& point,
    const Point2& start,
    const Point2& end,
    const double epsilon)
{
    return std::abs(Cross2(start, end, point)) <= epsilon
        && point.x >= std::min(start.x, end.x) - epsilon
        && point.x <= std::max(start.x, end.x) + epsilon
        && point.y >= std::min(start.y, end.y) - epsilon
        && point.y <= std::max(start.y, end.y) + epsilon;
}

int OrientationSign(
    const Point2& a,
    const Point2& b,
    const Point2& c,
    const double epsilon)
{
    const double value = Cross2(a, b, c);
    if (value > epsilon)
    {
        return 1;
    }
    if (value < -epsilon)
    {
        return -1;
    }
    return 0;
}

bool SegmentsIntersect(
    const Point2& a,
    const Point2& b,
    const Point2& c,
    const Point2& d,
    const double epsilon)
{
    const int abC = OrientationSign(a, b, c, epsilon);
    const int abD = OrientationSign(a, b, d, epsilon);
    const int cdA = OrientationSign(c, d, a, epsilon);
    const int cdB = OrientationSign(c, d, b, epsilon);
    if (abC != abD && cdA != cdB)
    {
        return true;
    }
    return (abC == 0 && PointOnSegment(c, a, b, epsilon))
        || (abD == 0 && PointOnSegment(d, a, b, epsilon))
        || (cdA == 0 && PointOnSegment(a, c, d, epsilon))
        || (cdB == 0 && PointOnSegment(b, c, d, epsilon));
}

bool IsSimplePolygon(const std::vector<Point2>& points, const double epsilon)
{
    const std::size_t count = points.size();
    for (std::size_t first{0U}; first < count; ++first)
    {
        const std::size_t firstNext = (first + 1U) % count;
        for (std::size_t second{first + 1U}; second < count; ++second)
        {
            const std::size_t secondNext = (second + 1U) % count;
            if (first == second || firstNext == second || secondNext == first)
            {
                continue;
            }
            if (SegmentsIntersect(
                    points.at(first),
                    points.at(firstNext),
                    points.at(second),
                    points.at(secondNext),
                    epsilon))
            {
                return false;
            }
        }
    }
    return true;
}

bool IsStrictlyConvex(const std::vector<Point2>& points, const double epsilon)
{
    int expectedSign{0};
    for (std::size_t index{0U}; index < points.size(); ++index)
    {
        const int sign = OrientationSign(
            points.at(index),
            points.at((index + 1U) % points.size()),
            points.at((index + 2U) % points.size()),
            epsilon);
        if (sign == 0)
        {
            return false;
        }
        if (expectedSign == 0)
        {
            expectedSign = sign;
        }
        else if (sign != expectedSign)
        {
            return false;
        }
    }
    return true;
}

bool ComputeLoopMetrics(
    const TriangleMeshData& mesh,
    BoundaryLoop& loop,
    const double areaEpsilonMm2)
{
    Vec3 centroid;
    for (const std::size_t vertexIndex : loop.vertices)
    {
        const Vec3& point = mesh.vertices.at(vertexIndex);
        centroid.x += point.x;
        centroid.y += point.y;
        centroid.z += point.z;
    }
    const double inverseCount = 1.0 / static_cast<double>(loop.vertices.size());
    centroid.x *= inverseCount;
    centroid.y *= inverseCount;
    centroid.z *= inverseCount;

    Vec3 normal;
    for (std::size_t index{0U}; index < loop.vertices.size(); ++index)
    {
        const Vec3& current = mesh.vertices.at(loop.vertices.at(index));
        const Vec3& next = mesh.vertices.at(
            loop.vertices.at((index + 1U) % loop.vertices.size()));
        normal.x += (current.y - next.y) * (current.z + next.z);
        normal.y += (current.z - next.z) * (current.x + next.x);
        normal.z += (current.x - next.x) * (current.y + next.y);
        loop.perimeterMm += Distance(current, next);
        for (std::size_t other{index + 1U}; other < loop.vertices.size(); ++other)
        {
            loop.diameterMm = std::max(
                loop.diameterMm,
                Distance(current, mesh.vertices.at(loop.vertices.at(other))));
        }
    }

    const double normalLength = std::sqrt(
        normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (!(normalLength > areaEpsilonMm2))
    {
        return false;
    }
    normal.x /= normalLength;
    normal.y /= normalLength;
    normal.z /= normalLength;
    for (const std::size_t vertexIndex : loop.vertices)
    {
        const Vec3& point = mesh.vertices.at(vertexIndex);
        const double error = std::abs(
            (point.x - centroid.x) * normal.x
            + (point.y - centroid.y) * normal.y
            + (point.z - centroid.z) * normal.z);
        loop.planarityErrorMm = std::max(loop.planarityErrorMm, error);
    }

    const std::array<double, 3> absoluteNormal{
        std::abs(normal.x),
        std::abs(normal.y),
        std::abs(normal.z)};
    const std::size_t droppedAxis = static_cast<std::size_t>(
        std::distance(
            absoluteNormal.begin(),
            std::max_element(absoluteNormal.begin(), absoluteNormal.end())));
    std::vector<Point2> projected;
    projected.reserve(loop.vertices.size());
    for (const std::size_t vertexIndex : loop.vertices)
    {
        projected.push_back(ProjectPoint(mesh.vertices.at(vertexIndex), droppedAxis));
    }
    double signedArea{0.0};
    for (std::size_t index{0U}; index < projected.size(); ++index)
    {
        const Point2& current = projected.at(index);
        const Point2& next = projected.at((index + 1U) % projected.size());
        signedArea += current.x * next.y - next.x * current.y;
    }
    const double projectedAreaMm2 = std::abs(signedArea) * 0.5;
    loop.areaMm2 = projectedAreaMm2 / absoluteNormal.at(droppedAxis);
    return loop.areaMm2 > areaEpsilonMm2
        && IsSimplePolygon(projected, areaEpsilonMm2)
        && IsStrictlyConvex(projected, areaEpsilonMm2);
}

void SetBlocked(
    BoundaryAnalysis& analysis,
    const std::string& attributeStatus,
    const MeshRepairErrorCode errorCode)
{
    analysis.blocked = true;
    analysis.attributeStatus = attributeStatus;
    analysis.blockerCode = MeshRepairErrorCodeName(errorCode);
}

BoundaryAnalysis AnalyzeBoundaryLoops(
    const MeshRepairBoundaryOperationRequest& request,
    const std::map<EdgeKey, std::vector<DirectedEdgeUse>>& edgeUses)
{
    BoundaryAnalysis analysis;
    std::map<std::size_t, std::size_t> outgoing;
    std::map<std::size_t, std::size_t> incoming;
    std::map<std::size_t, std::set<std::size_t>> neighbors;
    std::map<EdgeKey, std::size_t> adjacentTriangleByEdge;
    for (const auto& [edge, uses] : edgeUses)
    {
        if (uses.size() != 1U)
        {
            continue;
        }
        ++analysis.boundaryEdgeCount;
        const DirectedEdgeUse& use = uses.front();
        const std::size_t from = static_cast<std::size_t>(use.from);
        const std::size_t to = static_cast<std::size_t>(use.to);
        if (!outgoing.emplace(from, to).second
            || !incoming.emplace(to, from).second)
        {
            SetBlocked(
                analysis,
                "blocked_boundary_topology",
                MeshRepairErrorCode::AmbiguousTopology);
            return analysis;
        }
        neighbors[from].insert(to);
        neighbors[to].insert(from);
        adjacentTriangleByEdge.emplace(edge, use.triangleIndex);
    }
    if (analysis.boundaryEdgeCount == 0U)
    {
        return analysis;
    }
    if (AnalyzeMeshTopology(request.mesh->mesh).non_manifold_edges > 0U)
    {
        SetBlocked(
            analysis,
            "blocked_boundary_topology",
            MeshRepairErrorCode::AmbiguousTopology);
        return analysis;
    }
    for (const auto& [vertex, connected] : neighbors)
    {
        if (connected.size() != 2U
            || outgoing.find(vertex) == outgoing.end()
            || incoming.find(vertex) == incoming.end())
        {
            SetBlocked(
                analysis,
                "blocked_boundary_topology",
                MeshRepairErrorCode::AmbiguousTopology);
            return analysis;
        }
    }

    std::set<std::size_t> unvisited;
    for (const auto& [vertex, connected] : neighbors)
    {
        (void)connected;
        unvisited.insert(vertex);
    }
    while (!unvisited.empty())
    {
        BoundaryLoop loop;
        const std::size_t start = *unvisited.begin();
        std::size_t current = start;
        for (std::size_t step{0U}; step <= analysis.boundaryEdgeCount; ++step)
        {
            if (current != start && unvisited.find(current) == unvisited.end())
            {
                SetBlocked(
                    analysis,
                    "blocked_boundary_topology",
                    MeshRepairErrorCode::AmbiguousTopology);
                return analysis;
            }
            loop.vertices.push_back(current);
            unvisited.erase(current);
            const std::size_t next = outgoing.at(current);
            loop.adjacentTriangleIndices.push_back(
                adjacentTriangleByEdge.at(MakeEdgeKey(
                    static_cast<int>(current),
                    static_cast<int>(next))));
            if (next == start)
            {
                break;
            }
            current = next;
        }
        if (loop.vertices.size() < 3U
            || outgoing.at(loop.vertices.back()) != start)
        {
            SetBlocked(
                analysis,
                "blocked_boundary_topology",
                MeshRepairErrorCode::AmbiguousTopology);
            return analysis;
        }
        if (!ComputeLoopMetrics(
                request.mesh->mesh,
                loop,
                request.robustnessOptions.tolerance.area_epsilon_mm2))
        {
            SetBlocked(
                analysis,
                "blocked_boundary_topology",
                MeshRepairErrorCode::AmbiguousTopology);
            return analysis;
        }
        analysis.loops.push_back(std::move(loop));
    }
    return analysis;
}

bool ValidateLoopBudgets(
    const MeshRepairBoundaryOperationRequest& request,
    const BoundaryLoop& loop,
    const std::size_t totalGeneratedFaces)
{
    const double generatedFaceRatio = static_cast<double>(totalGeneratedFaces)
        / static_cast<double>(request.mesh->mesh.triangles.size());
    return loop.vertices.size() <= request.options.maxBoundaryLoopEdges
        && loop.diameterMm <= request.options.maxBoundaryLoopDiameterMm
        && loop.perimeterMm <= request.options.maxBoundaryLoopPerimeterMm
        && loop.areaMm2 <= request.options.maxHoleAreaMm2
        && generatedFaceRatio <= request.options.maxAffectedFaceRatio;
}

bool ResolveUniformAttributes(
    const AdaptedTriangleMesh& mesh,
    const BoundaryLoop& loop,
    std::string& materialName)
{
    if (loop.adjacentTriangleIndices.empty())
    {
        return false;
    }
    materialName = mesh.triangle_attributes
        .at(loop.adjacentTriangleIndices.front())
        .material_name;
    if (materialName.empty())
    {
        return false;
    }
    for (const std::size_t triangleIndex : loop.adjacentTriangleIndices)
    {
        const SurfaceTriangleAttributes& attributes =
            mesh.triangle_attributes.at(triangleIndex);
        if (attributes.has_uv || attributes.material_name != materialName)
        {
            return false;
        }
    }
    return true;
}

std::vector<std::size_t> BuildFillOrder(const BoundaryLoop& loop)
{
    std::vector<std::size_t> order(loop.vertices.rbegin(), loop.vertices.rend());
    const auto minimum = std::min_element(order.begin(), order.end());
    std::rotate(order.begin(), minimum, order.end());
    return order;
}

std::size_t NextGeneratedSourceId(const AdaptedTriangleMesh& mesh)
{
    std::size_t nextId{0U};
    for (const SurfaceTriangleAttributes& attributes : mesh.triangle_attributes)
    {
        nextId = std::max(nextId, attributes.source_triangle_index + 1U);
    }
    for (const std::size_t sourceIndex : mesh.rejected_degenerate_source_triangle_indices)
    {
        nextId = std::max(nextId, sourceIndex + 1U);
    }
    return nextId;
}

void AppendLoopFill(
    const MeshRepairBoundaryOperationRequest& request,
    const BoundaryLoop& loop,
    const std::string& materialName,
    AdaptedTriangleMesh& candidate,
    std::vector<MeshRepairGeneratedTriangleMapping>& mappings,
    std::vector<MeshRepairOperation>& operations,
    std::size_t& generatedSourceId,
    std::uint64_t& operationId)
{
    const std::vector<std::size_t> order = BuildFillOrder(loop);
    MeshRepairOperation operation;
    operation.operationId = operationId++;
    operation.type = MeshRepairOperationType::FillBoundaryLoop;
    operation.reasonCode = "MESH_BOUNDARY_EDGES";
    for (const std::size_t vertexIndex : loop.vertices)
    {
        operation.inputElementIds.push_back(vertexIndex);
    }
    for (std::size_t index{1U}; index + 1U < order.size(); ++index)
    {
        const std::array<int, 3> triangle{
            static_cast<int>(order.front()),
            static_cast<int>(order.at(index)),
            static_cast<int>(order.at(index + 1U))};
        const std::size_t outputTriangleIndex = candidate.mesh.triangles.size();
        candidate.mesh.triangles.push_back(triangle);
        SurfaceTriangleAttributes attributes;
        attributes.source_triangle_index = generatedSourceId++;
        attributes.has_uv = false;
        attributes.material_name = materialName;
        candidate.triangle_attributes.push_back(attributes);

        MeshRepairGeneratedTriangleMapping mapping;
        mapping.outputTriangleIndex = outputTriangleIndex;
        mapping.generatingBoundaryVertexIndices = {
            static_cast<std::uint64_t>(triangle.at(0U)),
            static_cast<std::uint64_t>(triangle.at(1U)),
            static_cast<std::uint64_t>(triangle.at(2U))};
        mapping.attributePolicy = request.options.newFaceAttributePolicy;
        mapping.materialName = materialName;
        mapping.hasUv = false;
        mappings.push_back(std::move(mapping));
        operation.outputElementIds.push_back(outputTriangleIndex);
    }
    operation.parameters = Json::object({
        {"edgeCount", loop.vertices.size()},
        {"diameterMm", loop.diameterMm},
        {"perimeterMm", loop.perimeterMm},
        {"planarityErrorMm", loop.planarityErrorMm},
        {"areaMm2", loop.areaMm2},
        {"attributePolicy", request.options.newFaceAttributePolicy},
        {"materialName", materialName},
    });
    operation.attributeDecision = MeshRepairAttributeDecision::GeneratedByPolicy;
    operation.affectedVertices = loop.vertices.size();
    operation.affectedEdges = loop.vertices.size();
    operation.affectedFaces = order.size() - 2U;
    operations.push_back(std::move(operation));
}

bool ValidateFilledCandidate(
    const AdaptedTriangleMesh& before,
    const AdaptedTriangleMesh& after,
    const MeshRobustnessOptions& options,
    const std::size_t filledBoundaryEdges)
{
    const MeshTopologyReport beforeTopology = AnalyzeMeshTopology(before.mesh);
    const MeshTopologyReport afterTopology = AnalyzeMeshTopology(after.mesh);
    const MeshRobustnessReport beforeRobustness = AnalyzeMeshRobustness(before.mesh, options);
    const MeshRobustnessReport afterRobustness = AnalyzeMeshRobustness(after.mesh, options);
    if (beforeRobustness.self_intersection_check_sampled
        || afterRobustness.self_intersection_check_sampled)
    {
        return false;
    }
    return beforeTopology.boundary_edges >= filledBoundaryEdges
        && afterTopology.boundary_edges
            == beforeTopology.boundary_edges - filledBoundaryEdges
        && afterTopology.non_manifold_edges <= beforeTopology.non_manifold_edges
        && afterTopology.degenerate_triangles == 0U
        && afterRobustness.connected_components == beforeRobustness.connected_components
        && afterRobustness.duplicate_faces <= beforeRobustness.duplicate_faces
        && afterRobustness.opposite_duplicate_faces
            <= beforeRobustness.opposite_duplicate_faces
        && afterRobustness.inconsistent_oriented_edges
            <= beforeRobustness.inconsistent_oriented_edges
        && afterRobustness.confirmed_self_intersections
            <= beforeRobustness.confirmed_self_intersections;
}

void ValidateRequest(const MeshRepairBoundaryOperationRequest& request)
{
    if (request.mesh == nullptr)
    {
        throw MeshRepairError(
            MeshRepairErrorCode::InputInvalid,
            "mesh repair boundary operations require an adapted mesh");
    }
    if (request.mesh->triangle_attributes.size() != request.mesh->mesh.triangles.size())
    {
        throw MeshRepairError(
            MeshRepairErrorCode::AttributeMismatch,
            "mesh repair boundary operations require one attribute record per triangle");
    }
    if (!request.options.allowBoundaryFill)
    {
        return;
    }
    const bool validFiniteBudgets =
        std::isfinite(request.options.maxBoundaryLoopDiameterMm)
        && std::isfinite(request.options.maxBoundaryLoopPerimeterMm)
        && std::isfinite(request.options.maxBoundaryPlanarityErrorMm)
        && std::isfinite(request.options.maxHoleAreaMm2)
        && std::isfinite(request.options.maxAffectedFaceRatio);
    if (!request.options.allowNewFaces
        || request.options.maxBoundaryLoopEdges < 3U
        || !validFiniteBudgets
        || !(request.options.maxBoundaryLoopDiameterMm > 0.0)
        || !(request.options.maxBoundaryLoopPerimeterMm > 0.0)
        || !(request.options.maxBoundaryPlanarityErrorMm > 0.0)
        || !(request.options.maxHoleAreaMm2 > 0.0)
        || !(request.options.maxAffectedFaceRatio > 0.0)
        || request.options.maxAffectedFaceRatio > 1.0)
    {
        throw MeshRepairError(
            MeshRepairErrorCode::BudgetExceeded,
            "boundary fill requires explicit positive geometry budgets");
    }
    if (request.options.newFaceAttributePolicy != "inherit_uniform_material_no_uv")
    {
        throw MeshRepairError(
            MeshRepairErrorCode::AttributeMismatch,
            "boundary fill requires inherit_uniform_material_no_uv policy");
    }
}

}  // namespace

MeshRepairBoundaryOperationResult ExecuteMeshRepairBoundaryOperations(
    const MeshRepairBoundaryOperationRequest& request)
{
    ValidateRequest(request);
    MeshRepairBoundaryOperationResult result;
    result.candidate = *request.mesh;
    if (!request.options.allowBoundaryFill)
    {
        return result;
    }

    const MeshRobustnessReport beforeRobustness = AnalyzeMeshRobustness(
        request.mesh->mesh,
        request.robustnessOptions);
    const std::map<EdgeKey, std::vector<DirectedEdgeUse>> edgeUses =
        BuildEdgeUses(request.mesh->mesh);
    const bool hasBoundary = std::any_of(
        edgeUses.begin(),
        edgeUses.end(),
        [](const auto& entry)
        {
            return entry.second.size() == 1U;
        });
    if (!hasBoundary)
    {
        return result;
    }
    if (beforeRobustness.self_intersection_check_sampled)
    {
        result.blocked = true;
        result.attributeStatus = "blocked_boundary_intersection_evidence";
        result.blockerCode = MeshRepairErrorCodeName(MeshRepairErrorCode::PostStrictFailed);
        return result;
    }
    if (beforeRobustness.confirmed_self_intersections > 0U)
    {
        result.blocked = true;
        result.attributeStatus = "blocked_boundary_self_intersection";
        result.blockerCode = MeshRepairErrorCodeName(MeshRepairErrorCode::SelfIntersection);
        return result;
    }

    BoundaryAnalysis analysis = AnalyzeBoundaryLoops(request, edgeUses);
    if (analysis.blocked)
    {
        result.blocked = true;
        result.attributeStatus = std::move(analysis.attributeStatus);
        result.blockerCode = std::move(analysis.blockerCode);
        return result;
    }

    std::size_t totalGeneratedFaces{0U};
    for (const BoundaryLoop& loop : analysis.loops)
    {
        totalGeneratedFaces += loop.vertices.size() - 2U;
    }
    for (const BoundaryLoop& loop : analysis.loops)
    {
        if (loop.planarityErrorMm > request.options.maxBoundaryPlanarityErrorMm)
        {
            result.blocked = true;
            result.attributeStatus = "blocked_boundary_planarity";
            result.blockerCode = MeshRepairErrorCodeName(MeshRepairErrorCode::AmbiguousTopology);
            return result;
        }
        if (!ValidateLoopBudgets(request, loop, totalGeneratedFaces))
        {
            result.blocked = true;
            result.attributeStatus = "blocked_boundary_budget";
            result.blockerCode = MeshRepairErrorCodeName(MeshRepairErrorCode::BudgetExceeded);
            return result;
        }
        std::string materialName;
        if (!ResolveUniformAttributes(*request.mesh, loop, materialName))
        {
            result.blocked = true;
            result.attributeStatus = "blocked_boundary_attribute_policy";
            result.blockerCode = MeshRepairErrorCodeName(MeshRepairErrorCode::AttributeMismatch);
            return result;
        }
    }

    AdaptedTriangleMesh working = *request.mesh;
    std::size_t generatedSourceId = NextGeneratedSourceId(working);
    std::uint64_t operationId = request.firstOperationId;
    for (const BoundaryLoop& loop : analysis.loops)
    {
        std::string materialName;
        static_cast<void>(ResolveUniformAttributes(*request.mesh, loop, materialName));
        AppendLoopFill(
            request,
            loop,
            materialName,
            working,
            result.generatedTriangleMappings,
            result.operations,
            generatedSourceId,
            operationId);
    }
    working.topology = AnalyzeMeshTopology(working.mesh);
    working.topology.source_triangles = request.mesh->topology.source_triangles;
    working.topology.degenerate_triangles = request.mesh->topology.degenerate_triangles;
    if (!ValidateFilledCandidate(
            *request.mesh,
            working,
            request.robustnessOptions,
            analysis.boundaryEdgeCount))
    {
        result.blocked = true;
        result.attributeStatus = "blocked_boundary_post_guard";
        result.blockerCode = MeshRepairErrorCodeName(MeshRepairErrorCode::PostStrictFailed);
        result.operations.clear();
        result.generatedTriangleMappings.clear();
        return result;
    }

    result.candidate = std::move(working);
    return result;
}

}  // namespace slicer_core
