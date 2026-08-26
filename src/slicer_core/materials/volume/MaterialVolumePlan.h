#pragma once

// MATVOL MV-03：封闭材质有序交点、compact 层区间与 caller-owned 单层 owner 物化。
//
// 内存边界（DEV_MATVOL §10）：
//   允许  O(列数) 的 compact 区间 + 调用方持有的 O(XY) 单层 owner buffer；
//   禁止  O(材质数 × 层数 × 像素数) 的稠密所有权栈；
//   要求  move-only plan、构建期校验、物化热路径零分配、buffer 地址可复用。

#include "slicer_core/config.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/volume/MaterialTopologyClassifier.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace slicer_core
{

/// @brief owner buffer 中表示「无材质所有者」的哨兵值。
inline constexpr std::uint32_t kNoMaterialOwner{0xFFFFFFFFU};

/// @brief 单列上的一段闭合层区间；materialIndex 指向 MaterialVolumePlan::MaterialNames()。
struct MaterialLayerInterval
{
    int firstLayerInclusive{0};
    int lastLayerInclusive{-1};
    std::uint32_t materialIndex{kNoMaterialOwner};
};

/// @brief XY 栅格与层参数；采样中心与层换算必须与既有 S0 规则一致。
struct MaterialVolumeGrid
{
    int widthPx{0};
    int heightPx{0};
    double originXMm{0.0};
    double originYMm{0.0};
    double pixelSizeXMm{0.0};
    double pixelSizeYMm{0.0};
    double layerThicknessMm{0.0};
    int layerCount{0};
};

/// @brief 构建输入；mesh 与 policy 均为借入指针，plan 不持有它们。
struct MaterialVolumeBuildRequest
{
    const AdaptedTriangleMesh* mesh{nullptr};
    const MaterialVolumePolicyConfig* policy{nullptr};
    MaterialVolumeGrid grid;
    /// Optional single-material filter. Topology is still classified against
    /// the complete adapted mesh so material-interface edges remain distinct
    /// from true open boundaries.
    std::string materialNameFilter;
    /// 同步取消点；返回 true 时构建立即失败且不产出半成品 plan。
    std::function<bool()> cancellationRequested;
};

/// @brief 已校验、不可变、move-only 的逐列材质层区间计划。
class MaterialVolumePlan
{
public:
    MaterialVolumePlan(const MaterialVolumePlan&) = delete;
    MaterialVolumePlan(MaterialVolumePlan&&) noexcept = default;
    MaterialVolumePlan& operator=(const MaterialVolumePlan&) = delete;
    MaterialVolumePlan& operator=(MaterialVolumePlan&&) noexcept = default;

    [[nodiscard]] int LayerCount() const noexcept { return layerCount_; }
    [[nodiscard]] std::size_t ColumnCount() const noexcept { return columnCount_; }

    /// @brief 材质名表；owner 值即本表下标。
    [[nodiscard]] std::span<const std::string> MaterialNames() const noexcept
    {
        return materialNames_;
    }

    /// @brief 与材质名表同序的显式优先级，数值大者胜。
    [[nodiscard]] std::span<const int> MaterialPriorities() const noexcept
    {
        return materialPriorities_;
    }

    /// @brief 扁平化区间的列偏移表，长度为 ColumnCount() + 1。
    [[nodiscard]] std::span<const std::uint32_t> ColumnIntervalOffsets() const noexcept
    {
        return columnIntervalOffsets_;
    }

    /// @brief 扁平化的全部层区间，按列、再按 firstLayer 升序排列。
    /// @brief 构建期算出的逐材质拓扑事实。
    ///        构建器本就要调 ClassifyMaterialTopologies 才能判定准入，此处保留其结果，
    ///        使报告无需二次分析——重算一遍会把自交分析的代价白付两次。
    [[nodiscard]] std::span<const MaterialTopologyFact> TopologyFacts() const noexcept
    {
        return {topologyFacts_.data(), topologyFacts_.size()};
    }

    /// @brief 按 selfIntersectionPolicy=tolerate_if_parity_intact 放行的自交材质名。
    ///        放行不等于无缺陷：其奇偶性由逐列 IntersectionUnpaired 精确把关，
    ///        此处保留名单以便报告披露，不得静默吞掉。
    [[nodiscard]] std::span<const std::string> ToleratedSelfIntersectingMaterials() const noexcept
    {
        return {toleratedSelfIntersectingMaterials_.data(),
                toleratedSelfIntersectingMaterials_.size()};
    }

    [[nodiscard]] std::span<const MaterialLayerInterval> Intervals() const noexcept
    {
        return intervals_;
    }

    /// @brief 指定列的层区间视图。
    [[nodiscard]] std::span<const MaterialLayerInterval> ColumnIntervals(
        std::size_t columnIndex) const;

private:
    MaterialVolumePlan() = default;

    friend MaterialVolumePlan BuildMaterialVolumePlan(const MaterialVolumeBuildRequest& request);

    int layerCount_{0};
    std::size_t columnCount_{0U};
    std::vector<std::string> materialNames_;
    std::vector<int> materialPriorities_;
    std::vector<std::uint32_t> columnIntervalOffsets_;
    std::vector<MaterialLayerInterval> intervals_;
    std::vector<std::string> toleratedSelfIntersectingMaterials_;
    std::vector<MaterialTopologyFact> topologyFacts_;
};

/// @brief 对封闭可定向材质子网格求有序交点并生成 compact 层区间计划。
///
/// 构建期即完成全部校验：奇数交点、开放材质、缺失优先级与同级实际重叠一律 fail closed，
/// 因此物化阶段无需再抛错，可保持热路径纯净。
/// @throws MaterialVolumeError 任一 fail-closed 条件命中，或构建被取消。
[[nodiscard]] MaterialVolumePlan BuildMaterialVolumePlan(const MaterialVolumeBuildRequest& request);

/// @brief 把单层材质所有权写入调用方持有的 buffer；热路径零分配、零跨层扫描。
///
/// `ownerOut` 会被完全覆盖。仅在 `modelMask` 为非零的像素上写入材质 owner，
/// 其余一律写入 kNoMaterialOwner。缓冲区尺寸由调用方负责，尺寸不符属编程错误。
/// @throws std::invalid_argument 层号越界或缓冲区长度与 ColumnCount() 不符。
void MaterializeMaterialOwnershipLayer(
    const MaterialVolumePlan& plan,
    int layerIndex,
    std::span<const std::uint8_t> modelMask,
    std::span<std::uint32_t> ownerOut);

}  // namespace slicer_core
