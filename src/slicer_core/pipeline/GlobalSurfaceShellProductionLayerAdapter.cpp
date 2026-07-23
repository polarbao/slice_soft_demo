#include "slicer_core/pipeline/GlobalSurfaceShellProductionLayerAdapter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr std::array<const char*, 6> kChannelOrder{"R", "G", "B", "W", "S", "V"};

void BlockAdapter(
    GlobalSurfaceShellProductionLayerAdapterResult& result,
    const SlicePipelineErrorCode code,
    const std::string& detail)
{
    result.available = false;
    result.status = "blocked";
    result.layers.clear();
    result.errorCode = code;
    result.detail = detail;
}

bool HasFixedProtocol(const RgbwsvProtocol& protocol)
{
    if (protocol.schema != "p0.rgbwsv.2"
        || protocol.bit_depth != 8
        || protocol.polarity != "black_is_print"
        || protocol.print_value != 0U
        || protocol.empty_value != 255U)
    {
        return false;
    }
    for (std::size_t index{0U}; index < kChannelOrder.size(); ++index)
    {
        if (protocol.channel_order.at(index) != kChannelOrder.at(index))
        {
            return false;
        }
    }
    return true;
}

bool HasFixedChannelOrder(const std::array<std::string, 6>& channelOrder)
{
    for (std::size_t index{0U}; index < kChannelOrder.size(); ++index)
    {
        if (channelOrder.at(index) != kChannelOrder.at(index))
        {
            return false;
        }
    }
    return true;
}

bool SameLayerIdentity(
    const TextureFillPartitionRasterLayer& mapping,
    const TextureFillPartitionFullClosureLayerEvidence& evidence,
    const TextureFillPartitionFullClosureLayerResult& closure,
    const int expectedLayerIndex,
    const double expectedZ,
    const double zTolerance)
{
    return mapping.layerIndex == expectedLayerIndex
        && evidence.layerIndex == expectedLayerIndex
        && closure.layerIndex == expectedLayerIndex
        && closure.sidecar.layerIndex == expectedLayerIndex
        && std::isfinite(mapping.zMm)
        && std::isfinite(evidence.zMm)
        && std::isfinite(closure.zMm)
        && std::isfinite(closure.sidecar.zMm)
        && std::abs(mapping.zMm - expectedZ) <= zTolerance
        && std::abs(evidence.zMm - expectedZ) <= zTolerance
        && std::abs(closure.zMm - expectedZ) <= zTolerance
        && std::abs(closure.sidecar.zMm - expectedZ) <= zTolerance;
}

bool SameSemanticEvidence(
    const TextureFillPartitionRasterLayer& mapping,
    const TextureFillPartitionFullClosureLayerEvidence& evidence,
    const MaterialClosureSemanticLayerInput& semantic)
{
    return semantic.textureSurfaceMask == mapping.textureSurfaceMask
        && semantic.modelFillMask == mapping.modelFillMask
        && semantic.modelMaterialMask == mapping.modelMask
        && semantic.supportFillMask == evidence.supportFillMask
        && semantic.internalVoidSupportMask == evidence.internalVoidSupportMask
        && semantic.surfaceVarnishMask == evidence.surfaceVarnishMask
        && semantic.outerVarnishShellMask == evidence.outerVarnishShellMask
        && semantic.modelEnvelopeMask == evidence.modelEnvelopeMask
        && semantic.supportRequiredMask == evidence.supportRequiredMask;
}

bool IsBinaryMask(const std::vector<std::uint8_t>& mask)
{
    return std::all_of(
        mask.begin(),
        mask.end(),
        [](const std::uint8_t value)
        {
            return value == 0U || value == 1U;
        });
}

