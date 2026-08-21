#pragma once

// MATVOL MV-06：在 MATVOL 最终 RGB 之后复用 Stage 15 的按需补白谓词。
//
// 红线（DOC_DECISION_MATVOL §5）：
//   按需补白只【观察】最终 RGB；不修改 RGB，不新增 Z 层，不触碰 S / V，
//   也不新增 ownership mask。判据必须复用既有 ApplyUnprintableWhiteCarrier，
//   不得复制一份近似实现。

#include <cstdint>
#include <span>
#include <string>

namespace slicer_core
{

/// @brief 补白参数；policy 在像素循环外解析一次，与既有快路径重载对齐。
struct MaterialVolumeWhiteCarrierRequest
{
    /// texture.unprintableWhitePolicy 是否为 white_underbase。
    bool whiteUnderbaseEnabled{false};
    std::uint8_t inkThreshold{0U};
    std::uint8_t whiteValue{0U};
};

/// @brief 单层补白统计；字段名与既有 unprintableWhiteCarrierPixels 报告口径一致。
struct MaterialVolumeWhiteCarrierStats
{
    std::uint64_t unprintableWhiteCarrierPixels{0U};
    std::uint64_t evaluatedModelPixels{0U};
};

/// @brief 对单层最终 RGB 应用按需补白，仅写 W 通道。
///
/// `rgbLayer` 长度须为 3 × 列数且【只读】；`whiteLayer` 长度须为列数。
/// 仅在 `modelMask` 非零的像素上求值；命中阈值时把 W 写为 whiteValue，
/// 未命中的像素 W 保持调用方原值不变。
/// @throws std::invalid_argument 缓冲区长度不符。
void ApplyMaterialVolumeWhiteCarrierLayer(
    const MaterialVolumeWhiteCarrierRequest& request,
    std::span<const std::uint8_t> rgbLayer,
    std::span<const std::uint8_t> modelMask,
    std::span<std::uint8_t> whiteLayer,
    MaterialVolumeWhiteCarrierStats& stats);

/// @brief 判定 MATVOL 与按需补白的组合是否被窄放行；用于配置期与预检期共用同一口径。
///
/// 首批只放行「materialVolumePolicy.enabled 且 unprintableWhitePolicy=white_underbase」，
/// 且要求 materialPolicy 与旧 materialRoleMapping 均关闭。
[[nodiscard]] bool IsMaterialVolumeWhiteCarrierCombinationAllowed(
    bool materialVolumeEnabled,
    const std::string& unprintableWhitePolicy,
    bool materialPolicyEnabled,
    bool materialRoleMappingEnabled) noexcept;

}  // namespace slicer_core
