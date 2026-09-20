#include "slicer_core/materials/transfer/LegacyTransferChannelSession.h"

#include "slicer_core/materials/transfer/TransferChannelError.h"

#include <utility>

namespace slicer_core
{

void ValidateLegacyTransferChannelRunBoundary(
    const TransferChannelPolicyConfig& policy,
    const bool directComputeOnly,
    const bool tiffManifestCandidate)
{
    if (policy.enabled && !directComputeOnly && !tiffManifestCandidate)
    {
        throw TransferChannelError(
            TransferChannelErrorCode::ProtocolInvalid,
            "p0.rgbwsvt.1 currently supports only direct Legacy compute or TIFF/manifest candidate output");
    }
}

LegacyTransferChannelSession BuildLegacyTransferChannelSession(
    const TransferChannelPolicyConfig& policy,
    const AdaptedTriangleMesh& mesh,
    const MaterialVolumeGrid& grid,
    std::function<bool()> cancellationRequested)
{
    if (!policy.enabled)
    {
        throw TransferChannelError(
            TransferChannelErrorCode::ConfigInvalid,
            "Legacy transfer session requires an enabled transfer policy");
    }
    LegacyTransferChannelSession session;
    session.plan = BuildTransferMaterialVolumePlan(
        policy, mesh, grid, std::move(cancellationRequested));
    session.transferValue = policy.value;
    session.ownerScratch.assign(session.plan.columnCount, kNoMaterialOwner);
    session.transferMask.assign(session.plan.columnCount, 0U);
    return session;
}

void MaterializeLegacyTransferChannelMask(
    LegacyTransferChannelSession& session,
    const int layerIndex,
    const std::span<const std::uint8_t> modelMask)
{
    if (layerIndex < 0 || layerIndex >= session.plan.layerCount)
    {
        throw TransferChannelError(
            TransferChannelErrorCode::ProtocolInvalid,
            "Legacy RGBWSV layer index is outside the transfer plan");
    }
    MaterializeTransferLayerMask(
        session.plan,
        layerIndex,
        modelMask,
        session.ownerScratch,
        session.transferMask);
}

RgbwsvtProductionLayer ComposeLegacyTransferChannelLayer(
    LegacyTransferChannelSession& session,
    const RgbwsvProductionLayer& rgbwsvLayer,
    const std::span<const std::uint8_t> modelMask)
{
    MaterializeLegacyTransferChannelMask(
        session, rgbwsvLayer.layerIndex, modelMask);
    return ComposeRgbwsvtLayer(
        rgbwsvLayer,
        modelMask,
        session.transferMask,
        session.transferValue);
}

}  // namespace slicer_core
