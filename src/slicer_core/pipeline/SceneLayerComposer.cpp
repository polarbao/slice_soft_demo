#include "slicer_core/pipeline/SceneLayerComposer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

constexpr std::size_t kChannelCount{6U};
constexpr std::size_t kSupportChannel{4U};
constexpr std::size_t kVarnishChannel{5U};
constexpr std::size_t kCancellationCheckStride{4096U};

struct InstancePlacement
{
    const SceneInstanceRaster* instance{nullptr};
    int offsetx{0};
    int offsety{0};
    int offsetz{0};
    std::size_t statisticsindex{0U};
};

bool CheckedMultiply(
    const std::size_t first,
    const std::size_t second,
    std::size_t& output)
{
    if (first != 0U
        && second > std::numeric_limits<std::size_t>::max() / first)
    {
        return false;
    }
    output = first * second;
    return true;
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

bool ComputeLayerSizes(
    const SceneRasterGrid& grid,
    std::size_t& pixelCount,
    std::size_t& byteCount)
{
    if (!grid.IsValid()
        || !CheckedMultiply(
            static_cast<std::size_t>(grid.widthpx),
            static_cast<std::size_t>(grid.heightpx),
            pixelCount)
        || !CheckedMultiply(pixelCount, kChannelCount, byteCount))
    {
        return false;
    }
    return true;
}

bool SameChannelOrder(
    const std::array<std::string, 6>& first,
    const std::array<std::string, 6>& second)
{
    return first == second;
}

bool SameProtocol(
    const RgbwsvProtocol& first,
    const RgbwsvProtocol& second)
{
    return first.schema == second.schema
        && SameChannelOrder(first.channel_order, second.channel_order)
        && first.bit_depth == second.bit_depth
        && first.polarity == second.polarity
        && first.print_value == second.print_value
        && first.empty_value == second.empty_value;
}

bool HasFixedProtocol(const RgbwsvProtocol& protocol)
{
    return SameProtocol(protocol, FixedSceneRasterProtocol());
}

double SamplingTolerance(
    const double first,
    const double second,
    const double requested)
{
    return std::max(
        requested,
        std::max(std::abs(first), std::abs(second)) * 1.0e-9);
}

bool SameSampling(
    const double first,
    const double second,
    const double requested)
{
    return std::isfinite(first)
        && std::isfinite(second)
        && std::abs(first - second)
            <= SamplingTolerance(first, second, requested);
}

bool QuantizeOffset(
    const double localOrigin,
    const double globalOrigin,
    const double pitch,
    const double tolerance,
    int& offset)
{
    if (!std::isfinite(localOrigin)
        || !std::isfinite(globalOrigin)
        || !std::isfinite(pitch)
        || pitch <= 0.0)
    {
        return false;
    }
    const double raw = (localOrigin - globalOrigin) / pitch;
    const double rounded = std::round(raw);
    if (!std::isfinite(raw)
        || std::abs(raw - rounded) > tolerance
        || rounded < static_cast<double>(std::numeric_limits<int>::min())
        || rounded > static_cast<double>(std::numeric_limits<int>::max()))
    {
        return false;
    }
    offset = static_cast<int>(rounded);
    return true;
}

bool IsBinaryMask(
    const std::vector<std::uint8_t>& mask,
    const std::size_t expectedSize)
{
    return mask.size() == expectedSize
        && std::all_of(
            mask.begin(),
            mask.end(),
            [](const std::uint8_t value)
            {
                return value == 0U || value == 1U;
            });
}

std::size_t ChannelIndex(
    const std::size_t pixelIndex,
    const std::size_t channel)
{
    return pixelIndex * kChannelCount + channel;
}

bool IsEmptySourcePixel(
    const SceneInstanceRasterLayer& layer,
    const std::size_t pixelIndex,
    const std::uint8_t emptyValue)
{
    for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
    {
        if (layer.output.channels.at(ChannelIndex(pixelIndex, channel))
            != emptyValue)
        {
            return false;
        }
    }
    return true;
}

SceneRasterOwnership ResolveOwnership(
    const SceneInstanceRasterLayer& layer,
    const std::size_t pixelIndex)
{
    if (layer.modelownership.at(pixelIndex) != 0U)
    {
        return SceneRasterOwnership::Model;
    }
    if (layer.outervarnishownership.at(pixelIndex) != 0U)
    {
        return SceneRasterOwnership::OuterVarnish;
    }
    if (layer.supportownership.at(pixelIndex) != 0U)
    {
        return SceneRasterOwnership::Support;
    }
    return SceneRasterOwnership::Empty;
}

bool SourcePixelHasClosure(
    const SceneInstanceRasterLayer& layer,
    const std::size_t pixelIndex,
    const SceneRasterOwnership ownership,
    const RgbwsvProtocol& protocol)
{
    const std::size_t base = pixelIndex * kChannelCount;
    if (ownership == SceneRasterOwnership::Empty)
    {
        return IsEmptySourcePixel(layer, pixelIndex, protocol.empty_value);
    }
    if (ownership == SceneRasterOwnership::Support)
    {
        for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
        {
            const std::uint8_t expected =
                channel == kSupportChannel
                    ? protocol.print_value
                    : protocol.empty_value;
            if (layer.output.channels.at(base + channel) != expected)
            {
                return false;
            }
        }
        return true;
    }
    if (ownership == SceneRasterOwnership::OuterVarnish)
    {
        for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
        {
            std::uint8_t expected = protocol.empty_value;
            if (channel == kVarnishChannel
                || (channel == kSupportChannel
                    && layer.supportownership.at(pixelIndex) != 0U))
            {
                expected = protocol.print_value;
            }
            if (layer.output.channels.at(base + channel) != expected)
            {
                return false;
            }
        }
        return true;
    }

    bool modelMaterialPresent{false};
    for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
    {
        const std::uint8_t value = layer.output.channels.at(base + channel);
        if (channel == kSupportChannel)
        {
            const std::uint8_t expected =
                layer.supportownership.at(pixelIndex) != 0U
                    ? protocol.print_value
                    : protocol.empty_value;
            if (value != expected)
            {
                return false;
            }
            continue;
        }
        if (channel == kVarnishChannel)
        {
            const bool modelVarnish =
                layer.modelvarnishownership.at(pixelIndex) != 0U;
            if (modelVarnish)
            {
                if (value == protocol.empty_value)
                {
                    return false;
                }
                modelMaterialPresent = true;
            }
            else if (
                value != protocol.empty_value
                && layer.outervarnishownership.at(pixelIndex) == 0U)
            {
                return false;
            }
            continue;
        }
        if (channel < kSupportChannel && value != protocol.empty_value)
        {
            modelMaterialPresent = true;
        }
    }
    return modelMaterialPresent;
}

void Block(
    SceneLayerComposeResult& result,
    const SceneRasterErrorCode code,
    const SceneLayerComposeRequest& request,
    const std::string& field,
    const std::string& message,
    const SceneInstanceRaster* instance = nullptr,
    const SceneInstanceRaster* other = nullptr,
    const int layerIndex = -1)
{
    result.available = false;
    result.status = "blocked";
    result.layers.clear();
    result.layerstatistics.clear();
    result.statistics.outputlayercount = 0U;
    result.statistics.modelpixels = 0U;
    result.statistics.outervarnishpixels = 0U;
    result.statistics.supportpixels = 0U;
    result.statistics.emptypixels = 0U;
    result.statistics.instances.clear();
    SceneRasterError error;
    error.code = code;
    error.sceneid = request.sceneid;
    error.field = field;
    error.message = message;
    error.layerindex = layerIndex;
    if (instance != nullptr)
    {
        error.modelid = instance->modelid;
        error.instanceid = instance->instanceid;
    }
    if (other != nullptr)
    {
        error.otherinstanceid = other->instanceid;
    }
    result.error = std::move(error);
}

#include "slicer_core/pipeline/SceneLayerComposerCancellation.h"

bool ValidateRequest(
    const SceneLayerComposeRequest& request,
    SceneLayerComposeResult& result,
    std::size_t& globalPixelCount,
    std::size_t& globalByteCount)
{
    if (request.sceneid.empty()
        || !request.globalgrid.IsValid()
        || !std::isfinite(request.quantizationtolerance)
        || request.quantizationtolerance < 0.0
        || !ComputeLayerSizes(
            request.globalgrid,
            globalPixelCount,
            globalByteCount))
    {
        Block(
            result,
            SceneRasterErrorCode::GridInvalid,
            request,
            "globalgrid",
            "scene raster requires a finite positive global grid and checked layer size");
        return false;
    }
    if (!HasFixedProtocol(request.protocol))
    {
        Block(
            result,
            SceneRasterErrorCode::ProtocolMismatch,
            request,
            "protocol",
            "scene raster requires p0.rgbwsv.2 RGBWSV uint8 black_is_print");
        return false;
    }
    if (request.currentscenerevision != request.expectedscenerevision)
    {
        Block(
            result,
            SceneRasterErrorCode::RevisionStale,
            request,
            "scenerevision",
            "scene raster revision changed after admission");
        return false;
    }
    if (!request.admissionpassed)
    {
        Block(
            result,
            SceneRasterErrorCode::AdmissionRequired,
            request,
            "admissionpassed",
            "scene raster composition requires passing scene admission");
        return false;
    }
    return true;
}

bool ValidateLayer(
    const SceneLayerComposeRequest& request,
    const SceneInstanceRaster& instance,
    const SceneInstanceRasterLayer& layer,
    const int expectedLayerIndex,
    const std::size_t pixelCount,
    const std::size_t byteCount,
    SceneLayerComposeResult& result,
    SceneInstanceComposeStatistics::RasterStatistics& statistics)
{
    if (StopIfCancellationRequested(
            request, result, "composition.layer_validation", expectedLayerIndex))
    {
        return false;
    }
    const double expectedZ = instance.localgrid.originzmm
        + (static_cast<double>(expectedLayerIndex) + 0.5)
            * instance.localgrid.layerthicknessmm;
    const double zTolerance = SamplingTolerance(
        expectedZ,
        layer.zmm,
        request.quantizationtolerance
            * instance.localgrid.layerthicknessmm);
    if (layer.layerindex != expectedLayerIndex
        || layer.output.layerIndex != expectedLayerIndex
        || !std::isfinite(layer.zmm)
        || !std::isfinite(layer.output.zMm)
        || std::abs(layer.zmm - expectedZ) > zTolerance
        || std::abs(layer.output.zMm - expectedZ) > zTolerance)
    {
        Block(
            result,
            SceneRasterErrorCode::LayerSequenceMismatch,
            request,
            "layers.layerindex",
            "instance layers must be complete, ordered, and aligned to local true-Z centers",
            &instance,
            nullptr,
            expectedLayerIndex);
        return false;
    }
    if (layer.output.widthPx != instance.localgrid.widthpx
        || layer.output.heightPx != instance.localgrid.heightpx
        || layer.output.channels.size() != byteCount
        || !IsBinaryMask(layer.modelownership, pixelCount)
        || !IsBinaryMask(
            layer.modelvarnishownership,
            pixelCount)
        || !IsBinaryMask(layer.outervarnishownership, pixelCount)
        || !IsBinaryMask(layer.supportownership, pixelCount))
    {
        Block(
            result,
            SceneRasterErrorCode::LayerSizeInvalid,
            request,
            "layers.bytes",
            "instance layer dimensions, RGBWSV bytes, and ownership masks must match the local grid",
            &instance,
            nullptr,
            expectedLayerIndex);
        return false;
    }
    if (!SameChannelOrder(
            layer.output.channelOrder,
            request.protocol.channel_order))
    {
        Block(
            result,
            SceneRasterErrorCode::ProtocolMismatch,
            request,
            "layers.channelorder",
            "instance layer channel order must remain R G B W S V",
            &instance,
            nullptr,
            expectedLayerIndex);
        return false;
    }
    for (std::size_t pixelIndex{0U}; pixelIndex < pixelCount; ++pixelIndex)
    {
        if ((pixelIndex % kCancellationCheckStride) == 0U
            && StopIfCancellationRequested(
                request, result, "composition.source_closure", expectedLayerIndex))
        {
            return false;
        }
        const SceneRasterOwnership ownership =
            ResolveOwnership(layer, pixelIndex);
        if (!SourcePixelHasClosure(
                layer,
                pixelIndex,
                ownership,
                request.protocol))
        {
            const std::size_t base = pixelIndex * kChannelCount;
            std::string message{
                "instance RGBWSV bytes do not close against material ownership; pixel="};
            message += std::to_string(pixelIndex);
            message += " values=";
            for (std::size_t channel{0U};
                 channel < kChannelCount;
                 ++channel)
            {
                if (channel != 0U)
                {
                    message.push_back(',');
                }
                message += std::to_string(
                    layer.output.channels.at(base + channel));
            }
            message += " ownership=";
            message += std::to_string(
                layer.modelownership.at(pixelIndex));
            message.push_back(',');
            message += std::to_string(
                layer.modelvarnishownership.at(pixelIndex));
            message.push_back(',');
            message += std::to_string(
                layer.outervarnishownership.at(pixelIndex));
            message.push_back(',');
            message += std::to_string(
                layer.supportownership.at(pixelIndex));
            if (ownership == SceneRasterOwnership::Model
                && IsEmptySourcePixel(
                    layer,
                    pixelIndex,
                    request.protocol.empty_value))
            {
                message +=
                    "; This pixel is in a pure-white texture region. The "
                    "current Profile cannot express printable white under "
                    "black_is_print; select the full-volume RGB + on-demand "
                    "white carrier Profile or a white-fill Profile.";
            }
            Block(
                result,
                SceneRasterErrorCode::ClosureFailed,
                request,
                "layers.ownership",
                message,
                &instance,
                nullptr,
                expectedLayerIndex);
            return false;
        }
        const std::size_t byteOffset = pixelIndex * kChannelCount;
        statistics.printpixels[0U] +=
            layer.output.channels[byteOffset] != request.protocol.empty_value;
        statistics.printpixels[1U] +=
            layer.output.channels[byteOffset + 1U] != request.protocol.empty_value;
        statistics.printpixels[2U] +=
            layer.output.channels[byteOffset + 2U] != request.protocol.empty_value;
        statistics.printpixels[3U] +=
            layer.output.channels[byteOffset + 3U] != request.protocol.empty_value;
        statistics.printpixels[4U] +=
            layer.output.channels[byteOffset + 4U] != request.protocol.empty_value;
        statistics.printpixels[5U] +=
            layer.output.channels[byteOffset + 5U] != request.protocol.empty_value;
        if (layer.modelownership[pixelIndex] != 0U
            || layer.modelvarnishownership[pixelIndex] != 0U
            || layer.outervarnishownership[pixelIndex] != 0U
            || layer.supportownership[pixelIndex] != 0U)
        {
            const int x = static_cast<int>(
                pixelIndex % static_cast<std::size_t>(
                    instance.localgrid.widthpx));
            const int y = static_cast<int>(
                pixelIndex / static_cast<std::size_t>(
                    instance.localgrid.widthpx));
            statistics.minimumx = std::min(statistics.minimumx, x);
            statistics.minimumy = std::min(statistics.minimumy, y);
            statistics.minimumlayer = std::min(
                statistics.minimumlayer, expectedLayerIndex);
            statistics.maximumx = std::max(statistics.maximumx, x);
            statistics.maximumy = std::max(statistics.maximumy, y);
            statistics.maximumlayer = std::max(
                statistics.maximumlayer, expectedLayerIndex);
        }
    }
    return true;
}

bool ValidateInstance(
    const SceneLayerComposeRequest& request,
    const SceneInstanceRaster& instance,
    const std::unordered_set<std::string>& knownInstanceIds,
    SceneLayerComposeResult& result,
    InstancePlacement& placement,
    SceneInstanceComposeStatistics& instanceStatistics)
{
    if (instance.sceneid != request.sceneid
        || instance.modelid.empty()
        || instance.instanceid.empty()
        || knownInstanceIds.find(instance.instanceid) != knownInstanceIds.end())
    {
        Block(
            result,
            SceneRasterErrorCode::InstanceIdentityInvalid,
            request,
            "instances.identity",
            "visible scene raster instances require unique scene, model, and instance identities",
            &instance);
        return false;
    }
    if (instance.scenerevision != request.expectedscenerevision
        || instance.transformrevision
            != instance.admittedtransformrevision
        || instance.transformhash.empty()
        || instance.transformhash
            != instance.admittedtransformhash)
    {
        Block(
            result,
            SceneRasterErrorCode::RevisionStale,
            request,
            "instances.revision",
            "instance scene or transform revision changed after admission",
            &instance);
        return false;
    }
    if (!instance.admitted)
    {
        Block(
            result,
            SceneRasterErrorCode::AdmissionRequired,
            request,
            "instances.admitted",
            "every visible instance must pass admission before composition",
            &instance);
        return false;
    }
    if (instance.effectivepipelinemode
        != request.effectivepipelinemode)
    {
        Block(
            result,
            SceneRasterErrorCode::PipelineModeMismatch,
            request,
            "instances.effectivepipelinemode",
            "all visible instances must use one effective pipeline mode",
            &instance);
        return false;
    }
    if (!instance.localgrid.IsValid())
    {
        Block(
            result,
            SceneRasterErrorCode::GridInvalid,
            request,
            "instances.localgrid",
            "visible instance local grid must be finite and positive",
            &instance);
        return false;
    }
    if (!SameProtocol(instance.protocol, request.protocol))
    {
        Block(
            result,
            SceneRasterErrorCode::ProtocolMismatch,
            request,
            "instances.protocol",
            "visible instance protocol does not match the fixed scene protocol",
            &instance);
        return false;
    }
    if (!SameSampling(
            instance.localgrid.pitchxmm,
            request.globalgrid.pitchxmm,
            request.quantizationtolerance)
        || !SameSampling(
            instance.localgrid.pitchymm,
            request.globalgrid.pitchymm,
            request.quantizationtolerance)
        || !SameSampling(
            instance.localgrid.layerthicknessmm,
            request.globalgrid.layerthicknessmm,
            request.quantizationtolerance))
    {
        Block(
            result,
            SceneRasterErrorCode::ResolutionMismatch,
            request,
            "instances.localgrid.resolution",
            "all visible instances must share dpiX, dpiY, and layer thickness",
            &instance);
        return false;
    }
    if (!QuantizeOffset(
            instance.localgrid.originxmm,
            request.globalgrid.originxmm,
            request.globalgrid.pitchxmm,
            request.quantizationtolerance,
            placement.offsetx)
        || !QuantizeOffset(
            instance.localgrid.originymm,
            request.globalgrid.originymm,
            request.globalgrid.pitchymm,
            request.quantizationtolerance,
            placement.offsety)
        || !QuantizeOffset(
            instance.localgrid.originzmm,
            request.globalgrid.originzmm,
            request.globalgrid.layerthicknessmm,
            request.quantizationtolerance,
            placement.offsetz))
    {
        Block(
            result,
            SceneRasterErrorCode::OffsetNotIntegral,
            request,
            "instances.localgrid.origin",
            "local raster origin must map to integral scene pixel and layer offsets",
            &instance);
        return false;
    }
    if (placement.offsetx < 0
        || placement.offsety < 0
        || placement.offsetz < 0
        || instance.localgrid.widthpx
            > request.globalgrid.widthpx - placement.offsetx
        || instance.localgrid.heightpx
            > request.globalgrid.heightpx - placement.offsety
        || instance.localgrid.layercount
            > request.globalgrid.layercount - placement.offsetz)
    {
        Block(
            result,
            SceneRasterErrorCode::GridInvalid,
            request,
            "instances.localgrid.bounds",
            "local raster must fit completely inside the admitted scene raster",
            &instance);
        return false;
    }

    std::size_t pixelCount{0U};
    std::size_t byteCount{0U};
    if (!ComputeLayerSizes(instance.localgrid, pixelCount, byteCount)
        || instance.layers.size()
            != static_cast<std::size_t>(instance.localgrid.layercount))
    {
        Block(
            result,
            SceneRasterErrorCode::LayerSequenceMismatch,
            request,
            "instances.layers",
            "visible instance requires exactly one layer for every local layer index",
            &instance);
        return false;
    }
    instanceStatistics.instanceid = instance.instanceid;
    instanceStatistics.raster.grid = instance.localgrid;
    instanceStatistics.raster.minimumx = instance.localgrid.widthpx;
    instanceStatistics.raster.minimumy = instance.localgrid.heightpx;
    instanceStatistics.raster.minimumlayer = instance.localgrid.layercount;
    for (int layerIndex{0};
         layerIndex < instance.localgrid.layercount;
         ++layerIndex)
    {
        if (StopIfCancellationRequested(
                request, result, "composition.instance_layers", layerIndex))
        {
            return false;
        }
        if (!ValidateLayer(
                request,
                instance,
                instance.layers.at(static_cast<std::size_t>(layerIndex)),
                layerIndex,
                pixelCount,
                byteCount,
                result,
                instanceStatistics.raster))
        {
            return false;
        }
    }
    const std::uint64_t sampleCount =
        static_cast<std::uint64_t>(pixelCount)
        * static_cast<std::uint64_t>(instance.layers.size());
    for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
    {
        instanceStatistics.raster.emptypixels[channel] =
            sampleCount - instanceStatistics.raster.printpixels[channel];
    }
    instanceStatistics.raster.available = true;
    placement.instance = &instance;
    return true;
}

void WriteOwnedPixel(
    std::vector<std::uint8_t>& destination,
    const std::size_t destinationPixel,
    const SceneInstanceRasterLayer& source,
    const std::size_t sourcePixel,
    const SceneRasterOwnership ownership,
    const RgbwsvProtocol& protocol)
{
    const std::size_t destinationBase = destinationPixel * kChannelCount;
    const std::size_t sourceBase = sourcePixel * kChannelCount;
    std::fill_n(
        destination.begin() + static_cast<std::ptrdiff_t>(destinationBase),
        kChannelCount,
        protocol.empty_value);
    if (ownership == SceneRasterOwnership::Model)
    {
        for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
        {
            if (channel != kSupportChannel)
            {
                if (channel == kVarnishChannel
                    && source.modelvarnishownership.at(sourcePixel)
                        == 0U)
                {
                    continue;
                }
                destination.at(destinationBase + channel) =
                    source.output.channels.at(sourceBase + channel);
            }
        }
    }
    else if (ownership == SceneRasterOwnership::OuterVarnish)
    {
        destination.at(destinationBase + kVarnishChannel) =
            source.output.channels.at(sourceBase + kVarnishChannel);
    }
    else if (ownership == SceneRasterOwnership::Support)
    {
        destination.at(destinationBase + kSupportChannel) =
            source.output.channels.at(sourceBase + kSupportChannel);
    }
}

bool ResolveCrossInstancePixel(
    const SceneLayerComposeRequest& request,
    SceneLayerComposeResult& result,
    const InstancePlacement& placement,
    const SceneInstanceRasterLayer& sourceLayer,
    const std::size_t sourcePixel,
    const std::size_t destinationPixel,
    const SceneRasterOwnership sourceOwnership,
    std::vector<SceneRasterOwnership>& ownership,
    std::vector<int>& ownerindices,
    std::vector<std::uint8_t>& destination,
    const std::vector<InstancePlacement>& placements,
    const int globalLayerIndex)
{
    const SceneRasterOwnership currentOwnership =
        ownership.at(destinationPixel);
    const int currentOwner = ownerindices.at(destinationPixel);
    if (currentOwnership != SceneRasterOwnership::Empty
        && currentOwner >= 0
        && currentOwner != static_cast<int>(placement.statisticsindex))
    {
        const SceneInstanceRaster* other =
            placements.at(static_cast<std::size_t>(currentOwner)).instance;
        if (currentOwnership == SceneRasterOwnership::Model
            && sourceOwnership == SceneRasterOwnership::Model)
        {
            Block(
                result,
                SceneRasterErrorCode::InstanceOverlap,
                request,
                "layers.modelownership",
                "different instances claim the same model pixel",
                placement.instance,
                other,
                globalLayerIndex);
            return false;
        }
        if (currentOwnership == SceneRasterOwnership::Model
            || sourceOwnership == SceneRasterOwnership::Model)
        {
            Block(
                result,
                SceneRasterErrorCode::MaterialConflict,
                request,
                "layers.materialownership",
                "model ownership conflicts with another instance material",
                placement.instance,
                other,
                globalLayerIndex);
            return false;
        }
    }

    if (static_cast<unsigned int>(sourceOwnership)
        > static_cast<unsigned int>(currentOwnership))
    {
        WriteOwnedPixel(
            destination,
            destinationPixel,
            sourceLayer,
            sourcePixel,
            sourceOwnership,
            request.protocol);
        ownership.at(destinationPixel) = sourceOwnership;
        ownerindices.at(destinationPixel) =
            static_cast<int>(placement.statisticsindex);
    }
    return true;
}

void CountOutputPixel(
    const SceneRasterOwnership ownership,
    SceneLayerComposeStatistics& statistics)
{
    if (ownership == SceneRasterOwnership::Model)
    {
        ++statistics.modelpixels;
    }
    else if (ownership == SceneRasterOwnership::OuterVarnish)
    {
        ++statistics.outervarnishpixels;
    }
    else if (ownership == SceneRasterOwnership::Support)
    {
        ++statistics.supportpixels;
    }
    else
    {
        ++statistics.emptypixels;
    }
}

void CountInstanceOutputPixel(
    const SceneRasterOwnership ownership,
    SceneInstanceComposeStatistics& statistics)
{
    if (ownership == SceneRasterOwnership::Model)
    {
        ++statistics.modelpixels;
    }
    else if (ownership == SceneRasterOwnership::OuterVarnish)
    {
        ++statistics.outervarnishpixels;
    }
    else if (ownership == SceneRasterOwnership::Support)
    {
        ++statistics.supportpixels;
    }
}

}  // namespace

