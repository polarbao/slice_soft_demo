#include "slicer_core/pipeline/SceneRasterTypes.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace slicer_core
{
namespace
{

constexpr std::size_t kChannelCount{6U};
constexpr std::size_t kCancellationCheckStride{4096U};

bool IsCancellationRequested(
    const api::ICancelToken* cancelToken) noexcept
{
    return cancelToken != nullptr
        && cancelToken->IsCancelRequested();
}

bool SameProtocol(
    const RgbwsvProtocol& first,
    const RgbwsvProtocol& second)
{
    return first.schema == second.schema
        && first.channel_order == second.channel_order
        && first.bit_depth == second.bit_depth
        && first.polarity == second.polarity
        && first.print_value == second.print_value
        && first.empty_value == second.empty_value;
}

bool IsBinaryMask(
    const std::vector<std::uint8_t>& mask,
    const std::size_t expectedSize,
    const api::ICancelToken* cancelToken)
{
    if (mask.size() != expectedSize)
    {
        return false;
    }
    for (std::size_t index{0U}; index < mask.size(); ++index)
    {
        if ((index % kCancellationCheckStride) == 0U
            && IsCancellationRequested(cancelToken))
        {
            return false;
        }
        const std::uint8_t value = mask.at(index);
        if (value != 0U && value != 1U)
        {
            return false;
        }
    }
    return !IsCancellationRequested(cancelToken);
}

bool CheckedPixelCount(
    const int width,
    const int height,
    std::size_t& pixelCount)
{
    if (width <= 0
        || height <= 0
        || static_cast<std::size_t>(height)
            > std::numeric_limits<std::size_t>::max()
                / static_cast<std::size_t>(width))
    {
        return false;
    }
    pixelCount =
        static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height);
    return pixelCount
        <= std::numeric_limits<std::size_t>::max()
            / kChannelCount;
}

bool CheckedAdd(
    const std::size_t first,
    const std::size_t second,
    std::size_t& output)
{
    if (second
        > std::numeric_limits<std::size_t>::max() - first)
    {
        return false;
    }
    output = first + second;
    return true;
}

bool HasValidLayerSequence(
    const SceneInstanceRaster& raster,
    const api::ICancelToken* cancelToken)
{
    std::size_t pixelCount{0U};
    if (!CheckedPixelCount(
            raster.localgrid.widthpx,
            raster.localgrid.heightpx,
            pixelCount)
        || raster.layers.size()
            != static_cast<std::size_t>(
                raster.localgrid.layercount))
    {
        return false;
    }
    const double zTolerance =
        std::max(1.0e-9, raster.localgrid.layerthicknessmm * 1.0e-9);
    for (int layerIndex{0};
         layerIndex < raster.localgrid.layercount;
         ++layerIndex)
    {
        if (IsCancellationRequested(cancelToken))
        {
            return false;
        }
        const SceneInstanceRasterLayer& layer =
            raster.layers.at(static_cast<std::size_t>(layerIndex));
        const double expectedZ = raster.localgrid.originzmm
            + (static_cast<double>(layerIndex) + 0.5)
                * raster.localgrid.layerthicknessmm;
        if (layer.layerindex != layerIndex
            || layer.output.layerIndex != layerIndex
            || layer.output.widthPx != raster.localgrid.widthpx
            || layer.output.heightPx != raster.localgrid.heightpx
            || layer.output.channelOrder
                != raster.protocol.channel_order
            || !std::isfinite(layer.zmm)
            || !std::isfinite(layer.output.zMm)
            || std::abs(layer.zmm - expectedZ) > zTolerance
            || std::abs(layer.output.zMm - expectedZ) > zTolerance
            || layer.output.channels.size()
                != pixelCount * kChannelCount
            || !IsBinaryMask(
                layer.modelownership,
                pixelCount,
                cancelToken)
            || !IsBinaryMask(
                layer.modelvarnishownership,
                pixelCount,
                cancelToken)
            || !IsBinaryMask(
                layer.outervarnishownership,
                pixelCount,
                cancelToken)
            || !IsBinaryMask(
                layer.supportownership,
                pixelCount,
                cancelToken))
        {
            return false;
        }
    }
    return true;
}

bool HasValidComposedLayers(
    const SceneLayerComposeResult& result,
    std::size_t& occupiedPixels,
    std::size_t& emptyPixels,
    const api::ICancelToken* cancelToken)
{
    std::size_t pixelCount{0U};
    if (!SameProtocol(result.protocol, FixedSceneRasterProtocol())
        || !CheckedPixelCount(
            result.grid.widthpx,
            result.grid.heightpx,
            pixelCount)
        || result.layers.size()
            != static_cast<std::size_t>(result.grid.layercount)
        || (!result.layerstatistics.empty()
            && result.layerstatistics.size() != result.layers.size()))
    {
        return false;
    }

    const double zTolerance =
        std::max(1.0e-9, result.grid.layerthicknessmm * 1.0e-9);
    for (int layerIndex{0};
         layerIndex < result.grid.layercount;
         ++layerIndex)
    {
        if (IsCancellationRequested(cancelToken))
        {
            return false;
        }
        const RgbwsvProductionLayer& layer =
            result.layers.at(static_cast<std::size_t>(layerIndex));
        const RgbwsvProductionLayerStatistics* expectedStatistics =
            result.layerstatistics.empty()
                ? nullptr
                : &result.layerstatistics.at(
                    static_cast<std::size_t>(layerIndex));
        const double expectedZ = result.grid.originzmm
            + (static_cast<double>(layerIndex) + 0.5)
                * result.grid.layerthicknessmm;
        if (layer.layerIndex != layerIndex
            || layer.widthPx != result.grid.widthpx
            || layer.heightPx != result.grid.heightpx
            || layer.channelOrder != result.protocol.channel_order
            || !std::isfinite(layer.zMm)
            || std::abs(layer.zMm - expectedZ) > zTolerance
            || layer.channels.size() != pixelCount * kChannelCount
            || (expectedStatistics != nullptr
                && expectedStatistics->layerIndex != layerIndex))
        {
            return false;
        }

        RgbwsvProductionLayerStatistics actualStatistics;
        actualStatistics.layerIndex = layerIndex;
        for (std::size_t pixelIndex{0U};
             pixelIndex < pixelCount;
             ++pixelIndex)
        {
            if ((pixelIndex % kCancellationCheckStride) == 0U
                && IsCancellationRequested(cancelToken))
            {
                return false;
            }
            const std::size_t base = pixelIndex * kChannelCount;
            actualStatistics.emptyPixels[0U] +=
                layer.channels[base] == result.protocol.empty_value;
            actualStatistics.emptyPixels[1U] +=
                layer.channels[base + 1U] == result.protocol.empty_value;
            actualStatistics.emptyPixels[2U] +=
                layer.channels[base + 2U] == result.protocol.empty_value;
            actualStatistics.emptyPixels[3U] +=
                layer.channels[base + 3U] == result.protocol.empty_value;
            actualStatistics.emptyPixels[4U] +=
                layer.channels[base + 4U] == result.protocol.empty_value;
            actualStatistics.emptyPixels[5U] +=
                layer.channels[base + 5U] == result.protocol.empty_value;
            const bool occupied = std::any_of(
                layer.channels.begin()
                    + static_cast<std::ptrdiff_t>(base),
                layer.channels.begin()
                    + static_cast<std::ptrdiff_t>(
                        base + kChannelCount),
                [&result](const std::uint8_t value)
                {
                    return value != result.protocol.empty_value;
                });
            std::size_t next{0U};
            if (!CheckedAdd(
                    occupied ? occupiedPixels : emptyPixels,
                    1U,
                    next))
            {
                return false;
            }
            if (occupied)
            {
                occupiedPixels = next;
            }
            else
            {
                emptyPixels = next;
            }
        }
        for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
        {
            actualStatistics.printPixels[channel] =
                static_cast<std::uint64_t>(pixelCount)
                - actualStatistics.emptyPixels[channel];
        }
        if (expectedStatistics != nullptr
            && (actualStatistics.printPixels
                    != expectedStatistics->printPixels
                || actualStatistics.emptyPixels
                    != expectedStatistics->emptyPixels))
        {
            return false;
        }
    }
    return true;
}

bool HasClosedComposeStatistics(
    const SceneLayerComposeResult& result,
    const std::size_t occupiedPixels,
    const std::size_t emptyPixels)
{
    const SceneLayerComposeStatistics& statistics =
        result.statistics;
    if (statistics.outputlayercount != result.layers.size()
        || statistics.visibleinstancecount
            != statistics.instances.size())
    {
        return false;
    }

    std::size_t totalInstances{0U};
    if (!CheckedAdd(
            statistics.visibleinstancecount,
            statistics.hiddeninstancecount,
            totalInstances)
        || totalInstances != statistics.totalinstancecount)
    {
        return false;
    }

    std::size_t ownedPixels{0U};
    if (!CheckedAdd(
            statistics.modelpixels,
            statistics.outervarnishpixels,
            ownedPixels)
        || !CheckedAdd(
            ownedPixels,
            statistics.supportpixels,
            ownedPixels)
        || ownedPixels != occupiedPixels
        || statistics.emptypixels != emptyPixels)
    {
        return false;
    }

    std::unordered_set<std::string> instanceIds;
    std::size_t instanceModelPixels{0U};
    std::size_t instanceOuterVarnishPixels{0U};
    std::size_t instanceSupportPixels{0U};
    for (const SceneInstanceComposeStatistics& instance :
         statistics.instances)
    {
        if (instance.instanceid.empty()
            || !instanceIds.insert(instance.instanceid).second
            || !CheckedAdd(
                instanceModelPixels,
                instance.modelpixels,
                instanceModelPixels)
            || !CheckedAdd(
                instanceOuterVarnishPixels,
                instance.outervarnishpixels,
                instanceOuterVarnishPixels)
            || !CheckedAdd(
                instanceSupportPixels,
                instance.supportpixels,
                instanceSupportPixels))
        {
            return false;
        }
    }
    return instanceModelPixels == statistics.modelpixels
        && instanceOuterVarnishPixels
            == statistics.outervarnishpixels
        && instanceSupportPixels == statistics.supportpixels;
}

}  // namespace

