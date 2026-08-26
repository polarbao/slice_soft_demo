#pragma once

#include "slicer_core/config.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/transfer/TransferMaterialResolver.h"
#include "slicer_core/materials/volume/MaterialVolumePlan.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace slicer_core
{

struct TransferMaterialVolumePlan
{
    TransferMaterialMatch material;
    std::optional<MaterialVolumePlan> volume;
    int layerCount{0};
    std::size_t columnCount{0U};

    [[nodiscard]] bool HasRegion() const noexcept
    {
        return material.present && volume.has_value();
    }
};

/**
 * @brief Build a compact plan for only the colour-resolved transfer material.
 *
 * The full mesh remains available to topology classification, while interval
 * generation is filtered to the resolved material geometry.
 */
[[nodiscard]] TransferMaterialVolumePlan BuildTransferMaterialVolumePlan(
    const TransferChannelPolicyConfig& policy,
    const AdaptedTriangleMesh& mesh,
    const MaterialVolumeGrid& grid,
    std::function<bool()> cancellationRequested = {});

/**
 * @brief Materialize one binary transfer occupancy mask with caller-owned buffers.
 */
void MaterializeTransferLayerMask(
    const TransferMaterialVolumePlan& plan,
    int layerIndex,
    std::span<const std::uint8_t> modelMask,
    std::span<std::uint32_t> ownerScratch,
    std::span<std::uint8_t> transferMaskOut);

}  // namespace slicer_core
