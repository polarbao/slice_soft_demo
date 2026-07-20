#include "slicer_core/geometry/repair/MeshRepairTopologyOperations.h"

#include "slicer_core/geometry/MeshTopologyDiagnostics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

using EdgeKey = std::pair<int, int>;
using BucketKey = std::tuple<long long, long long, long long>;

struct DirectedEdgeUse
{
    std::size_t triangleIndex{0U};
    int from{0};
    int to{0};
};

struct WindingConstraint
{
    std::size_t triangleIndex{0U};
    bool flipParity{false};
};

class DisjointSet
{
public:
    explicit DisjointSet(const std::size_t size)
        : m_parent(size),
          m_rank(size, 0U)
    {
        std::iota(m_parent.begin(), m_parent.end(), 0U);
    }

    std::size_t Find(const std::size_t value)
    {
        if (m_parent.at(value) != value)
        {
            m_parent.at(value) = Find(m_parent.at(value));
        }
        return m_parent.at(value);
    }

    void Unite(const std::size_t left, const std::size_t right)
    {
        std::size_t leftRoot = Find(left);
        std::size_t rightRoot = Find(right);
        if (leftRoot == rightRoot)
        {
            return;
        }
        if (m_rank.at(leftRoot) < m_rank.at(rightRoot))
        {
            std::swap(leftRoot, rightRoot);
        }
        m_parent.at(rightRoot) = leftRoot;
        if (m_rank.at(leftRoot) == m_rank.at(rightRoot))
        {
            ++m_rank.at(leftRoot);
        }
    }

private:
    std::vector<std::size_t> m_parent;
    std::vector<std::uint8_t> m_rank;
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

double DistanceSquared(const Vec3& left, const Vec3& right)
{
    const double dx = left.x - right.x;
    const double dy = left.y - right.y;
    const double dz = left.z - right.z;
    return dx * dx + dy * dy + dz * dz;
}

double TriangleAreaMm2(
    const TriangleMeshData& mesh,
    const std::array<int, 3>& triangle)
{
    const Vec3& a = mesh.vertices.at(static_cast<std::size_t>(triangle.at(0U)));
    const Vec3& b = mesh.vertices.at(static_cast<std::size_t>(triangle.at(1U)));
    const Vec3& c = mesh.vertices.at(static_cast<std::size_t>(triangle.at(2U)));
    const double abX = b.x - a.x;
    const double abY = b.y - a.y;
    const double abZ = b.z - a.z;
    const double acX = c.x - a.x;
    const double acY = c.y - a.y;
    const double acZ = c.z - a.z;
    const double crossX = abY * acZ - abZ * acY;
    const double crossY = abZ * acX - abX * acZ;
    const double crossZ = abX * acY - abY * acX;
    return 0.5 * std::sqrt(crossX * crossX + crossY * crossY + crossZ * crossZ);
}

double SignedVolume(
    const TriangleMeshData& mesh,
    const std::vector<std::size_t>& triangleIndices)
{
    double volume{0.0};
    for (const std::size_t triangleIndex : triangleIndices)
    {
        const std::array<int, 3>& triangle = mesh.triangles.at(triangleIndex);
        const Vec3& a = mesh.vertices.at(static_cast<std::size_t>(triangle.at(0U)));
        const Vec3& b = mesh.vertices.at(static_cast<std::size_t>(triangle.at(1U)));
        const Vec3& c = mesh.vertices.at(static_cast<std::size_t>(triangle.at(2U)));
        volume +=
            (a.x * (b.y * c.z - b.z * c.y)
             - a.y * (b.x * c.z - b.z * c.x)
             + a.z * (b.x * c.y - b.y * c.x))
            / 6.0;
    }
    return volume;
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

std::vector<std::size_t> BuildTriangleComponents(
    const TriangleMeshData& mesh,
    std::size_t& componentCount)
{
    DisjointSet sets(mesh.triangles.size());
    const std::map<EdgeKey, std::vector<DirectedEdgeUse>> edgeUses = BuildEdgeUses(mesh);
    for (const auto& [edge, uses] : edgeUses)
    {
        (void)edge;
        for (std::size_t index{1U}; index < uses.size(); ++index)
        {
            sets.Unite(uses.front().triangleIndex, uses.at(index).triangleIndex);
        }
    }

    std::map<std::size_t, std::size_t> componentByRoot;
    std::vector<std::size_t> components(mesh.triangles.size(), 0U);
    for (std::size_t triangleIndex{0U}; triangleIndex < mesh.triangles.size(); ++triangleIndex)
    {
        const std::size_t root = sets.Find(triangleIndex);
        const auto [found, inserted] = componentByRoot.emplace(root, componentByRoot.size());
        (void)inserted;
        components.at(triangleIndex) = found->second;
    }
    componentCount = componentByRoot.size();
    return components;
}

std::vector<std::set<std::size_t>> BuildVertexComponents(
    const TriangleMeshData& mesh,
    const std::vector<std::size_t>& triangleComponents)
{
    std::vector<std::set<std::size_t>> components(mesh.vertices.size());
    for (std::size_t triangleIndex{0U}; triangleIndex < mesh.triangles.size(); ++triangleIndex)
    {
        for (const int vertexIndex : mesh.triangles.at(triangleIndex))
        {
            components.at(static_cast<std::size_t>(vertexIndex)).insert(
                triangleComponents.at(triangleIndex));
        }
    }
    return components;
}

BucketKey MakeBucketKey(const Vec3& vertex, const double toleranceMm)
{
    const double x = std::floor(vertex.x / toleranceMm);
    const double y = std::floor(vertex.y / toleranceMm);
    const double z = std::floor(vertex.z / toleranceMm);
    const double limit = static_cast<double>(std::numeric_limits<long long>::max() / 2LL);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
        || std::abs(x) > limit || std::abs(y) > limit || std::abs(z) > limit)
    {
        throw MeshRepairError(
            MeshRepairErrorCode::InputInvalid,
            "vertex weld quantization exceeded its finite coordinate range");
    }
    return {
        static_cast<long long>(x),
        static_cast<long long>(y),
        static_cast<long long>(z)};
}

std::vector<MeshRepairVertexMapping> BuildIdentityVertexMappings(
    const std::size_t vertexCount)
{
    std::vector<MeshRepairVertexMapping> mappings;
    mappings.reserve(vertexCount);
    for (std::size_t index{0U}; index < vertexCount; ++index)
    {
        MeshRepairVertexMapping mapping;
        mapping.outputVertexIndex = index;
        mapping.sourceVertexIndices.push_back(index);
        mappings.push_back(std::move(mapping));
    }
    return mappings;
}

bool IsWeldCandidateSafe(
    const AdaptedTriangleMesh& before,
    const AdaptedTriangleMesh& after,
    const MeshRobustnessOptions& options)
{
    for (const std::array<int, 3>& triangle : after.mesh.triangles)
    {
        if (triangle.at(0U) == triangle.at(1U)
            || triangle.at(1U) == triangle.at(2U)
            || triangle.at(0U) == triangle.at(2U)
            || TriangleAreaMm2(after.mesh, triangle) <= options.tolerance.area_epsilon_mm2)
        {
            return false;
        }
    }

    const MeshTopologyReport beforeTopology = AnalyzeMeshTopology(before.mesh);
    const MeshTopologyReport afterTopology = AnalyzeMeshTopology(after.mesh);
    const MeshRobustnessReport beforeRobustness = AnalyzeMeshRobustness(before.mesh, options);
    const MeshRobustnessReport afterRobustness = AnalyzeMeshRobustness(after.mesh, options);
    if (beforeRobustness.self_intersection_check_sampled
        || afterRobustness.self_intersection_check_sampled)
    {
        return false;
    }
    return afterRobustness.connected_components == beforeRobustness.connected_components
        && afterTopology.boundary_edges <= beforeTopology.boundary_edges
        && afterTopology.non_manifold_edges <= beforeTopology.non_manifold_edges
        && afterRobustness.duplicate_faces <= beforeRobustness.duplicate_faces
        && afterRobustness.opposite_duplicate_faces <= beforeRobustness.opposite_duplicate_faces
        && afterRobustness.inconsistent_oriented_edges <= beforeRobustness.inconsistent_oriented_edges
        && afterRobustness.confirmed_self_intersections
            <= beforeRobustness.confirmed_self_intersections;
}

bool ApplyVertexWeld(
    const MeshRepairTopologyOperationRequest& request,
    AdaptedTriangleMesh& candidate,
    std::vector<MeshRepairOperation>& operations,
    std::vector<MeshRepairVertexMapping>& vertexMappings,
    std::uint64_t& operationId)
{
    vertexMappings = BuildIdentityVertexMappings(candidate.mesh.vertices.size());
    if (!request.options.allowVertexWeld || !(request.options.weldToleranceMm > 0.0))
    {
        return true;
    }

    const double toleranceMm = std::max(
        request.options.weldToleranceMm,
        request.robustnessOptions.tolerance.position_epsilon_mm);
    const double toleranceSquared = toleranceMm * toleranceMm;
    std::size_t componentCount{0U};
    const std::vector<std::size_t> triangleComponents =
        BuildTriangleComponents(candidate.mesh, componentCount);
    const std::vector<std::set<std::size_t>> vertexComponents =
        BuildVertexComponents(candidate.mesh, triangleComponents);

    std::vector<std::size_t> rootByVertex(candidate.mesh.vertices.size(), 0U);
    std::map<BucketKey, std::vector<std::size_t>> rootsByBucket;
    for (std::size_t vertexIndex{0U}; vertexIndex < candidate.mesh.vertices.size(); ++vertexIndex)
    {
        const Vec3& vertex = candidate.mesh.vertices.at(vertexIndex);
        const BucketKey bucket = MakeBucketKey(vertex, toleranceMm);
        const auto [bucketX, bucketY, bucketZ] = bucket;
        std::optional<std::size_t> selectedRoot;
        if (vertexComponents.at(vertexIndex).size() == 1U)
        {
            for (long long offsetX{-1}; offsetX <= 1; ++offsetX)
            {
                for (long long offsetY{-1}; offsetY <= 1; ++offsetY)
                {
                    for (long long offsetZ{-1}; offsetZ <= 1; ++offsetZ)
                    {
                        const auto found = rootsByBucket.find({
                            bucketX + offsetX,
                            bucketY + offsetY,
                            bucketZ + offsetZ});
                        if (found == rootsByBucket.end())
                        {
                            continue;
                        }
                        for (const std::size_t rootIndex : found->second)
                        {
                            if (vertexComponents.at(rootIndex) != vertexComponents.at(vertexIndex)
                                || DistanceSquared(
                                       candidate.mesh.vertices.at(rootIndex),
                                       vertex) > toleranceSquared)
                            {
                                continue;
                            }
                            if (!selectedRoot.has_value() || rootIndex < selectedRoot.value())
                            {
                                selectedRoot = rootIndex;
                            }
                        }
                    }
                }
            }
        }

        const std::size_t root = selectedRoot.value_or(vertexIndex);
        rootByVertex.at(vertexIndex) = root;
        if (root == vertexIndex)
        {
            rootsByBucket[bucket].push_back(vertexIndex);
        }
    }

    bool hasWeld{false};
    for (std::size_t vertexIndex{0U}; vertexIndex < rootByVertex.size(); ++vertexIndex)
    {
        if (rootByVertex.at(vertexIndex) != vertexIndex)
        {
            hasWeld = true;
            break;
        }
    }
    if (!hasWeld)
    {
        return true;
    }

    AdaptedTriangleMesh welded = candidate;
    welded.mesh.vertices.clear();
    std::map<std::size_t, std::size_t> outputByRoot;
    for (std::size_t sourceIndex{0U}; sourceIndex < rootByVertex.size(); ++sourceIndex)
    {
        const std::size_t root = rootByVertex.at(sourceIndex);
        if (outputByRoot.find(root) == outputByRoot.end())
        {
            const std::size_t outputIndex = welded.mesh.vertices.size();
            outputByRoot.emplace(root, outputIndex);
            welded.mesh.vertices.push_back(candidate.mesh.vertices.at(root));
        }
    }
    for (std::array<int, 3>& triangle : welded.mesh.triangles)
    {
        for (int& vertexIndex : triangle)
        {
            const std::size_t sourceIndex = static_cast<std::size_t>(vertexIndex);
            vertexIndex = static_cast<int>(outputByRoot.at(rootByVertex.at(sourceIndex)));
        }
    }
    welded.topology = AnalyzeMeshTopology(welded.mesh);
    welded.topology.source_triangles = candidate.topology.source_triangles;
    welded.topology.degenerate_triangles = candidate.topology.degenerate_triangles;
    if (!IsWeldCandidateSafe(candidate, welded, request.robustnessOptions))
    {
        return false;
    }

    vertexMappings.assign(welded.mesh.vertices.size(), {});
    for (std::size_t sourceIndex{0U}; sourceIndex < rootByVertex.size(); ++sourceIndex)
    {
        const std::size_t outputIndex = outputByRoot.at(rootByVertex.at(sourceIndex));
        MeshRepairVertexMapping& mapping = vertexMappings.at(outputIndex);
        mapping.outputVertexIndex = outputIndex;
        mapping.sourceVertexIndices.push_back(sourceIndex);
    }
    for (const MeshRepairVertexMapping& mapping : vertexMappings)
    {
        if (mapping.sourceVertexIndices.size() <= 1U)
        {
            continue;
        }
        MeshRepairOperation operation;
        operation.operationId = operationId++;
        operation.type = MeshRepairOperationType::WeldVertex;
        operation.reasonCode = "MESH_VERTEX_WELD_CANDIDATE";
        operation.inputElementIds = mapping.sourceVertexIndices;
        operation.outputElementIds = {mapping.outputVertexIndex};
        operation.parameters = Json::object({
            {"weldToleranceMm", request.options.weldToleranceMm},
            {"effectiveToleranceMm", toleranceMm},
            {"componentCountBefore", componentCount},
            {"componentCountAfter", componentCount},
        });
        operation.attributeDecision = MeshRepairAttributeDecision::Preserved;
        operation.affectedVertices = mapping.sourceVertexIndices.size();
        operations.push_back(std::move(operation));
    }
    candidate = std::move(welded);
    return true;
}

bool ApplyWindingRepair(
    const MeshRepairTopologyOperationRequest& request,
    AdaptedTriangleMesh& candidate,
    std::vector<MeshRepairOperation>& operations,
    std::uint64_t& operationId)
{
    if (!request.options.allowWindingRepair)
    {
        return true;
    }

    const MeshRobustnessReport before = AnalyzeMeshRobustness(
        candidate.mesh,
        request.robustnessOptions);
    if (before.inconsistent_oriented_edges == 0U)
    {
        return true;
    }
    if (candidate.topology.non_manifold_edges > 0U)
    {
        return false;
    }

    const std::map<EdgeKey, std::vector<DirectedEdgeUse>> edgeUses =
        BuildEdgeUses(candidate.mesh);
    std::vector<std::vector<WindingConstraint>> adjacency(candidate.mesh.triangles.size());
    for (const auto& [edge, uses] : edgeUses)
    {
        (void)edge;
        if (uses.size() != 2U)
        {
            continue;
        }
        const bool sameDirection = uses.at(0U).from == uses.at(1U).from
            && uses.at(0U).to == uses.at(1U).to;
        adjacency.at(uses.at(0U).triangleIndex).push_back(
            {uses.at(1U).triangleIndex, sameDirection});
        adjacency.at(uses.at(1U).triangleIndex).push_back(
            {uses.at(0U).triangleIndex, sameDirection});
    }

    std::vector<int> parity(candidate.mesh.triangles.size(), -1);
    std::vector<std::size_t> trianglesToFlip;
    for (std::size_t seed{0U}; seed < candidate.mesh.triangles.size(); ++seed)
    {
        if (parity.at(seed) >= 0)
        {
            continue;
        }
        parity.at(seed) = 0;
        std::vector<std::size_t> stack{seed};
        std::vector<std::size_t> component;
        while (!stack.empty())
        {
            const std::size_t current = stack.back();
            stack.pop_back();
            component.push_back(current);
            for (const WindingConstraint& constraint : adjacency.at(current))
            {
                const int expected = parity.at(current) ^ (constraint.flipParity ? 1 : 0);
                if (parity.at(constraint.triangleIndex) < 0)
                {
                    parity.at(constraint.triangleIndex) = expected;
                    stack.push_back(constraint.triangleIndex);
                }
                else if (parity.at(constraint.triangleIndex) != expected)
                {
                    return false;
                }
            }
        }

        std::size_t flipCount = 0U;
        for (const std::size_t triangleIndex : component)
        {
            flipCount += parity.at(triangleIndex) == 1 ? 1U : 0U;
        }
        if (flipCount > 0U && flipCount * 2U == component.size())
        {
            return false;
        }
        const bool invert = flipCount * 2U > component.size();
        const std::size_t selectedFlipCount = invert
            ? component.size() - flipCount
            : flipCount;
        if (selectedFlipCount > 0U
            && std::abs(SignedVolume(candidate.mesh, component))
                <= request.robustnessOptions.tolerance.area_epsilon_mm2)
        {
            return false;
        }
        for (const std::size_t triangleIndex : component)
        {
            const bool shouldFlip = (parity.at(triangleIndex) == 1) != invert;
            if (shouldFlip)
            {
                trianglesToFlip.push_back(triangleIndex);
            }
        }
    }

    if (trianglesToFlip.empty())
    {
        return true;
    }
    std::sort(trianglesToFlip.begin(), trianglesToFlip.end());
    AdaptedTriangleMesh oriented = candidate;
    for (const std::size_t triangleIndex : trianglesToFlip)
    {
        std::swap(
            oriented.mesh.triangles.at(triangleIndex).at(1U),
            oriented.mesh.triangles.at(triangleIndex).at(2U));
        std::swap(
            oriented.triangle_attributes.at(triangleIndex).uv.at(1U),
            oriented.triangle_attributes.at(triangleIndex).uv.at(2U));
    }
    oriented.topology = AnalyzeMeshTopology(oriented.mesh);
    oriented.topology.source_triangles = candidate.topology.source_triangles;
    oriented.topology.degenerate_triangles = candidate.topology.degenerate_triangles;
    const MeshRobustnessReport after = AnalyzeMeshRobustness(
        oriented.mesh,
        request.robustnessOptions);
    if (after.inconsistent_oriented_edges != 0U
        || after.connected_components != before.connected_components
        || oriented.topology.boundary_edges != candidate.topology.boundary_edges
        || oriented.topology.non_manifold_edges != candidate.topology.non_manifold_edges
        || after.duplicate_faces != before.duplicate_faces
        || after.opposite_duplicate_faces != before.opposite_duplicate_faces)
    {
        return false;
    }

    for (const std::size_t triangleIndex : trianglesToFlip)
    {
        MeshRepairOperation operation;
        operation.operationId = operationId++;
        operation.type = MeshRepairOperationType::FlipTriangleWinding;
        operation.reasonCode = "MESH_LOCAL_WINDING";
        operation.inputElementIds = {
            candidate.triangle_attributes.at(triangleIndex).source_triangle_index};
        operation.outputElementIds = {triangleIndex};
        operation.parameters = Json::object({
            {"outputTriangleIndex", triangleIndex},
            {"uvCornerSwap", "1<->2"},
        });
        operation.attributeDecision = MeshRepairAttributeDecision::Preserved;
        operation.affectedFaces = 1U;
        operations.push_back(std::move(operation));
    }
    candidate = std::move(oriented);
    return true;
}

void ValidateRequest(const MeshRepairTopologyOperationRequest& request)
{
    if (request.mesh == nullptr)
    {
        throw MeshRepairError(
            MeshRepairErrorCode::InputInvalid,
            "mesh repair topology operations require an adapted mesh");
    }
    if (request.mesh->triangle_attributes.size() != request.mesh->mesh.triangles.size())
    {
        throw MeshRepairError(
            MeshRepairErrorCode::AttributeMismatch,
            "mesh repair topology operations require one attribute record per triangle");
    }
    if (!std::isfinite(request.options.weldToleranceMm)
        || request.options.weldToleranceMm < 0.0)
    {
        throw MeshRepairError(
            MeshRepairErrorCode::InputInvalid,
            "mesh repair weld tolerance must be finite and non-negative");
    }
}

}  // namespace

