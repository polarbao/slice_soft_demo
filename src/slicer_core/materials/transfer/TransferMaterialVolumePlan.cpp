#include "slicer_core/materials/transfer/TransferMaterialVolumePlan.h"

#include "slicer_core/materials/transfer/TransferChannelError.h"
#include "slicer_core/materials/volume/MaterialVolumeError.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace slicer_core
{

TransferMaterialVolumePlan BuildTransferMaterialVolumePlan(
    const TransferChannelPolicyConfig& policy,
    const AdaptedTriangleMesh& mesh,
    const MaterialVolumeGrid& grid,
    std::function<bool()> cancellationRequested)
{
    TransferMaterialVolumePlan result;
    result.layerCount = grid.layerCount;
    if (grid.widthPx > 0 && grid.heightPx > 0)
    {
        result.columnCount = static_cast<std::size_t>(grid.widthPx)
            * static_cast<std::size_t>(grid.heightPx);
    }
    result.material = ResolveTransferMaterial(policy, mesh.material_infos);
    if (!result.material.present)
    {
        return result;
    }
    // 整模模式：区域就是模型本身，不需要按材质求体积区间。
    //
    // 这与 materialPolicy.varnish.mode == "all_model" 同构——那条路径也是在
    // 已栅格化的模型像素上直接赋值（SliceMaterialTexture.cpp），不经体积求解。
    // 走体积求解会平白引入对流形与自交的要求：实测 alg_suoguo 的 10 件资产里
    // 有 5 件因此被拒，而它们在光油整模工艺下切得好好的。
    if (result.material.wholeModel)
    {
        return result;
    }

    MaterialVolumePolicyConfig volumePolicy;
    volumePolicy.enabled = true;
    volumePolicy.mode = "closed_intervals";
    volumePolicy.missing_material = "fail_closed";
    volumePolicy.open_surface.mode = "reject";
    volumePolicy.overlap.mode = "explicit_priority";
    volumePolicy.topology = policy.topology;
    volumePolicy.overlap.rules.push_back(
        MaterialVolumeOverlapRuleConfig{result.material.materialName, 1});

    MaterialVolumeBuildRequest request;
    request.mesh = &mesh;
    request.policy = &volumePolicy;
    request.grid = grid;
    request.materialNameFilter = result.material.materialName;
    request.cancellationRequested = std::move(cancellationRequested);
    try
    {
        result.volume.emplace(BuildMaterialVolumePlan(request));
    }
    catch (const MaterialVolumeError& error)
    {
        throw TransferChannelError(
            TransferChannelErrorCode::TopologyInvalid,
            "transfer material '" + result.material.materialName + "' was rejected: "
                + error.what());
    }
    return result;
}

void MaterializeTransferLayerMask(
    const TransferMaterialVolumePlan& plan,
    const int layerIndex,
    const std::span<const std::uint8_t> modelMask,
    const std::span<std::uint32_t> ownerScratch,
    const std::span<std::uint8_t> transferMaskOut)
{
    if (layerIndex < 0 || layerIndex >= plan.layerCount)
    {
        throw std::invalid_argument("transfer layer index is out of range");
    }
    if (modelMask.size() != plan.columnCount
        || ownerScratch.size() != plan.columnCount
        || transferMaskOut.size() != plan.columnCount)
    {
        throw std::invalid_argument("transfer layer buffers have invalid sizes");
    }
    if (!plan.HasRegion())
    {
        std::fill(transferMaskOut.begin(), transferMaskOut.end(), 0U);
        std::fill(ownerScratch.begin(), ownerScratch.end(), kNoMaterialOwner);
        return;
    }
    if (plan.material.wholeModel)
    {
        // 整模即缩裹：本层的缩裹区域就是本层的模型区域，逐像素照搬。
        std::fill(ownerScratch.begin(), ownerScratch.end(), kNoMaterialOwner);
        for (std::size_t index{0U}; index < plan.columnCount; ++index)
        {
            transferMaskOut[index] = modelMask[index] != 0U ? 1U : 0U;
        }
        return;
    }

    MaterializeMaterialOwnershipLayer(
        *plan.volume, layerIndex, modelMask, ownerScratch);
    for (std::size_t index{0U}; index < plan.columnCount; ++index)
    {
        transferMaskOut[index] = ownerScratch[index] == kNoMaterialOwner ? 0U : 1U;
    }
}

}  // namespace slicer_core