bool SceneRasterGrid::IsValid() const
{
    return widthpx > 0
        && heightpx > 0
        && layercount > 0
        && std::isfinite(originxmm)
        && std::isfinite(originymm)
        && std::isfinite(originzmm)
        && std::isfinite(pitchxmm)
        && std::isfinite(pitchymm)
        && std::isfinite(layerthicknessmm)
        && pitchxmm > 0.0
        && pitchymm > 0.0
        && layerthicknessmm > 0.0;
}

bool SceneLayerComposeResult::IsValid() const
{
    return IsValid(nullptr);
}

bool SceneLayerComposeResult::IsValid(
    const api::ICancelToken* cancelToken) const
{
    if (IsCancellationRequested(cancelToken)
        || !available
        || status != "ready_for_writer"
        || error.has_value()
        || sceneid.empty()
        || !grid.IsValid()
        || !std::isfinite(composems)
        || composems < 0.0)
    {
        return false;
    }

    std::size_t occupiedPixels{0U};
    std::size_t emptyPixels{0U};
    return HasValidComposedLayers(
               *this,
               occupiedPixels,
               emptyPixels,
               cancelToken)
        && HasClosedComposeStatistics(
            *this,
            occupiedPixels,
            emptyPixels);
}

bool SceneRasterAdapterResult::IsValid() const
{
    return IsValid(nullptr);
}

