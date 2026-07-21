#include "slicer_core/geometry/repair/MeshNonManifoldPatternClassifier.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

using EdgeKey = std::pair<int, int>;
using FaceKey = std::array<int, 3>;

struct EdgeUse
{
    std::size_t triangleIndex{0U};
    bool forward{false};
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

struct EdgeAttributeSignature
{
    std::string materialName;
    bool hasUv{false};
    TexCoord lowUv;
    TexCoord highUv;
};

EdgeKey MakeEdgeKey(const int first, const int second)
{
    return std::minmax(first, second);
}

void ValidateMesh(const AdaptedTriangleMesh& mesh)
{
    if (mesh.mesh.triangles.size() != mesh.triangle_attributes.size())
    {
        throw MeshRepairError(
            MeshRepairErrorCode::AttributeMismatch,
            "non-manifold classification requires one attribute record per triangle");
    }
    for (const std::array<int, 3>& triangle : mesh.mesh.triangles)
    {
        for (const int vertexIndex : triangle)
        {
            if (vertexIndex < 0
                || static_cast<std::size_t>(vertexIndex) >= mesh.mesh.vertices.size())
            {
                throw MeshRepairError(
                    MeshRepairErrorCode::InputInvalid,
                    "non-manifold classification received an invalid vertex index");
            }
        }
    }
}

std::map<EdgeKey, std::vector<EdgeUse>> BuildEdgeUses(
    const TriangleMeshData& mesh)
{
    std::map<EdgeKey, std::vector<EdgeUse>> usesByEdge;
    for (std::size_t triangleIndex{0U};
         triangleIndex < mesh.triangles.size();
         ++triangleIndex)
    {
        const std::array<int, 3>& triangle = mesh.triangles.at(triangleIndex);
        for (std::size_t corner{0U}; corner < 3U; ++corner)
        {
            const int first = triangle.at(corner);
            const int second = triangle.at((corner + 1U) % 3U);
            const EdgeKey edge = MakeEdgeKey(first, second);
            usesByEdge[edge].push_back(EdgeUse{
                triangleIndex,
                first == edge.first && second == edge.second});
        }
    }
    return usesByEdge;
}

std::vector<std::size_t> BuildResidualComponents(
    const std::size_t triangleCount,
    const std::map<EdgeKey, std::vector<EdgeUse>>& usesByEdge)
{
    DisjointSet sets(triangleCount);
    for (const auto& [edge, uses] : usesByEdge)
    {
        (void)edge;
        if (uses.size() == 2U)
        {
            sets.Unite(uses.at(0U).triangleIndex, uses.at(1U).triangleIndex);
        }
    }

    std::map<std::size_t, std::size_t> minimumTriangleByRoot;
    for (std::size_t triangleIndex{0U}; triangleIndex < triangleCount; ++triangleIndex)
    {
        const std::size_t root = sets.Find(triangleIndex);
        const auto found = minimumTriangleByRoot.find(root);
        if (found == minimumTriangleByRoot.end())
        {
            minimumTriangleByRoot.emplace(root, triangleIndex);
        }
        else
        {
            found->second = std::min(found->second, triangleIndex);
        }
    }
    std::vector<std::pair<std::size_t, std::size_t>> orderedRoots;
    for (const auto& [root, minimumTriangle] : minimumTriangleByRoot)
    {
        orderedRoots.emplace_back(minimumTriangle, root);
    }
    std::sort(orderedRoots.begin(), orderedRoots.end());

    std::map<std::size_t, std::size_t> componentByRoot;
    for (std::size_t component{0U}; component < orderedRoots.size(); ++component)
    {
        componentByRoot.emplace(orderedRoots.at(component).second, component);
    }
    std::vector<std::size_t> components(triangleCount, 0U);
    for (std::size_t triangleIndex{0U}; triangleIndex < triangleCount; ++triangleIndex)
    {
        components.at(triangleIndex) = componentByRoot.at(sets.Find(triangleIndex));
    }
    return components;
}

TexCoord FindVertexUv(
    const std::array<int, 3>& triangle,
    const SurfaceTriangleAttributes& attributes,
    const int vertexIndex)
{
    for (std::size_t corner{0U}; corner < 3U; ++corner)
    {
        if (triangle.at(corner) == vertexIndex)
        {
            return attributes.uv.at(corner);
        }
    }
    throw MeshRepairError(
        MeshRepairErrorCode::InputInvalid,
        "non-manifold edge vertex is not present in its incident triangle");
}

EdgeAttributeSignature MakeAttributeSignature(
    const AdaptedTriangleMesh& mesh,
    const EdgeKey& edge,
    const EdgeUse& use)
{
    const std::array<int, 3>& triangle = mesh.mesh.triangles.at(use.triangleIndex);
    const SurfaceTriangleAttributes& attributes =
        mesh.triangle_attributes.at(use.triangleIndex);
    EdgeAttributeSignature signature;
    signature.materialName = attributes.material_name;
    signature.hasUv = attributes.has_uv;
    if (attributes.has_uv)
    {
        signature.lowUv = FindVertexUv(triangle, attributes, edge.first);
        signature.highUv = FindVertexUv(triangle, attributes, edge.second);
    }
    return signature;
}

bool EqualUv(const TexCoord& left, const TexCoord& right)
{
    return left.u == right.u && left.v == right.v;
}

bool EqualSignature(
    const EdgeAttributeSignature& left,
    const EdgeAttributeSignature& right)
{
    return left.materialName == right.materialName
        && left.hasUv == right.hasUv
        && (!left.hasUv
            || (EqualUv(left.lowUv, right.lowUv)
                && EqualUv(left.highUv, right.highUv)));
}

FaceKey MakeFaceKey(const std::array<int, 3>& triangle)
{
    FaceKey key = triangle;
    std::sort(key.begin(), key.end());
    return key;
}

bool HasDuplicateGeometry(
    const TriangleMeshData& mesh,
    const std::vector<EdgeUse>& uses)
{
    std::set<FaceKey> faces;
    for (const EdgeUse& use : uses)
    {
        if (!faces.insert(MakeFaceKey(mesh.triangles.at(use.triangleIndex))).second)
        {
            return true;
        }
    }
    return false;
}

bool HasAttributeConflict(
    const AdaptedTriangleMesh& mesh,
    const EdgeKey& edge,
    const std::vector<EdgeUse>& uses)
{
    const EdgeAttributeSignature reference = MakeAttributeSignature(
        mesh,
        edge,
        uses.front());
    return std::any_of(
        uses.begin() + 1,
        uses.end(),
        [&mesh, &edge, &reference](const EdgeUse& use)
        {
            return !EqualSignature(
                reference,
                MakeAttributeSignature(mesh, edge, use));
        });
}

bool GroupHasOppositePair(const std::vector<EdgeUse>& uses)
{
    return uses.size() == 2U && uses.at(0U).forward != uses.at(1U).forward;
}

void SetPattern(
    MeshNonManifoldEdgeAnalysis& edge,
    const MeshNonManifoldPattern pattern,
    const std::string& reasonCode)
{
    edge.pattern = pattern;
    edge.reasonCode = reasonCode;
}

MeshNonManifoldEdgeAnalysis ClassifyEdge(
    const AdaptedTriangleMesh& mesh,
    const EdgeKey& edgeKey,
    const std::vector<EdgeUse>& uses,
    const std::vector<std::size_t>& residualComponents)
{
    MeshNonManifoldEdgeAnalysis edge;
    edge.edgeVertexIndices = {
        static_cast<std::uint64_t>(edgeKey.first),
        static_cast<std::uint64_t>(edgeKey.second)};
    std::map<std::size_t, std::vector<EdgeUse>> usesByComponent;
    for (const EdgeUse& use : uses)
    {
        edge.incidentTriangleIndices.push_back(use.triangleIndex);
        edge.incidentSourceTriangleIndices.push_back(
            mesh.triangle_attributes.at(use.triangleIndex).source_triangle_index);
        usesByComponent[residualComponents.at(use.triangleIndex)].push_back(use);
        if (use.forward)
        {
            ++edge.forwardUses;
        }
        else
        {
            ++edge.reverseUses;
        }
    }
    std::sort(edge.incidentTriangleIndices.begin(), edge.incidentTriangleIndices.end());
    std::sort(
        edge.incidentSourceTriangleIndices.begin(),
        edge.incidentSourceTriangleIndices.end());
    for (const auto& [component, componentUses] : usesByComponent)
    {
        (void)componentUses;
        edge.residualComponentIds.push_back(component);
    }

    edge.duplicateGeometry = HasDuplicateGeometry(mesh.mesh, uses);
    edge.attributeConflict = HasAttributeConflict(mesh, edgeKey, uses);
    edge.mixedWinding = edge.forwardUses != edge.reverseUses
        || std::any_of(
            usesByComponent.begin(),
            usesByComponent.end(),
            [](const auto& item)
            {
                return item.second.size() == 2U
                    && !GroupHasOppositePair(item.second);
            });
    const bool residualPairsAreUnique = usesByComponent.size() > 1U
        && std::all_of(
            usesByComponent.begin(),
            usesByComponent.end(),
            [](const auto& item)
            {
                return GroupHasOppositePair(item.second);
            });

    if (edge.duplicateGeometry)
    {
        SetPattern(
            edge,
            MeshNonManifoldPattern::DuplicateShellOrExporterDuplicate,
            "MESH_NON_MANIFOLD_DUPLICATE_SHELL_OR_EXPORTER_DUPLICATE");
    }
    else if (edge.attributeConflict)
    {
        SetPattern(
            edge,
            MeshNonManifoldPattern::AttributeConflictingFan,
            "MESH_NON_MANIFOLD_ATTRIBUTE_CONFLICTING_FAN");
    }
    else if (edge.mixedWinding)
    {
        SetPattern(
            edge,
            MeshNonManifoldPattern::MixedWindingFan,
            "MESH_NON_MANIFOLD_MIXED_WINDING_FAN");
    }
    else if (residualPairsAreUnique)
    {
        edge.uniqueFanSplitFeasible = true;
        SetPattern(
            edge,
            MeshNonManifoldPattern::SeparableLocalEdgeFan,
            "MESH_NON_MANIFOLD_SEPARABLE_LOCAL_EDGE_FAN");
    }
    else if (usesByComponent.size() > 1U)
    {
        SetPattern(
            edge,
            MeshNonManifoldPattern::OverlappingComponent,
            "MESH_NON_MANIFOLD_OVERLAPPING_COMPONENT");
    }
    else
    {
        SetPattern(
            edge,
            MeshNonManifoldPattern::Unclassified,
            "MESH_NON_MANIFOLD_PATTERN_UNCLASSIFIED");
    }
    return edge;
}

void CountPattern(
    MeshNonManifoldAnalysis& analysis,
    const MeshNonManifoldPattern pattern)
{
    switch (pattern)
    {
    case MeshNonManifoldPattern::DuplicateShellOrExporterDuplicate:
        ++analysis.duplicateShellOrExporterDuplicateEdges;
        break;
    case MeshNonManifoldPattern::SeparableLocalEdgeFan:
        ++analysis.separableLocalEdgeFanEdges;
        break;
    case MeshNonManifoldPattern::OverlappingComponent:
        ++analysis.overlappingComponentEdges;
        break;
    case MeshNonManifoldPattern::MixedWindingFan:
        ++analysis.mixedWindingFanEdges;
        break;
    case MeshNonManifoldPattern::AttributeConflictingFan:
        ++analysis.attributeConflictingFanEdges;
        break;
    case MeshNonManifoldPattern::Unclassified:
        ++analysis.unclassifiedEdges;
        break;
    }
}

}  // namespace

