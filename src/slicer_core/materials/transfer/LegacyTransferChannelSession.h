#pragma once

#include "slicer_core/materials/transfer/TransferMaterialVolumePlan.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtProtocol.h"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace slicer_core
{

/**
 * @brief Per-run state for upgrading Legacy RGBWSV layers to RGBWSVT.
 *
 * Scratch buffers are retained by the caller-owned session and reused for
 * every layer. The session does not write files or decide package metadata.
 */
struct LegacyTransferChannelSession
{
    TransferMaterialVolumePlan plan;
    std::uint8_t transferValue{0U};
    std::vector<std::uint32_t> ownerScratch;
    std::vector<std::uint8_t> transferMask;
};

void ValidateLegacyTransferChannelRunBoundary(
    const TransferChannelPolicyConfig& policy,
    bool directComputeOnly,
    bool tiffManifestCandidate);

[[nodiscard]] LegacyTransferChannelSession BuildLegacyTransferChannelSession(
    const TransferChannelPolicyConfig& policy,
    const AdaptedTriangleMesh& mesh,
    const MaterialVolumeGrid& grid,
    std::function<bool()> cancellationRequested = {});

/**
 * @brief Materialize T for one layer and apply the frozen exclusive composer.
 */
/**
 * @brief 只把本层缩裹掩膜算进 `session.transferMask`，**不构建七通道层**。
 *
 * 整版路径只需要这张掩膜——七通道层由整版合成器在整版坐标上统一装配。
 * 逐实例再建一份整层七通道（还要跟着补白搬一次）纯属白做功：
 * 那条路上 `write_tiff_layers` 为 false，没有任何消费者。
 */
void MaterializeLegacyTransferChannelMask(
    LegacyTransferChannelSession& session,
    int layerIndex,
    std::span<const std::uint8_t> modelMask);

[[nodiscard]] RgbwsvtProductionLayer ComposeLegacyTransferChannelLayer(
    LegacyTransferChannelSession& session,
    const RgbwsvProductionLayer& rgbwsvLayer,
    std::span<const std::uint8_t> modelMask);

}  // namespace slicer_core
