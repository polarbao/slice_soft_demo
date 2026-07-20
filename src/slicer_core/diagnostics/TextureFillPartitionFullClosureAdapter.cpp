#include "slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.h"

#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <string>
#include <utility>

namespace slicer_core
{
namespace
{

using Clock = std::chrono::steady_clock;

constexpr std::array<const char*, 6> kChannelOrder{"R", "G", "B", "W", "S", "V"};
constexpr std::size_t kSupportChannel{4U};
constexpr std::size_t kVarnishChannel{5U};

ValidationIssue MakeFullClosureIssue(
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    return MakeValidationIssue(
        TextureFillPartitionErrorCodeName(code),
        ValidationSeverity::Error,
        message);
}

void BlockFullClosure(
    TextureFillPartitionFullClosureAdapterResult& result,
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    result.available = false;
    result.status = "blocked";
    result.layers.clear();
    result.issues.push_back(MakeFullClosureIssue(code, message));
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

bool HasFixedChannelOrder(const std::array<std::string, 6>& order)
{
    for (std::size_t index{0U}; index < kChannelOrder.size(); ++index)
    {
        if (order.at(index) != kChannelOrder.at(index))
        {
            return false;
        }
    }
    return true;
}

bool IsEmptyPixel(
    const std::vector<std::uint8_t>& channels,
    const std::size_t pixelIndex)
{
    for (std::size_t channel{0U}; channel < kChannelOrder.size(); ++channel)
    {
        if (channels.at(pixelIndex * kChannelOrder.size() + channel) != 255U)
        {
            return false;
        }
    }
    return true;
}

bool IsChannelPrinted(
    const std::vector<std::uint8_t>& channels,
    const std::size_t pixelIndex,
    const std::size_t channel)
{
    return channels.at(pixelIndex * kChannelOrder.size() + channel) < 255U;
}

bool ValidateRoot(
    const TextureFillPartitionFullClosureAdapterRequest& request,
    TextureFillPartitionFullClosureAdapterResult& result)
{
    if (request.rasterMapping == nullptr || request.layers == nullptr)
    {
        BlockFullClosure(
            result,
            TextureFillPartitionErrorCode::FullClosureInputInvalid,
            "full closure requires raster mapping and material layer evidence");
        return false;
    }
    if ((request.connectivity != 4 && request.connectivity != 8)
        || request.maxGapPx <= 0)
    {
        BlockFullClosure(
            result,
            TextureFillPartitionErrorCode::FullClosureInputInvalid,
            "full closure connectivity and gap radius are invalid");
        return false;
    }

    const TextureFillPartitionRasterMappingResult& mapping = *request.rasterMapping;
    if (!mapping.available
        || mapping.status != "diagnostic"
        || !mapping.stats.partitionPass
        || mapping.grid.width <= 0
        || mapping.grid.height <= 0
        || mapping.grid.depth <= 0
        || mapping.layers.size() != static_cast<std::size_t>(mapping.grid.depth)
        || request.layers->size() != mapping.layers.size())
    {
        BlockFullClosure(
            result,
            TextureFillPartitionErrorCode::FullClosureInputInvalid,
            "full closure requires an available diagnostic raster mapping and aligned layers");
        return false;
    }
    return true;
}

bool ValidateLayerMasks(
    const TextureFillPartitionRasterLayer& mappingLayer,
    const TextureFillPartitionFullClosureLayerEvidence& evidence,
    const std::size_t pixelCount,
    TextureFillPartitionFullClosureAdapterResult& result)
{
    const std::array<const std::vector<std::uint8_t>*, 9> masks{
        &mappingLayer.modelMask,
        &mappingLayer.textureSurfaceMask,
        &mappingLayer.modelFillMask,
        &evidence.supportFillMask,
        &evidence.internalVoidSupportMask,
        &evidence.surfaceVarnishMask,
        &evidence.outerVarnishShellMask,
        &evidence.modelEnvelopeMask,
        &evidence.supportRequiredMask,
    };
    for (const std::vector<std::uint8_t>* mask : masks)
    {
        if (mask->size() != pixelCount || !IsBinaryMask(*mask))
        {
            BlockFullClosure(
                result,
                TextureFillPartitionErrorCode::FullClosureMaskInvalid,
                "full closure masks must be binary and match the raster layer");
            return false;
        }
    }
    if (mappingLayer.textureRgb.size() != pixelCount)
    {
        BlockFullClosure(
            result,
            TextureFillPartitionErrorCode::FullClosureMaskInvalid,
            "full closure texture RGB evidence must match the raster layer");
        return false;
    }
    return true;
}

bool ValidateOwnership(
    const TextureFillPartitionRasterLayer& mappingLayer,
    const TextureFillPartitionFullClosureLayerEvidence& evidence,
    TextureFillPartitionFullClosureAdapterResult& result)
{
    for (std::size_t index{0U}; index < mappingLayer.modelMask.size(); ++index)
    {
        const bool model = mappingLayer.modelMask.at(index) != 0U;
        const bool texture = mappingLayer.textureSurfaceMask.at(index) != 0U;
        const bool fill = mappingLayer.modelFillMask.at(index) != 0U;
        const bool support = evidence.supportFillMask.at(index) != 0U;
        const bool internalVoid = evidence.internalVoidSupportMask.at(index) != 0U;
        const bool surfaceVarnish = evidence.surfaceVarnishMask.at(index) != 0U;
        const bool outerVarnish = evidence.outerVarnishShellMask.at(index) != 0U;
        const bool modelEnvelope = evidence.modelEnvelopeMask.at(index) != 0U;
        const bool supportRequired = evidence.supportRequiredMask.at(index) != 0U;

        if ((texture && fill) || (texture && !model) || (fill && !model)
            || (model && !texture && !fill))
        {
            BlockFullClosure(
                result,
                TextureFillPartitionErrorCode::FullClosureMaskInvalid,
                "raster model must remain an exact texture/fill complement");
            return false;
        }
        if (!modelEnvelope && model)
        {
            BlockFullClosure(
                result,
                TextureFillPartitionErrorCode::FullClosureMaskInvalid,
                "model envelope must contain every model pixel");
            return false;
        }
        if (surfaceVarnish && !model)
        {
            BlockFullClosure(
                result,
                TextureFillPartitionErrorCode::FullClosureMaskInvalid,
                "surface varnish must be a subset of the model domain");
            return false;
        }
        if (internalVoid && (!support || !modelEnvelope || model))
        {
            BlockFullClosure(
                result,
                TextureFillPartitionErrorCode::FullClosureMaskInvalid,
                "internal-void support must be support inside the model envelope and outside model material");
            return false;
        }
        if (support && !supportRequired)
        {
            BlockFullClosure(
                result,
                TextureFillPartitionErrorCode::FullClosureMaskInvalid,
                "final support must belong to the support-required domain");
            return false;
        }
        if ((model && support) || (model && outerVarnish) || (outerVarnish && support))
        {
            BlockFullClosure(
                result,
                TextureFillPartitionErrorCode::FullClosurePriorityConflict,
                "final masks violate Model > OuterVarnishShell > Support priority");
            return false;
        }
    }
    return true;
}

}  // namespace

TextureFillPartitionFullClosureAdapterResult AdaptTextureFillPartitionFullClosure(
    const TextureFillPartitionFullClosureAdapterRequest& request)
{
    TextureFillPartitionFullClosureAdapterResult result;
    if (!ValidateRoot(request, result))
    {
        return result;
    }

    const auto analysisStart = Clock::now();
    const TextureFillPartitionRasterMappingResult& mapping = *request.rasterMapping;
    const std::vector<TextureFillPartitionFullClosureLayerEvidence>& evidenceLayers =
        *request.layers;
    result.allTexture = mapping.allTexture;
    if (result.allTexture)
    {
        result.colorFillApplicability = "not_applicable";
        result.allTextureReason = "all_texture_partition";
    }
    result.layers.reserve(mapping.layers.size());

    for (int layerIndex{0}; layerIndex < mapping.grid.depth; ++layerIndex)
    {
        const TextureFillPartitionRasterLayer& mappingLayer = mapping.layers.at(
            static_cast<std::size_t>(layerIndex));
        const TextureFillPartitionFullClosureLayerEvidence& evidence = evidenceLayers.at(
            static_cast<std::size_t>(layerIndex));
        const double expectedZ = mapping.grid.originZMm
            + (static_cast<double>(layerIndex) + 0.5)
                * mapping.grid.layerThicknessMm;
        const double zTolerance = std::max(
            1.0e-9,
            std::abs(mapping.grid.layerThicknessMm) * 1.0e-9);
        if (mappingLayer.layerIndex != layerIndex
            || evidence.layerIndex != layerIndex
            || !std::isfinite(mappingLayer.zMm)
            || !std::isfinite(evidence.zMm)
            || std::abs(mappingLayer.zMm - expectedZ) > zTolerance
            || std::abs(evidence.zMm - expectedZ) > zTolerance)
        {
            BlockFullClosure(
                result,
                TextureFillPartitionErrorCode::FullClosureLayerOrderInvalid,
                "full closure layer index and Z must match true raster order");
            return result;
        }
        if (evidence.widthPx != mapping.grid.width
            || evidence.heightPx != mapping.grid.height)
        {
            BlockFullClosure(
                result,
                TextureFillPartitionErrorCode::FullClosureInputInvalid,
                "full closure evidence dimensions must match the raster mapping");
            return result;
        }
        if (!HasFixedChannelOrder(evidence.channelOrder))
        {
            BlockFullClosure(
                result,
                TextureFillPartitionErrorCode::FullClosureChannelOrderInvalid,
                "full closure requires channel order R G B W S V");
            return result;
        }

        const std::size_t pixelCount = static_cast<std::size_t>(mapping.grid.width)
            * static_cast<std::size_t>(mapping.grid.height);
        if (evidence.channels.size() != pixelCount * kChannelOrder.size())
        {
            BlockFullClosure(
                result,
                TextureFillPartitionErrorCode::FullClosureInputInvalid,
                "full closure RGBWSV bytes must match the raster layer");
            return result;
        }
        if (!ValidateLayerMasks(mappingLayer, evidence, pixelCount, result)
            || !ValidateOwnership(mappingLayer, evidence, result))
        {
            return result;
        }

        TextureFillPartitionFullClosureLayerResult layerResult;
        layerResult.layerIndex = layerIndex;
        layerResult.zMm = evidence.zMm;
        MaterialClosureSemanticLayerInput& sidecar = layerResult.sidecar;
        sidecar.layerIndex = layerIndex;
        sidecar.zMm = evidence.zMm;
        sidecar.widthPx = mapping.grid.width;
        sidecar.heightPx = mapping.grid.height;
        sidecar.textureSurfaceMask = mappingLayer.textureSurfaceMask;
        sidecar.modelFillMask = mappingLayer.modelFillMask;
        sidecar.modelMaterialMask = mappingLayer.modelMask;
        sidecar.supportFillMask = evidence.supportFillMask;
        sidecar.internalVoidSupportMask = evidence.internalVoidSupportMask;
        sidecar.surfaceVarnishMask = evidence.surfaceVarnishMask;
        sidecar.outerVarnishShellMask = evidence.outerVarnishShellMask;
        sidecar.modelEnvelopeMask = evidence.modelEnvelopeMask;
        sidecar.supportRequiredMask = evidence.supportRequiredMask;
        sidecar.expectedOccupiedDomainMask.assign(pixelCount, 0U);
        sidecar.layerEmptyMask.assign(pixelCount, 0U);

        for (std::size_t pixelIndex{0U}; pixelIndex < pixelCount; ++pixelIndex)
        {
            const bool model = sidecar.modelMaterialMask.at(pixelIndex) != 0U;
            const bool support = sidecar.supportFillMask.at(pixelIndex) != 0U;
            const bool surfaceVarnish = sidecar.surfaceVarnishMask.at(pixelIndex) != 0U;
            const bool outerVarnish = sidecar.outerVarnishShellMask.at(pixelIndex) != 0U;
            const bool expected = sidecar.modelEnvelopeMask.at(pixelIndex) != 0U
                || sidecar.supportRequiredMask.at(pixelIndex) != 0U
                || outerVarnish;
            const bool empty = IsEmptyPixel(evidence.channels, pixelIndex);
            const bool supportPrinted = IsChannelPrinted(
                evidence.channels,
                pixelIndex,
                kSupportChannel);
            const bool varnishPrinted = IsChannelPrinted(
                evidence.channels,
                pixelIndex,
                kVarnishChannel);
            sidecar.expectedOccupiedDomainMask.at(pixelIndex) = expected ? 1U : 0U;
            sidecar.layerEmptyMask.at(pixelIndex) = empty ? 1U : 0U;

            layerResult.expectedDomainGapPixels += expected && empty ? 1U : 0U;
            layerResult.modelDomainGapPixels += model && empty ? 1U : 0U;
            layerResult.supportRequiredGapPixels +=
                sidecar.supportRequiredMask.at(pixelIndex) != 0U && empty ? 1U : 0U;
            layerResult.outerVarnishGapPixels += outerVarnish && empty ? 1U : 0U;
            layerResult.unexpectedOccupiedPixels += !expected && !empty ? 1U : 0U;
            layerResult.supportChannelMismatchPixels +=
                support != supportPrinted ? 1U : 0U;
            layerResult.varnishChannelMismatchPixels +=
                ((surfaceVarnish || outerVarnish) && !varnishPrinted)
                    || (!(surfaceVarnish || outerVarnish)
                        && varnishPrinted
                        && sidecar.modelFillMask.at(pixelIndex) == 0U)
                ? 1U
                : 0U;
        }

        try
        {
            layerResult.semantic = DetectMaterialClosureSemanticLayer(
                sidecar,
                request.connectivity,
                request.maxGapPx);
        }
        catch (const std::exception& error)
        {
            BlockFullClosure(
                result,
                TextureFillPartitionErrorCode::FullClosureInputInvalid,
                std::string{"12D semantic detector rejected full closure evidence: "}
                    + error.what());
            return result;
        }

        const std::uint64_t channelMismatchPixels =
            layerResult.supportChannelMismatchPixels
            + layerResult.varnishChannelMismatchPixels;
        layerResult.closurePass = layerResult.expectedDomainGapPixels == 0U
            && layerResult.unexpectedOccupiedPixels == 0U
            && channelMismatchPixels == 0U
            && layerResult.semantic.gapPixels == 0;
        result.totalExpectedDomainGapPixels += layerResult.expectedDomainGapPixels;
        result.totalModelDomainGapPixels += layerResult.modelDomainGapPixels;
        result.totalSupportRequiredGapPixels += layerResult.supportRequiredGapPixels;
        result.totalOuterVarnishGapPixels += layerResult.outerVarnishGapPixels;
        result.totalUnexpectedOccupiedPixels += layerResult.unexpectedOccupiedPixels;
        result.totalSupportChannelMismatchPixels +=
            layerResult.supportChannelMismatchPixels;
        result.totalVarnishChannelMismatchPixels +=
            layerResult.varnishChannelMismatchPixels;
        result.totalSemanticChannelMismatchPixels += channelMismatchPixels;
        result.totalColorFillGapPixels += static_cast<std::uint64_t>(
            layerResult.semantic.colorFillGapPixels);
        result.totalModelSupportGapPixels += static_cast<std::uint64_t>(
            layerResult.semantic.modelSupportGapPixels);
        result.totalColorSupportGapPixels += static_cast<std::uint64_t>(
            layerResult.semantic.colorSupportGapPixels);
        result.totalInternalVoidGapPixels += static_cast<std::uint64_t>(
            layerResult.semantic.internalVoidGapPixels);
        result.totalVarnishSupportGapPixels += static_cast<std::uint64_t>(
            layerResult.semantic.varnishSupportGapPixels);
        result.layers.push_back(std::move(layerResult));
    }

    result.available = true;
    result.source = "semantic_masks";
    result.confidence = "exact";
    result.modelClosureStatus = result.totalModelDomainGapPixels == 0U
            && result.totalColorFillGapPixels == 0U
        ? "pass"
        : "fail";
    result.supportClosureStatus = result.totalSupportRequiredGapPixels == 0U
            && result.totalModelSupportGapPixels == 0U
            && result.totalColorSupportGapPixels == 0U
            && result.totalInternalVoidGapPixels == 0U
            && result.totalSupportChannelMismatchPixels == 0U
        ? "pass"
        : "fail";
    result.varnishClosureStatus = result.totalOuterVarnishGapPixels == 0U
            && result.totalVarnishSupportGapPixels == 0U
            && result.totalVarnishChannelMismatchPixels == 0U
        ? "pass"
        : "fail";
    result.fullClosurePass = std::all_of(
        result.layers.begin(),
        result.layers.end(),
        [](const TextureFillPartitionFullClosureLayerResult& layer)
        {
            return layer.closurePass;
        });
    result.status = result.fullClosurePass ? "diagnostic" : "fail";
    result.analysisMs = std::chrono::duration<double, std::milli>(
        Clock::now() - analysisStart).count();

    if (result.totalExpectedDomainGapPixels > 0U)
    {
        result.issues.push_back(MakeFullClosureIssue(
            TextureFillPartitionErrorCode::FullClosureGapDetected,
            "empty pixels exist inside the exact expected occupied domain: "
                + std::to_string(result.totalExpectedDomainGapPixels)));
    }
    if (result.totalSemanticChannelMismatchPixels > 0U)
    {
        result.issues.push_back(MakeFullClosureIssue(
            TextureFillPartitionErrorCode::FullClosureSemanticMismatch,
            "semantic masks and final RGBWSV channels disagree: "
                + std::to_string(result.totalSemanticChannelMismatchPixels)));
    }
    if (result.totalUnexpectedOccupiedPixels > 0U)
    {
        result.issues.push_back(MakeFullClosureIssue(
            TextureFillPartitionErrorCode::FullClosureUnexpectedMaterial,
            "printed material exists outside the expected occupied domain: "
                + std::to_string(result.totalUnexpectedOccupiedPixels)));
    }
    return result;
}

}  // namespace slicer_core
