#pragma once

// MATVOL MV-06：material_volume_report 构建（纯函数式，Json 进 Json 出，不做 I/O）。
//
// 采用与 MaterialClosureReport 一致的扁平领域对象风格，不使用 7 字段报告基座。

#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/materials/volume/MaterialLayerRgbComposer.h"
#include "slicer_core/materials/volume/MaterialTopologyClassifier.h"
#include "slicer_core/materials/volume/MaterialVolumePlan.h"

#include <cstdint>
#include <span>
#include <vector>

namespace slicer_core
{

/// @brief 单层 owner 与补白统计；ownerPixelsByMaterial 与 plan 的材质表同序。
struct MaterialVolumeLayerStat
{
    int layerIndex{0};
    std::vector<std::uint64_t> ownerPixelsByMaterial;
    std::uint64_t unownedModelPixels{0U};
    std::uint64_t unprintableWhiteCarrierPixels{0U};
};

/// @brief 报告输入；全部为借入指针或视图，构建函数不持有它们。
struct MaterialVolumeReportInput
{
    const MaterialVolumePlan* plan{nullptr};
    const MaterialRgbTable* rgbTable{nullptr};
    const MaterialVolumePolicyConfig* policy{nullptr};
    std::span<const MaterialTopologyFact> topologyFacts;
    std::span<const MaterialVolumeLayerStat> layers;
};

/// @brief 统计单层各材质的 owner 像素数与未拥有的模型像素数。
/// @throws std::invalid_argument 缓冲区长度不符。
[[nodiscard]] MaterialVolumeLayerStat CountMaterialVolumeLayerOwners(
    const MaterialVolumePlan& plan,
    int layerIndex,
    std::span<const std::uint32_t> ownerLayer,
    std::span<const std::uint8_t> modelMask);

/// @brief 构建 `slicesoft.material_volume_report.1` 报告文档。
/// @throws std::invalid_argument 输入不完整或逐层统计与材质表长度不符。
[[nodiscard]] Json BuildMaterialVolumeReport(const MaterialVolumeReportInput& input);

}  // namespace slicer_core