static SceneLayerComposeResult ComposeSceneLayersWithInstances(
    const SceneLayerComposeRequest& request,
    const std::span<const SceneInstanceRaster> instances)
{
    const auto start = std::chrono::steady_clock::now();
    SceneLayerComposeResult result;
    result.sceneid = request.sceneid;
    result.scenerevision = request.currentscenerevision;
    result.grid = request.globalgrid;
    result.protocol = request.protocol;
    result.effectivepipelinemode =
        request.effectivepipelinemode;
    result.statistics.totalinstancecount = instances.size();

    if (StopIfCancellationRequested(request, result, "composition.start"))
    {
        return result;
    }

    std::size_t globalPixelCount{0U};
    std::size_t globalByteCount{0U};
    if (!ValidateRequest(
            request,
            result,
            globalPixelCount,
            globalByteCount))
    {
        result.composems = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        return result;
    }
    std::size_t retainedOutputBytes{0U};
    std::size_t ownershipBytes{0U};
    std::size_t ownershipAndOwnerBytes{0U};
    if (!CheckedMultiply(
            globalByteCount,
            static_cast<std::size_t>(
                request.globalgrid.layercount),
            retainedOutputBytes)
        || !CheckedMultiply(
            globalPixelCount,
            sizeof(SceneRasterOwnership),
            ownershipBytes)
        || !CheckedAdd(
            ownershipBytes,
            globalPixelCount * sizeof(int),
            ownershipAndOwnerBytes)
        || !CheckedAdd(
            retainedOutputBytes,
            ownershipAndOwnerBytes,
            result.peakworkingbytes))
    {
        Block(
            result,
            SceneRasterErrorCode::GridInvalid,
            request,
            "globalgrid.bytes",
            "scene raster retained output size exceeds addressable memory");
        result.composems = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        return result;
    }

    std::vector<InstancePlacement> placements;
    placements.reserve(instances.size());
    std::unordered_set<std::string> knownInstanceIds;
    for (const SceneInstanceRaster& instance : instances)
    {
        if (StopIfCancellationRequested(request, result, "composition.instances"))
        {
            return result;
        }
        if (!instance.visible)
        {
            ++result.statistics.hiddeninstancecount;
            continue;
        }

        InstancePlacement placement;
        placement.statisticsindex = placements.size();
        SceneInstanceComposeStatistics instanceStatistics;
        if (!ValidateInstance(
                request,
                instance,
                knownInstanceIds,
                result,
                placement,
                instanceStatistics))
        {
            result.composems = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();
            return result;
        }
        knownInstanceIds.insert(instance.instanceid);
        result.statistics.instances.push_back(std::move(instanceStatistics));
        placements.push_back(placement);
        ++result.statistics.visibleinstancecount;
    }
    if (placements.empty())
    {
        Block(
            result,
            SceneRasterErrorCode::AdmissionRequired,
            request,
            "instances",
            "scene raster composition requires at least one visible admitted instance");
        result.composems = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        return result;
    }

    result.layers.reserve(
        static_cast<std::size_t>(request.globalgrid.layercount));
    result.layerstatistics.reserve(
        static_cast<std::size_t>(request.globalgrid.layercount));
    std::vector<SceneRasterOwnership> ownership(
        globalPixelCount,
        SceneRasterOwnership::Empty);
    std::vector<int> ownerindices(globalPixelCount, -1);
    for (int globalLayerIndex{0};
         globalLayerIndex < request.globalgrid.layercount;
         ++globalLayerIndex)
    {
        if (StopIfCancellationRequested(
                request, result, "composition.layers", globalLayerIndex))
        {
            return result;
        }
        std::fill(
            ownership.begin(),
            ownership.end(),
            SceneRasterOwnership::Empty);
        std::fill(ownerindices.begin(), ownerindices.end(), -1);

        RgbwsvProductionLayer output;
        output.layerIndex = globalLayerIndex;
        output.zMm = request.globalgrid.originzmm
            + (static_cast<double>(globalLayerIndex) + 0.5)
                * request.globalgrid.layerthicknessmm;
        output.widthPx = request.globalgrid.widthpx;
        output.heightPx = request.globalgrid.heightpx;
        output.channelOrder = request.protocol.channel_order;
        output.channels.assign(globalByteCount, request.protocol.empty_value);
        RgbwsvProductionLayerStatistics layerStatistics;
        layerStatistics.layerIndex = globalLayerIndex;

        for (const InstancePlacement& placement : placements)
        {
            if (StopIfCancellationRequested(
                    request, result, "composition.placements", globalLayerIndex))
            {
                return result;
            }
            const int localLayerIndex =
                globalLayerIndex - placement.offsetz;
            if (localLayerIndex < 0
                || localLayerIndex >= placement.instance->localgrid.layercount)
            {
                continue;
            }
            const SceneInstanceRasterLayer& sourceLayer =
                placement.instance->layers.at(
                    static_cast<std::size_t>(localLayerIndex));
            const int localWidth = placement.instance->localgrid.widthpx;
            const int localHeight = placement.instance->localgrid.heightpx;
            for (int y{0}; y < localHeight; ++y)
            {
                for (int x{0}; x < localWidth; ++x)
                {
                    const std::size_t sourcePixel =
                        static_cast<std::size_t>(y)
                            * static_cast<std::size_t>(localWidth)
                        + static_cast<std::size_t>(x);
                    if ((sourcePixel % kCancellationCheckStride) == 0U
                        && StopIfCancellationRequested(
                            request, result, "composition.pixel_copy", globalLayerIndex))
                    {
                        return result;
                    }
                    const SceneRasterOwnership sourceOwnership =
                        ResolveOwnership(sourceLayer, sourcePixel);
                    if (sourceOwnership == SceneRasterOwnership::Empty)
                    {
                        continue;
                    }
                    const std::size_t destinationPixel =
                        static_cast<std::size_t>(y + placement.offsety)
                            * static_cast<std::size_t>(
                                request.globalgrid.widthpx)
                        + static_cast<std::size_t>(
                            x + placement.offsetx);
                    if (!ResolveCrossInstancePixel(
                            request,
                            result,
                            placement,
                            sourceLayer,
                            sourcePixel,
                            destinationPixel,
                            sourceOwnership,
                            ownership,
                            ownerindices,
                            output.channels,
                            placements,
                            globalLayerIndex))
                    {
                        result.composems =
                            std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - start)
                                .count();
                        return result;
                    }

                }
            }
        }

        for (std::size_t pixelIndex{0U};
             pixelIndex < ownership.size();
             ++pixelIndex)
        {
            if ((pixelIndex % kCancellationCheckStride) == 0U
                && StopIfCancellationRequested(
                    request, result, "composition.closure", globalLayerIndex))
            {
                return result;
            }
            const SceneRasterOwnership pixelOwnership =
                ownership.at(pixelIndex);
            if (!OutputPixelHasClosure(
                    output.channels,
                    pixelIndex,
                    pixelOwnership,
                    request.protocol))
            {
                const int owner = ownerindices.at(pixelIndex);
                const SceneInstanceRaster* instance =
                    owner >= 0
                        ? placements.at(static_cast<std::size_t>(owner)).instance
                        : nullptr;
                Block(
                    result,
                    SceneRasterErrorCode::ClosureFailed,
                    request,
                    "layers.outputclosure",
                    "composed scene pixel does not close against final ownership",
                    instance,
                    nullptr,
                    globalLayerIndex);
                result.composems =
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - start)
                        .count();
                return result;
            }
            CountOutputPixel(pixelOwnership, result.statistics);
            const std::size_t channelBase = pixelIndex * kChannelCount;
            layerStatistics.emptyPixels[0U] +=
                output.channels[channelBase] == request.protocol.empty_value;
            layerStatistics.emptyPixels[1U] +=
                output.channels[channelBase + 1U] == request.protocol.empty_value;
            layerStatistics.emptyPixels[2U] +=
                output.channels[channelBase + 2U] == request.protocol.empty_value;
            layerStatistics.emptyPixels[3U] +=
                output.channels[channelBase + 3U] == request.protocol.empty_value;
            layerStatistics.emptyPixels[4U] +=
                output.channels[channelBase + 4U] == request.protocol.empty_value;
            layerStatistics.emptyPixels[5U] +=
                output.channels[channelBase + 5U] == request.protocol.empty_value;
            const int owner = ownerindices.at(pixelIndex);
            if (owner >= 0)
            {
                CountInstanceOutputPixel(
                    pixelOwnership,
                    result.statistics.instances.at(
                        static_cast<std::size_t>(owner)));
            }
        }
        for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
        {
            layerStatistics.printPixels[channel] =
                static_cast<std::uint64_t>(globalPixelCount)
                - layerStatistics.emptyPixels[channel];
        }
        result.layers.push_back(std::move(output));
        result.layerstatistics.push_back(std::move(layerStatistics));
        if (StopIfCancellationRequested(
                request, result, "composition.layer_complete", globalLayerIndex))
        {
            return result;
        }
    }

    result.available = true;
    result.status = "ready_for_writer";
    result.statistics.outputlayercount = result.layers.size();
    result.composems = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    return result;
}

