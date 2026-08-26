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
};

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
