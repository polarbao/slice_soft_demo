#pragma once

#include "slicer_core/config.h"
#include "slicer_core/model.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace slicer_core
{

struct TransferMaterialMatch
{
    bool present{false};
    std::string materialName;
    std::array<std::uint8_t, 3> diffuseRgb{0U, 0U, 0U};
    /// true 表示这是 whole_model 命中：materialName 是下方的合成名而非真实材质，
    /// diffuseRgb 无意义。报告层据此决定如何披露。
    bool wholeModel{false};
};

/// whole_model 命中时写进报告的合成材质名。
///
/// 它不是资产里的真实材质——无 mtl 的纯几何资产本就没有材质名。
/// 报告契约要求 materialName 非空，故给一个可辨识的占位值。
/// **它只是报告标签，不参与任何几何筛选**：整模模式的区域直接取模型区域，
/// 不经体积求解（见 TransferMaterialVolumePlan）。
inline constexpr const char* kWholeModelTransferMaterialName{"__monowrap_whole_model__"};

/**
 * @brief Resolve one transfer material by exact configured diffuse colour.
 *
 * Material names are returned as geometry keys only after colour matching;
 * they are never treated as transfer-role configuration.
 */
[[nodiscard]] TransferMaterialMatch ResolveTransferMaterial(
    const TransferChannelPolicyConfig& policy,
    std::span<const MaterialInfo> materialInfos);

}  // namespace slicer_core
