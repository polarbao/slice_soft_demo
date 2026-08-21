#pragma once

// MATVOL MV-05：按材质 owner 解析 RGB 并合成单层。
//
// 红线：本模块【只写 RGB】。不触碰 W / S / V，不新增 Z 层，不修改 model occupancy。
// 按需补白由 MV-06 在最终 RGB 之后复用既有 ApplyUnprintableWhiteCarrier 完成。

#include "slicer_core/materials/volume/MaterialVolumePlan.h"
#include "slicer_core/model.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace slicer_core
{

/// @brief 材质缺失 Kd 时的处置；默认 fail closed，不静默继承相邻材质。
enum class MaterialRgbFallbackPolicy
{
    FailClosed,
    ExplicitFallback,
};

/// @brief RGB 解析输入；materialInfos 通常直接取自 ModelReport::material_infos。
struct MaterialRgbTableRequest
{
    const MaterialVolumePlan* plan{nullptr};
    std::span<const MaterialInfo> materialInfos;
    MaterialRgbFallbackPolicy fallbackPolicy{MaterialRgbFallbackPolicy::FailClosed};
    std::array<std::uint8_t, 3> explicitFallbackRgb{0U, 0U, 0U};
    /// 无 owner 的像素写入该值；由调用方显式给出，避免隐式背景约定。
    std::array<std::uint8_t, 3> unownedRgb{0U, 0U, 0U};
};

/// @brief 已解析并校验的逐材质 RGB 表；与 plan 的材质下标同序。
class MaterialRgbTable
{
public:
    MaterialRgbTable(const MaterialRgbTable&) = delete;
    MaterialRgbTable(MaterialRgbTable&&) noexcept = default;
    MaterialRgbTable& operator=(const MaterialRgbTable&) = delete;
    MaterialRgbTable& operator=(MaterialRgbTable&&) noexcept = default;

    [[nodiscard]] std::span<const std::array<std::uint8_t, 3>> RgbByMaterial() const noexcept
    {
        return rgbByMaterial_;
    }

    [[nodiscard]] std::array<std::uint8_t, 3> UnownedRgb() const noexcept { return unownedRgb_; }

    /// @brief 各材质 RGB 的来源，取值为 mtl_kd 或 explicit_fallback，供报告使用。
    [[nodiscard]] std::span<const std::string> RgbSources() const noexcept { return rgbSources_; }

private:
    MaterialRgbTable() = default;

    friend MaterialRgbTable BuildMaterialRgbTable(const MaterialRgbTableRequest& request);

    std::vector<std::array<std::uint8_t, 3>> rgbByMaterial_;
    std::vector<std::string> rgbSources_;
    std::array<std::uint8_t, 3> unownedRgb_{0U, 0U, 0U};
};

/// @brief 解析每个材质的最终 RGB；MTL Kd 优先，其次显式 fallback。
/// @throws MaterialVolumeError 材质缺 Kd 且策略为 FailClosed，或输入不完整。
[[nodiscard]] MaterialRgbTable BuildMaterialRgbTable(const MaterialRgbTableRequest& request);

/// @brief 把单层 owner 合成为 RGB；`rgbOut` 长度须为 3 × 列数，逐像素 R、G、B 交错。
///
/// 本函数不分配、不跨层扫描，且【只写入 rgbOut 的前 3 × 列数 字节】。
/// @throws std::invalid_argument 缓冲区长度不符。
/// @throws MaterialVolumeError modelMask 标记为模型的像素没有有效 owner。
void ComposeMaterialLayerRgb(
    const MaterialRgbTable& table,
    std::span<const std::uint32_t> ownerLayer,
    std::span<const std::uint8_t> modelMask,
    std::span<std::uint8_t> rgbOut);

}  // namespace slicer_core
