#pragma once

// MATVOL MV-02：逐材质子网格拓扑分类。
//
// 关键语义：`AdaptSceneModelToTriangleMesh` 会做【全网格顶点焊接】，因此材质交界处的边
// 在子网格里必然退化为边界边。若不区分「真开边」与「材质交界边」，任何多材质模型都会被
// 误判为 OpenSurface。本分类器用全网格边度数表做这一区分。

#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"

#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/// @brief 单个材质子网格的拓扑类别。
enum class MaterialTopologyKind
{
    ClosedOrientable,
    OpenSurface,
    NonManifold,
    SelfIntersecting,
    Invalid,
};

/// @brief 拓扑类别的稳定名称，用于报告与错误信息。
[[nodiscard]] std::string MaterialTopologyKindName(MaterialTopologyKind kind);

/// @brief 单个材质子网格的拓扑事实。
struct MaterialTopologyFact
{
    std::string materialName;
    MaterialTopologyKind kind{MaterialTopologyKind::Invalid};
    std::uint64_t triangleCount{0U};
    /// 真开边：该边在【全网格】中也只有一个入射面。
    std::uint64_t boundaryEdgeCount{0U};
    /// 材质交界边：在子网格中是边界，但在全网格中仍是流形边。
    std::uint64_t materialInterfaceEdgeCount{0U};
    std::uint64_t nonManifoldEdgeCount{0U};
    std::uint64_t confirmedSelfIntersectionPairs{0U};
    bool selfIntersectionEvaluated{false};
    bool selfIntersectionComplete{false};
    std::string selfIntersectionBlockerCode;
    double signedVolumeMm3{0.0};
};

/// @brief 分类选项；自交分析代价较高，可显式关闭。
struct MaterialTopologyClassifierOptions
{
    bool analyzeSelfIntersections{true};
    double selfIntersectionEpsilonMm{1.0e-6};
    std::uint64_t maxSelfIntersectionCandidatePairs{5000000U};
};

/// @brief 对每个 `usemtl` 分组独立分类；材质名为空的三角面归入独立的空名分组。
///
/// 分类优先级：NonManifold > SelfIntersecting > OpenSurface > ClosedOrientable；
/// 三角面数为 0 或自交分析被预算阻断且未完成时为 Invalid。
[[nodiscard]] std::vector<MaterialTopologyFact> ClassifyMaterialTopologies(
    const AdaptedTriangleMesh& mesh,
    const MaterialTopologyClassifierOptions& options = {});

}  // namespace slicer_core
