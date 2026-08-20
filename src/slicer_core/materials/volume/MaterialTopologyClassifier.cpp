#include "slicer_core/materials/volume/MaterialTopologyClassifier.h"

#include "slicer_core/geometry/MeshTopologyDiagnostics.h"
#include "slicer_core/geometry/repair/MeshCompleteSelfIntersectionAnalyzer.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

using EdgeKey = std::pair<int, int>;

EdgeKey MakeEdgeKey(const int left, const int right)
{
    return left <= right ? EdgeKey{left, right} : EdgeKey{right, left};
}

/// @brief 统计整网格每条边的入射面数，用于区分真开边与材质交界边。
std::map<EdgeKey, int> BuildWholeMeshEdgeIncidence(const TriangleMeshData& mesh)
{
    std::map<EdgeKey, int> incidence;
    for (const std::array<int, 3>& triangle : mesh.triangles)
    {
        ++incidence[MakeEdgeKey(triangle[0], triangle[1])];
        ++incidence[MakeEdgeKey(triangle[1], triangle[2])];
        ++incidence[MakeEdgeKey(triangle[2], triangle[0])];
    }
    return incidence;
}

/// @brief 从全网格中抽取指定三角面下标构成的子网格，并重映射顶点索引。
TriangleMeshData ExtractSubMesh(
    const TriangleMeshData& mesh,
    const std::vector<std::size_t>& triangleIndices)
{
    TriangleMeshData subMesh;
    subMesh.source_name = mesh.source_name;
    std::map<int, int> remap;
    subMesh.triangles.reserve(triangleIndices.size());
    for (const std::size_t triangleIndex : triangleIndices)
    {
        const std::array<int, 3>& source = mesh.triangles.at(triangleIndex);
        std::array<int, 3> mapped{0, 0, 0};
        for (std::size_t corner{0}; corner < 3U; ++corner)
        {
            const int sourceVertex = source.at(corner);
            const auto found = remap.find(sourceVertex);
            if (found != remap.end())
            {
                mapped.at(corner) = found->second;
                continue;
            }
            const int nextIndex = static_cast<int>(subMesh.vertices.size());
            subMesh.vertices.push_back(mesh.vertices.at(static_cast<std::size_t>(sourceVertex)));
            remap.emplace(sourceVertex, nextIndex);
            mapped.at(corner) = nextIndex;
        }
        subMesh.triangles.push_back(mapped);
    }
    return subMesh;
}

}  // namespace

std::string MaterialTopologyKindName(const MaterialTopologyKind kind)
{
    switch (kind)
    {
        case MaterialTopologyKind::ClosedOrientable:
            return "closed_orientable";
        case MaterialTopologyKind::OpenSurface:
            return "open_surface";
        case MaterialTopologyKind::NonManifold:
            return "non_manifold";
        case MaterialTopologyKind::SelfIntersecting:
            return "self_intersecting";
        case MaterialTopologyKind::Invalid:
            return "invalid";
    }
    return "invalid";
}