bool HasValidLayerShapes(
    const TextureFillPartitionRasterLayer& mapping,
    const TextureFillPartitionFullClosureLayerEvidence& evidence,
    const MaterialClosureSemanticLayerInput& semantic,
    const std::size_t pixelCount)
{
    const std::array<const std::vector<std::uint8_t>*, 20> masks{
        &mapping.modelMask,
        &mapping.textureSurfaceMask,
        &mapping.modelFillMask,
        &evidence.supportFillMask,
        &evidence.internalVoidSupportMask,
        &evidence.surfaceVarnishMask,
        &evidence.outerVarnishShellMask,
        &evidence.modelEnvelopeMask,
        &evidence.supportRequiredMask,
        &semantic.textureSurfaceMask,
        &semantic.modelFillMask,
        &semantic.modelMaterialMask,
        &semantic.supportFillMask,
        &semantic.internalVoidSupportMask,
        &semantic.surfaceVarnishMask,
        &semantic.outerVarnishShellMask,
        &semantic.modelEnvelopeMask,
        &semantic.supportRequiredMask,
        &semantic.expectedOccupiedDomainMask,
        &semantic.layerEmptyMask,
    };
    if (mapping.textureRgb.size() != pixelCount)
    {
        return false;
    }
    return std::all_of(
        masks.begin(),
        masks.end(),
        [pixelCount](const std::vector<std::uint8_t>* mask)
        {
            return mask->size() == pixelCount && IsBinaryMask(*mask);
        });
}

bool TextureRgbMatchesFinalChannels(
    const TextureFillPartitionRasterLayer& mapping,
    const std::vector<std::uint8_t>& channels)
{
    for (std::size_t pixelIndex{0U};
         pixelIndex < mapping.textureSurfaceMask.size();
         ++pixelIndex)
    {
        if (mapping.textureSurfaceMask.at(pixelIndex) == 0U)
        {
            continue;
        }
        const std::size_t base = pixelIndex * kChannelOrder.size();
        const std::array<std::uint8_t, 3>& expected =
            mapping.textureRgb.at(pixelIndex);
        if (channels.at(base) != expected.at(0)
            || channels.at(base + 1U) != expected.at(1)
            || channels.at(base + 2U) != expected.at(2))
        {
            return false;
        }
    }
    return true;
}

bool EmptyMaskMatchesFinalChannels(
    const MaterialClosureSemanticLayerInput& semantic,
    const std::vector<std::uint8_t>& channels,
    const std::uint8_t emptyValue)
{
    for (std::size_t pixelIndex{0U};
         pixelIndex < semantic.layerEmptyMask.size();
         ++pixelIndex)
    {
        const std::size_t base = pixelIndex * kChannelOrder.size();
        bool empty{true};
        for (std::size_t channel{0U}; channel < kChannelOrder.size(); ++channel)
        {
            empty = empty && channels.at(base + channel) == emptyValue;
        }
        if ((semantic.layerEmptyMask.at(pixelIndex) != 0U) != empty)
        {
            return false;
        }
    }
    return true;
}

}  // namespace