bool SceneRasterAdapterResult::IsValid(
    const api::ICancelToken* cancelToken) const
{
    return !IsCancellationRequested(cancelToken)
        && available
        && status == "ready_for_composer"
        && !productionoutputwritten
        && !error.has_value()
        && !raster.sceneid.empty()
        && !raster.modelid.empty()
        && !raster.instanceid.empty()
        && raster.visible
        && raster.admitted
        && raster.transformrevision
            == raster.admittedtransformrevision
        && !raster.transformhash.empty()
        && raster.transformhash
            == raster.admittedtransformhash
        && raster.localgrid.IsValid()
        && SameProtocol(
            raster.protocol,
            FixedSceneRasterProtocol())
        && HasValidLayerSequence(raster, cancelToken);
}

std::string_view SceneRasterErrorCodeName(const SceneRasterErrorCode code)
{
    switch (code)
    {
        case SceneRasterErrorCode::None:
            return "NONE";
        case SceneRasterErrorCode::AdmissionRequired:
            return "SCENE_RASTER_ADMISSION_REQUIRED";
        case SceneRasterErrorCode::GridInvalid:
            return "SCENE_RASTER_GRID_INVALID";
        case SceneRasterErrorCode::ProtocolMismatch:
            return "SCENE_RASTER_PROTOCOL_MISMATCH";
        case SceneRasterErrorCode::ResolutionMismatch:
            return "SCENE_RASTER_RESOLUTION_MISMATCH";
        case SceneRasterErrorCode::LayerSequenceMismatch:
            return "SCENE_RASTER_LAYER_SEQUENCE_MISMATCH";
        case SceneRasterErrorCode::OffsetNotIntegral:
            return "SCENE_RASTER_OFFSET_NOT_INTEGRAL";
        case SceneRasterErrorCode::LayerSizeInvalid:
            return "SCENE_RASTER_LAYER_SIZE_INVALID";
        case SceneRasterErrorCode::InstanceIdentityInvalid:
            return "SCENE_RASTER_INSTANCE_IDENTITY_INVALID";
        case SceneRasterErrorCode::InstanceOverlap:
            return "SCENE_RASTER_INSTANCE_OVERLAP";
        case SceneRasterErrorCode::MaterialConflict:
            return "SCENE_RASTER_MATERIAL_CONFLICT";
        case SceneRasterErrorCode::ClosureFailed:
            return "SCENE_RASTER_CLOSURE_FAILED";
        case SceneRasterErrorCode::RevisionStale:
            return "SCENE_RASTER_REVISION_STALE";
        case SceneRasterErrorCode::PipelineModeMismatch:
            return "SCENE_RASTER_PIPELINE_MODE_MISMATCH";
        case SceneRasterErrorCode::ProducerFailed:
            return "SCENE_RASTER_PRODUCER_FAILED";
        case SceneRasterErrorCode::Cancelled:
            return "SCENE_RASTER_CANCELLED";
    }
    return "SCENE_RASTER_UNKNOWN";
}

RgbwsvProtocol FixedSceneRasterProtocol()
{
    return RgbwsvProtocol{};
}

std::vector<std::uint8_t> BuildModelVarnishOwnership(
    const RgbwsvProductionLayer& output,
    const std::vector<std::uint8_t>& modelOwnership,
    const RgbwsvProtocol& protocol)
{
    constexpr std::size_t varnishChannel{5U};
    std::size_t pixelCount{0U};
    if (!CheckedPixelCount(
            output.widthPx,
            output.heightPx,
            pixelCount)
        || modelOwnership.size() != pixelCount
        || output.channels.size()
            != pixelCount * kChannelCount)
    {
        throw std::invalid_argument(
            "model varnish ownership requires aligned layer bytes and model mask");
    }

    std::vector<std::uint8_t> ownership(pixelCount, 0U);
    for (std::size_t pixelIndex{0U};
         pixelIndex < pixelCount;
         ++pixelIndex)
    {
        ownership.at(pixelIndex) =
            modelOwnership.at(pixelIndex) != 0U
                && output.channels.at(
                    pixelIndex * kChannelCount + varnishChannel)
                    != protocol.empty_value
            ? 1U
            : 0U;
    }
    return ownership;
}

}  // namespace slicer_core