MeshNonManifoldAnalysis ClassifyMeshNonManifoldPatterns(
    const AdaptedTriangleMesh& mesh)
{
    ValidateMesh(mesh);
    MeshNonManifoldAnalysis analysis;
    analysis.complete = true;
    const std::map<EdgeKey, std::vector<EdgeUse>> usesByEdge =
        BuildEdgeUses(mesh.mesh);
    const std::vector<std::size_t> residualComponents = BuildResidualComponents(
        mesh.mesh.triangles.size(),
        usesByEdge);

    for (const auto& [edgeKey, uses] : usesByEdge)
    {
        if (uses.size() <= 2U)
        {
            continue;
        }
        analysis.edges.push_back(ClassifyEdge(
            mesh,
            edgeKey,
            uses,
            residualComponents));
        CountPattern(analysis, analysis.edges.back().pattern);
    }
    analysis.nonManifoldEdgeCount = analysis.edges.size();
    if (analysis.edges.empty())
    {
        analysis.status = "not_present";
        analysis.allEdgesClassified = true;
        return analysis;
    }

    analysis.allEdgesClassified = analysis.unclassifiedEdges == 0U;
    analysis.allUniqueFanSplitsFeasible = analysis.separableLocalEdgeFanEdges
        == analysis.nonManifoldEdgeCount;
    analysis.status = analysis.allEdgesClassified
        ? "classified"
        : "classified_with_unknown";
    if (!analysis.allEdgesClassified)
    {
        analysis.issues.push_back(MakeValidationIssue(
            "MESH_NON_MANIFOLD_PATTERN_UNCLASSIFIED",
            ValidationSeverity::Warning,
            "one or more non-manifold edges have no proven structural pattern"));
    }
    return analysis;
}

}  // namespace slicer_core