GlobalSurfaceShellProductionLayerAdapterResult
AdaptGlobalSurfaceShellProductionLayers(
    const GlobalSurfaceShellProductionLayerAdapterRequest& request)
{
    GlobalSurfaceShellProductionLayerAdapterResult result;
    result.protocol = CurrentRgbwsvProtocol();
    if (request.rasterMapping == nullptr
        || request.fullClosure == nullptr
        || request.closureEvidence == nullptr)
    {
        BlockAdapter(
            result,
            SlicePipelineErrorCode::GlobalAdapterInputInvalid,
            "global production layer adapter requires raster mapping, full closure, and final evidence");
        return result;
    }
    if (!HasFixedProtocol(result.protocol))
    {
        BlockAdapter(
            result,
            SlicePipelineErrorCode::GlobalAdapterProtocolMismatch,
            "global production layer adapter requires p0.rgbwsv.2 RGBWSV uint8 black_is_print");
        return result;
    }

    const TextureFillPartitionRasterMappingResult& mapping =
        *request.rasterMapping;
    const TextureFillPartitionFullClosureAdapterResult& closure =
        *request.fullClosure;
    const std::vector<TextureFillPartitionFullClosureLayerEvidence>& evidence =
        *request.closureEvidence;

    if (!mapping.available
        || mapping.status != "diagnostic"
        || !mapping.stats.partitionPass
        || mapping.productionOutputWritten
        || mapping.grid.width <= 0
        || mapping.grid.height <= 0
        || mapping.grid.depth <= 0)
    {
        BlockAdapter(
            result,
            SlicePipelineErrorCode::GlobalAdapterInputInvalid,
            "global production layer adapter requires a passing diagnostic raster mapping");
        return result;
    }
    if (!closure.available
        || !closure.fullClosurePass
        || closure.status != "diagnostic"
        || closure.source != "semantic_masks"
        || closure.confidence != "exact"
        || closure.repairAttempted
        || closure.productionOutputWritten)
    {
        BlockAdapter(
            result,
            SlicePipelineErrorCode::GlobalAdapterClosureRequired,
            "global production layer adapter requires exact passing full-material closure");
        return result;
    }

    const std::size_t layerCount = static_cast<std::size_t>(mapping.grid.depth);
    if (mapping.layers.size() != layerCount
        || closure.layers.size() != layerCount
        || evidence.size() != layerCount)
    {
        BlockAdapter(
            result,
            SlicePipelineErrorCode::GlobalAdapterLayerMismatch,
            "global production layer inputs must contain one aligned true-Z layer list");
        return result;
    }

    result.widthPx = mapping.grid.width;
    result.heightPx = mapping.grid.height;
    result.layerCount = mapping.grid.depth;
    result.layers.reserve(layerCount);
    const std::size_t pixelCount =
        static_cast<std::size_t>(result.widthPx)
        * static_cast<std::size_t>(result.heightPx);
    const std::size_t channelByteCount = pixelCount * kChannelOrder.size();
    const double zTolerance = std::max(
        1.0e-9,
        std::abs(mapping.grid.layerThicknessMm) * 1.0e-9);

    for (int layerIndex{0}; layerIndex < result.layerCount; ++layerIndex)
    {
        const std::size_t index = static_cast<std::size_t>(layerIndex);
        const TextureFillPartitionRasterLayer& mappingLayer =
            mapping.layers.at(index);
        const TextureFillPartitionFullClosureLayerEvidence& evidenceLayer =
            evidence.at(index);
        const TextureFillPartitionFullClosureLayerResult& closureLayer =
            closure.layers.at(index);
        const double expectedZ = mapping.grid.originZMm
            + (static_cast<double>(layerIndex) + 0.5)
                * mapping.grid.layerThicknessMm;

        if (!SameLayerIdentity(
                mappingLayer,
                evidenceLayer,
                closureLayer,
                layerIndex,
                expectedZ,
                zTolerance)
            || evidenceLayer.widthPx != result.widthPx
            || evidenceLayer.heightPx != result.heightPx
            || closureLayer.sidecar.widthPx != result.widthPx
            || closureLayer.sidecar.heightPx != result.heightPx
            || evidenceLayer.channels.size() != channelByteCount
            || !HasValidLayerShapes(
                mappingLayer,
                evidenceLayer,
                closureLayer.sidecar,
                pixelCount)
            || !closureLayer.closurePass)
        {
            BlockAdapter(
                result,
                SlicePipelineErrorCode::GlobalAdapterLayerMismatch,
                "global production layer identity, dimensions, bytes, or closure result do not align");
            return result;
        }
        if (!HasFixedChannelOrder(evidenceLayer.channelOrder))
        {
            BlockAdapter(
                result,
                SlicePipelineErrorCode::GlobalAdapterProtocolMismatch,
                "global production layer evidence must use channel order R G B W S V");
            return result;
        }
        if (!SameSemanticEvidence(
                mappingLayer,
                evidenceLayer,
                closureLayer.sidecar)
            || !TextureRgbMatchesFinalChannels(
                mappingLayer,
                evidenceLayer.channels)
            || !EmptyMaskMatchesFinalChannels(
                closureLayer.sidecar,
                evidenceLayer.channels,
                result.protocol.empty_value))
        {
            BlockAdapter(
                result,
                SlicePipelineErrorCode::GlobalAdapterLayerMismatch,
                "global production bytes no longer match the validated raster and closure evidence");
            return result;
        }

        GlobalSurfaceShellProductionLayer layer;
        layer.output.layerIndex = layerIndex;
        layer.output.zMm = expectedZ;
        layer.output.widthPx = result.widthPx;
        layer.output.heightPx = result.heightPx;
        layer.output.channelOrder = result.protocol.channel_order;
        layer.output.channels = evidenceLayer.channels;
        layer.semantic = closureLayer.sidecar;
        result.layers.push_back(std::move(layer));
    }

    result.available = true;
    result.fullClosurePass = true;
    result.status = "ready_for_writer";
    return result;
}

}  // namespace slicer_core
