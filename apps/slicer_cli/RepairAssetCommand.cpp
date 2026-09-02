#include "RepairAssetCommand.h"

#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/DeterministicObjAssetWriter.h"
#include "slicer_core/geometry/repair/MeshRepairBoundaryOperations.h"
#include "slicer_core/geometry/repair/MeshRepairService.h"
#include "slicer_core/model.h"
#include "slicer_core/model/ModelLoadConfig.h"

#include <cctype>
#include <exception>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace slicer_cli
{
namespace
{

/// @brief 统计子网格的开放边界边数，用于修复前后对照。
[[nodiscard]] std::uint64_t BoundaryEdgeCount(
    const slicer_core::MeshTopologyReport& topology)
{
    return topology.boundary_edges;
}

void PrintTopology(const char* label, const slicer_core::MeshTopologyReport& topology)
{
    std::cout
        << "  " << label
        << " triangles=" << topology.accepted_triangles
        << " vertices=" << topology.unique_vertices
        << " boundaryEdges=" << BoundaryEdgeCount(topology)
        << " nonManifoldEdges=" << topology.non_manifold_edges
        << " degenerate=" << topology.degenerate_triangles
        << '\n';
}

/**
 * @brief 把材质名转为 OBJ 令牌安全形式：空白与控制字符换成下划线。
 *
 * `DeterministicObjAssetWriter` 拒绝含空白或控制字符的材质名，而 MO-06 起
 * 材质名会保留 `sg (1)` 这样的内部空格。此处只在【修复产物】里做转义，
 * 不回写源资产，也不改动写盘器自身的校验口径。
 */
[[nodiscard]] std::string ToObjSafeName(const std::string& name)
{
    std::string safe;
    safe.reserve(name.size());
    for (const char character : name)
    {
        const auto raw = static_cast<unsigned char>(character);
        const bool unsafe = std::isspace(raw) != 0 || std::iscntrl(raw) != 0;
        safe.push_back(unsafe ? '_' : character);
    }
    return safe;
}

/**
 * @brief 就地把网格内的材质名换成 OBJ 令牌安全形式，并回报改名映射。
 * @param mesh 待改名的候选网格。
 * @return 发生改名的 (原名, 新名) 列表；未改名的材质不入列。
 */
std::vector<std::pair<std::string, std::string>> ApplyObjSafeMaterialNames(
    slicer_core::AdaptedTriangleMesh& mesh)
{
    std::vector<std::pair<std::string, std::string>> renamed;
    std::map<std::string, std::string> mapping;
    for (slicer_core::MaterialInfo& material : mesh.material_infos)
    {
        const std::string safe = ToObjSafeName(material.name);
        if (safe == material.name)
        {
            continue;
        }
        mapping.emplace(material.name, safe);
        renamed.emplace_back(material.name, safe);
        material.name = safe;
    }
    if (mapping.empty())
    {
        return renamed;
    }
    for (slicer_core::SurfaceTriangleAttributes& attributes : mesh.triangle_attributes)
    {
        const auto found = mapping.find(attributes.material_name);
        if (found != mapping.end())
        {
            attributes.material_name = found->second;
        }
    }
    return renamed;
}

}  // namespace

int RunRepairAsset(const RepairAssetRequest& request)
{
    if (request.inputModelPath.empty() || request.outputObjPath.empty())
    {
        std::cerr << "slicer_cli error: --repair-asset requires --input and --output\n";
        return 2;
    }

    slicer_core::ModelLoadConfig loadConfig;
    loadConfig.input.model_path = request.inputModelPath;
    loadConfig.input.format = "auto";
    loadConfig.auto_orient.enabled = false;

    const slicer_core::ModelReport report =
        slicer_core::load_model_report(loadConfig, std::filesystem::current_path());

    slicer_core::SceneModelTriangleMeshAdapterOptions adapterOptions;
// 诊断用：允许显式收紧退化面阈值。默认阈值会把面积 < 1e-6 mm^2 的
// 【极小但合法】的面当成退化面丢弃，从而在闭合网格上制造出洞。
    if (request.degenerateAreaEpsilonMm2 > 0.0)
    {
        adapterOptions.degenerate_area_epsilon_mm2 = request.degenerateAreaEpsilonMm2;
    }
    const slicer_core::AdaptedTriangleMesh adapted =
        slicer_core::AdaptSceneModelToTriangleMesh(report, adapterOptions);

    std::cout << "slicer_cli: asset repair\n";
    std::cout << "  input: " << request.inputModelPath.generic_string() << '\n';
    PrintTopology("before", adapted.topology);

    slicer_core::MeshRepairCleanupRequest cleanup;
    cleanup.mesh = &adapted;
    cleanup.input.sourcePath = request.inputModelPath.generic_string();
    cleanup.input.inputFormat = report.format;
    cleanup.input.vertexCount = adapted.topology.unique_vertices;
    cleanup.input.triangleCount = adapted.topology.accepted_triangles;
    cleanup.input.materialCount = adapted.material_infos.size();
    cleanup.options.enabled = true;
// 清理服务只接受 repair_then_strict：修复后必须重跑严格诊断，
// 不允许把修复产物当作已通过严格准入。
    cleanup.options.mode = "repair_then_strict";
    cleanup.options.allowBoundaryFill = request.allowBoundaryFill;
    cleanup.options.maxBoundaryLoopEdges = request.maxBoundaryLoopEdges;
    cleanup.options.maxHoleAreaMm2 = request.maxHoleAreaMm2;
    cleanup.options.maxBoundaryLoopDiameterMm = request.maxBoundaryLoopDiameterMm;
    cleanup.options.maxBoundaryLoopPerimeterMm = request.maxBoundaryLoopPerimeterMm;
    cleanup.options.maxBoundaryPlanarityErrorMm = request.maxBoundaryPlanarityErrorMm;
    cleanup.options.maxAffectedFaceRatio = request.maxAffectedFaceRatio;
    cleanup.options.allowNewFaces = request.allowBoundaryFill;
// 填补服务只接受该属性策略：新面继承统一材质且【不带 UV】。
// 对带贴图资产意味着补出的面没有纹理坐标，这一语义损失必须让调用方看见。
    cleanup.options.newFaceAttributePolicy =
        request.allowBoundaryFill ? "inherit_uniform_material_no_uv" : "reject";

    const slicer_core::MeshRepairCleanupResult cleaned =
        slicer_core::ExecuteMeshRepairCleanup(cleanup);
    PrintTopology("afterCleanup", cleaned.candidate.topology);
    std::cout << "  cleanupOperations=" << cleaned.evidence.operations.size() << '\n';

    const slicer_core::AdaptedTriangleMesh* finalMesh = &cleaned.candidate;
    slicer_core::MeshRepairBoundaryOperationResult boundary;
    if (request.allowBoundaryFill)
    {
        slicer_core::MeshRepairBoundaryOperationRequest boundaryRequest;
        boundaryRequest.mesh = &cleaned.candidate;
        boundaryRequest.options = cleanup.options;
        boundary = slicer_core::ExecuteMeshRepairBoundaryOperations(boundaryRequest);
        if (boundary.blocked)
        {
// 填补被预算或属性策略挡下时不得静默继续，否则会写出一个看似已修好的资产。
            std::cerr
                << "slicer_cli error: boundary fill was blocked: "
                << boundary.blockerCode << '\n';
            return 3;
        }
        finalMesh = &boundary.candidate;
        PrintTopology("afterBoundaryFill", boundary.candidate.topology);
        std::cout << "  boundaryOperations=" << boundary.operations.size() << '\n';
    }

// 选项 1（用户 2026-09-01 裁定）：写盘前转义材质名并落盘映射关系，
// 不放宽写盘器校验、不改源资产命名。
    slicer_core::AdaptedTriangleMesh writable = *finalMesh;
    const auto renamed = ApplyObjSafeMaterialNames(writable);
    if (!renamed.empty())
    {
        std::cout << "  materialNameRemap (" << renamed.size() << "):" << '\n';
        for (const auto& entry : renamed)
        {
            std::cout
                << "    \"" << entry.first << "\" -> \""
                << entry.second << "\"" << '\n';
        }
    }

    slicer_core::DeterministicObjAssetWriteRequest write;
    write.mesh = &writable;
    write.outputObjPath = request.outputObjPath;
    const slicer_core::DeterministicObjAssetWriteResult written =
        slicer_core::WriteDeterministicObjAsset(write);

    std::cout
        << "  obj: " << written.objPath.generic_string() << '\n'
        << "  mtl: " << written.mtlPath.generic_string() << '\n'
        << "  uvPreserved=" << (written.uvPreserved ? "true" : "false")
        << " materialsPreserved=" << (written.materialsPreserved ? "true" : "false")
        << " textureBytesPreserved=" << (written.textureBytesPreserved ? "true" : "false")
        << '\n';

    const std::uint64_t remaining = BoundaryEdgeCount(writable.topology);
    if (remaining > 0U)
    {
        std::cout
            << "  note: " << remaining
            << " boundary edges remain; the asset is still not closed per material\n";
    }
    return 0;
}

}  // namespace slicer_cli