MeshRepairTopologyOperationResult ExecuteMeshRepairTopologyOperations(
    const MeshRepairTopologyOperationRequest& request)
{
    ValidateRequest(request);
    MeshRepairTopologyOperationResult result;
    result.candidate = *request.mesh;
    result.vertexMappings = BuildIdentityVertexMappings(
        request.mesh->mesh.vertices.size());
    std::uint64_t operationId = request.firstOperationId;

    AdaptedTriangleMesh working = *request.mesh;
    std::vector<MeshRepairOperation> operations;
    std::vector<MeshRepairVertexMapping> mappings;
    if (!ApplyVertexWeld(
            request,
            working,
            operations,
            mappings,
            operationId))
    {
        result.blocked = true;
        result.attributeStatus = "blocked_vertex_weld_guard";
        result.blockerCode = "E_12E_REPAIR_OPERATION_FAILED";
        return result;
    }
    if (!ApplyWindingRepair(request, working, operations, operationId))
    {
        result.blocked = true;
        result.attributeStatus = "blocked_winding_ambiguity";
        result.blockerCode = "E_12E_REPAIR_AMBIGUOUS_TOPOLOGY";
        return result;
    }

    result.candidate = std::move(working);
    result.operations = std::move(operations);
    result.vertexMappings = std::move(mappings);
    return result;
}

}  // namespace slicer_core