SceneLayerComposeResult ComposeSceneLayers(
    const SceneLayerComposeRequest& request)
{
    return ComposeSceneLayersWithInstances(request, request.instances);
}

namespace internal
{

SceneLayerComposeResult ComposeSceneLayersBorrowed(
    const SceneLayerComposeRequest& request,
    const std::span<const SceneInstanceRaster> instances)
{
    return ComposeSceneLayersWithInstances(request, instances);
}

}  // namespace internal

ValidatedSceneLayerComposeResult::ValidatedSceneLayerComposeResult(
    SceneLayerComposeResult result) noexcept
    : m_result(std::move(result)),
      m_validated(
          m_result.available
          && m_result.status == "ready_for_writer"
          && !m_result.error.has_value()
          && m_result.grid.IsValid()
          && m_result.layers.size()
              == static_cast<std::size_t>(m_result.grid.layercount)
          && m_result.layerstatistics.size() == m_result.layers.size())
{
}

bool ValidatedSceneLayerComposeResult::IsValid() const noexcept
{
    return m_validated;
}

const SceneLayerComposeResult&
ValidatedSceneLayerComposeResult::Value() const noexcept
{
    return m_result;
}

SceneLayerComposeResult
ValidatedSceneLayerComposeResult::Release() && noexcept
{
    m_validated = false;
    return std::move(m_result);
}

ValidatedSceneLayerComposeResult ComposeValidatedSceneLayers(
    const SceneLayerComposeRequest& request)
{
    return ValidatedSceneLayerComposeResult{ComposeSceneLayers(request)};
}

}  // namespace slicer_core