std::vector<MaterialTopologyFact> ClassifyMaterialTopologies(
    const AdaptedTriangleMesh& mesh,
    const MaterialTopologyClassifierOptions& options)
{
    const std::map<EdgeKey, int> wholeMeshEdges = BuildWholeMeshEdgeIncidence(mesh.mesh);

    // 按材质名分组，保持首次出现顺序以获得确定性输出。
    std::vector<std::string> order;
    std::map<std::string, std::vector<std::size_t>> groups;
    const std::size_t triangleCount =
        std::min(mesh.mesh.triangles.size(), mesh.triangle_attributes.size());
    for (std::size_t index{0}; index < triangleCount; ++index)
    {
        const std::string& name = mesh.triangle_attributes.at(index).material_name;
        const auto found = groups.find(name);
        if (found == groups.end())
        {
            groups.emplace(name, std::vector<std::size_t>{index});
            order.push_back(name);
            continue;
        }
        found->second.push_back(index);
    }

    std::vector<MaterialTopologyFact> facts;
    facts.reserve(order.size());
    for (const std::string& name : order)
    {
        const std::vector<std::size_t>& triangleIndices = groups.at(name);
        MaterialTopologyFact fact;
        fact.materialName = name;
        fact.triangleCount = static_cast<std::uint64_t>(triangleIndices.size());
        if (triangleIndices.empty())
        {
            fact.kind = MaterialTopologyKind::Invalid;
            facts.push_back(fact);
            continue;
        }

        // 子网格边统计使用【全网格顶点索引】，以便直接查全网格入射度。
        std::map<EdgeKey, int> subEdges;
        for (const std::size_t triangleIndex : triangleIndices)
        {
            const std::array<int, 3>& triangle = mesh.mesh.triangles.at(triangleIndex);
            ++subEdges[MakeEdgeKey(triangle[0], triangle[1])];
            ++subEdges[MakeEdgeKey(triangle[1], triangle[2])];
            ++subEdges[MakeEdgeKey(triangle[2], triangle[0])];
        }
        for (const std::pair<const EdgeKey, int>& edge : subEdges)
        {
            if (edge.second > 2)
            {
                ++fact.nonManifoldEdgeCount;
                continue;
            }
            if (edge.second != 1)
            {
                continue;
            }
            const auto whole = wholeMeshEdges.find(edge.first);
            const int wholeIncidence = whole == wholeMeshEdges.end() ? 1 : whole->second;
            if (wholeIncidence <= 1)
            {
                ++fact.boundaryEdgeCount;
            }
            else
            {
                ++fact.materialInterfaceEdgeCount;
            }
        }

        const TriangleMeshData subMesh = ExtractSubMesh(mesh.mesh, triangleIndices);
        const MeshTopologyReport topology = AnalyzeMeshTopology(subMesh);
        fact.signedVolumeMm3 = topology.signed_volume_mm3;

        if (options.analyzeSelfIntersections)
        {
            MeshCompleteSelfIntersectionOptions selfIntersection;
            selfIntersection.epsilonMm = options.selfIntersectionEpsilonMm;
            selfIntersection.maxCandidatePairs = options.maxSelfIntersectionCandidatePairs;
            const MeshCompleteSelfIntersectionAnalysis analysis =
                AnalyzeCompleteMeshSelfIntersections(subMesh, selfIntersection);
            fact.selfIntersectionEvaluated = true;
            fact.selfIntersectionComplete = analysis.complete;
            fact.confirmedSelfIntersectionPairs = analysis.confirmedIntersectionPairs;
            fact.selfIntersectionBlockerCode = analysis.blockerCode;
        }

        if (fact.nonManifoldEdgeCount > 0U)
        {
            fact.kind = MaterialTopologyKind::NonManifold;
        }
        else if (fact.confirmedSelfIntersectionPairs > 0U)
        {
            fact.kind = MaterialTopologyKind::SelfIntersecting;
        }
        else if (fact.selfIntersectionEvaluated && !fact.selfIntersectionComplete)
        {
            // 自交未判定完毕时不得报告为「无自交」，按 fail-closed 归入 Invalid。
            fact.kind = MaterialTopologyKind::Invalid;
        }
        else if (fact.boundaryEdgeCount > 0U || fact.materialInterfaceEdgeCount > 0U)
        {
            // 仅由材质交界边围成的子网格同样【不是】独立闭合体：垂直射线在它上面
            // 拿不到成对交点。归入 OpenSurface 以保持 fail-closed，两类边数分别保留，
            // 供 MV-04 区分「源模型真开放」与「材质交界导致开放」。
            fact.kind = MaterialTopologyKind::OpenSurface;
        }
        else
        {
            fact.kind = MaterialTopologyKind::ClosedOrientable;
        }
        facts.push_back(fact);
    }
    return facts;
}

}  // namespace slicer_core
