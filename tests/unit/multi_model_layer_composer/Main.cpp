#include "slicer_core/pipeline/SceneLayerComposer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr std::size_t kChannelCount{6U};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::size_t PixelIndex(const int width, const int x, const int y)
{
    return static_cast<std::size_t>(y)
            * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x);
}

std::size_t ChannelIndex(
    const int width,
    const int x,
    const int y,
    const std::size_t channel)
{
    return PixelIndex(width, x, y) * kChannelCount + channel;
}

slicer_core::SceneRasterGrid MakeGrid(
    const int width,
    const int height,
    const int layers,
    const double originX = 0.0,
    const double originY = 0.0,
    const double originZ = 0.0)
{
    slicer_core::SceneRasterGrid grid;
    grid.widthpx = width;
    grid.heightpx = height;
    grid.layercount = layers;
    grid.originxmm = originX;
    grid.originymm = originY;
    grid.originzmm = originZ;
    grid.pitchxmm = 0.1;
    grid.pitchymm = 0.2;
    grid.layerthicknessmm = 0.05;
    return grid;
}

slicer_core::SceneInstanceRaster MakeInstance(
    const std::string& id,
    const slicer_core::SceneRasterGrid& grid)
{
    slicer_core::SceneInstanceRaster instance;
    instance.sceneid = "scene";
    instance.modelid = "model-" + id;
    instance.instanceid = id;
    instance.scenerevision = 7U;
    instance.transformrevision = 11U;
    instance.admittedtransformrevision = 11U;
    instance.transformhash = "transform-" + id;
    instance.admittedtransformhash = instance.transformhash;
    instance.visible = true;
    instance.admitted = true;
    instance.localgrid = grid;
    instance.protocol = slicer_core::FixedSceneRasterProtocol();

    const std::size_t pixelCount =
        static_cast<std::size_t>(grid.widthpx)
        * static_cast<std::size_t>(grid.heightpx);
    for (int layerIndex{0}; layerIndex < grid.layercount; ++layerIndex)
    {
        slicer_core::SceneInstanceRasterLayer layer;
        layer.layerindex = layerIndex;
        layer.zmm = grid.originzmm
            + (static_cast<double>(layerIndex) + 0.5)
                * grid.layerthicknessmm;
        layer.output.layerIndex = layerIndex;
        layer.output.zMm = layer.zmm;
        layer.output.widthPx = grid.widthpx;
        layer.output.heightPx = grid.heightpx;
        layer.output.channelOrder = instance.protocol.channel_order;
        layer.output.channels.assign(
            pixelCount * kChannelCount,
            instance.protocol.empty_value);
        layer.modelownership.assign(pixelCount, 0U);
        layer.modelvarnishownership.assign(pixelCount, 0U);
        layer.outervarnishownership.assign(pixelCount, 0U);
        layer.supportownership.assign(pixelCount, 0U);
        instance.layers.push_back(std::move(layer));
    }
    return instance;
}

void AddVarnishModel(
    slicer_core::SceneInstanceRaster& instance,
    const int layerIndex,
    const int x,
    const int y)
{
    slicer_core::SceneInstanceRasterLayer& layer =
        instance.layers.at(static_cast<std::size_t>(layerIndex));
    const std::size_t pixel =
        PixelIndex(instance.localgrid.widthpx, x, y);
    layer.modelownership.at(pixel) = 1U;
    layer.modelvarnishownership.at(pixel) = 1U;
    layer.output.channels.at(
        ChannelIndex(
            instance.localgrid.widthpx,
            x,
            y,
            5U)) = instance.protocol.print_value;
}

void AddModel(
    slicer_core::SceneInstanceRaster& instance,
    const int layerIndex,
    const int x,
    const int y,
    const std::array<std::uint8_t, 4>& rgbw)
{
    slicer_core::SceneInstanceRasterLayer& layer =
        instance.layers.at(static_cast<std::size_t>(layerIndex));
    const std::size_t pixel =
        PixelIndex(instance.localgrid.widthpx, x, y);
    layer.modelownership.at(pixel) = 1U;
    for (std::size_t channel{0U}; channel < rgbw.size(); ++channel)
    {
        layer.output.channels.at(
            ChannelIndex(
                instance.localgrid.widthpx,
                x,
                y,
                channel)) = rgbw.at(channel);
    }
}

void AddSupport(
    slicer_core::SceneInstanceRaster& instance,
    const int layerIndex,
    const int x,
    const int y)
{
    slicer_core::SceneInstanceRasterLayer& layer =
        instance.layers.at(static_cast<std::size_t>(layerIndex));
    const std::size_t pixel =
        PixelIndex(instance.localgrid.widthpx, x, y);
    layer.supportownership.at(pixel) = 1U;
    layer.output.channels.at(
        ChannelIndex(
            instance.localgrid.widthpx,
            x,
            y,
            4U)) = instance.protocol.print_value;
}

void AddOuterVarnish(
    slicer_core::SceneInstanceRaster& instance,
    const int layerIndex,
    const int x,
    const int y)
{
    slicer_core::SceneInstanceRasterLayer& layer =
        instance.layers.at(static_cast<std::size_t>(layerIndex));
    const std::size_t pixel =
        PixelIndex(instance.localgrid.widthpx, x, y);
    layer.outervarnishownership.at(pixel) = 1U;
    layer.output.channels.at(
        ChannelIndex(
            instance.localgrid.widthpx,
            x,
            y,
            5U)) = instance.protocol.print_value;
}

slicer_core::SceneLayerComposeRequest MakeRequest()
{
    slicer_core::SceneLayerComposeRequest request;
    request.sceneid = "scene";
    request.currentscenerevision = 7U;
    request.expectedscenerevision = 7U;
    request.admissionpassed = true;
    request.globalgrid = MakeGrid(8, 5, 4);
    request.protocol = slicer_core::FixedSceneRasterProtocol();
    return request;
}

std::uint8_t OutputChannel(
    const slicer_core::SceneLayerComposeResult& result,
    const int layer,
    const int x,
    const int y,
    const std::size_t channel)
{
    return result.layers.at(static_cast<std::size_t>(layer))
        .channels.at(
            ChannelIndex(result.grid.widthpx, x, y, channel));
}

bool HasError(
    const slicer_core::SceneLayerComposeResult& result,
    const slicer_core::SceneRasterErrorCode code)
{
    return !result.available
        && result.layers.empty()
        && result.error.has_value()
        && result.error->code == code
        && slicer_core::SceneRasterErrorCodeName(code)
            != "SCENE_RASTER_UNKNOWN";
}

bool SingleInstancePreservesWriterReadyBytes()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid = MakeGrid(3, 1, 1);
    slicer_core::SceneInstanceRaster instance =
        MakeInstance("single", request.globalgrid);
    AddModel(instance, 0, 0, 0, {10U, 20U, 30U, 0U});
    AddSupport(instance, 0, 1, 0);
    request.instances.push_back(instance);

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);
    return ExpectTrue(result.IsValid(), "single instance result is valid")
        && ExpectTrue(result.layers.size() == 1U, "single output layer")
        && ExpectTrue(
            result.layers.at(0).channels
                == instance.layers.at(0).output.channels,
            "single instance bytes are unchanged")
        && ExpectTrue(
            result.protocol.schema == "p0.rgbwsv.2",
            "schema remains p0.rgbwsv.2")
        && ExpectTrue(
            result.protocol.bit_depth == 8,
            "bit depth remains uint8")
        && ExpectTrue(
            result.protocol.polarity == "black_is_print",
            "polarity remains black_is_print");
}

bool SeparatedOffsetsDifferentSizesAndHeightsCompose()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    slicer_core::SceneInstanceRaster first =
        MakeInstance("first", MakeGrid(2, 2, 1, 0.0, 0.0, 0.0));
    slicer_core::SceneInstanceRaster second =
        MakeInstance("second", MakeGrid(3, 1, 2, 0.4, 0.4, 0.05));
    AddModel(first, 0, 1, 1, {15U, 25U, 35U, 255U});
    AddModel(second, 0, 0, 0, {45U, 55U, 65U, 255U});
    AddSupport(second, 1, 2, 0);
    request.instances = {first, second};

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);
    const std::size_t globalPixels =
        static_cast<std::size_t>(result.grid.widthpx)
        * static_cast<std::size_t>(result.grid.heightpx);
    const std::size_t retainedOutputBytes =
        globalPixels * kChannelCount * result.layers.size();
    const std::size_t ownershipBytes =
        globalPixels
            * (sizeof(slicer_core::SceneRasterOwnership) + sizeof(int));
    return ExpectTrue(result.IsValid(), "separated instances compose")
        && ExpectTrue(
            OutputChannel(result, 0, 1, 1, 0U) == 15U,
            "first XY offset")
        && ExpectTrue(
            OutputChannel(result, 1, 4, 2, 1U) == 55U,
            "second XY and Z offset")
        && ExpectTrue(
            OutputChannel(result, 2, 6, 2, 4U) == 0U,
            "different-height upper support layer")
        && ExpectTrue(
            result.statistics.emptypixels > 0U,
            "net spacing remains empty")
        && ExpectTrue(
            result.peakworkingbytes
                >= retainedOutputBytes + ownershipBytes,
            "peak bytes include retained scene output and working ownership");
}

bool HiddenInstanceIsSkippedWithoutValidation()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid = MakeGrid(2, 1, 1);
    slicer_core::SceneInstanceRaster visible =
        MakeInstance("visible", request.globalgrid);
    AddModel(visible, 0, 0, 0, {1U, 2U, 3U, 255U});

    slicer_core::SceneInstanceRaster hidden;
    hidden.visible = false;
    hidden.instanceid = "hidden";
    hidden.layers.resize(3U);
    request.instances = {visible, hidden};

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);
    return ExpectTrue(result.IsValid(), "hidden invalid raster is skipped")
        && ExpectTrue(
            result.statistics.visibleinstancecount == 1U,
            "one visible instance")
        && ExpectTrue(
            result.statistics.hiddeninstancecount == 1U,
            "one hidden instance");
}

bool EmptyAndChannelIsolationArePreserved()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid = MakeGrid(5, 1, 1);
    slicer_core::SceneInstanceRaster model =
        MakeInstance("model", MakeGrid(1, 1, 1, 0.0, 0.0, 0.0));
    slicer_core::SceneInstanceRaster white =
        MakeInstance("white", MakeGrid(1, 1, 1, 0.2, 0.0, 0.0));
    slicer_core::SceneInstanceRaster support =
        MakeInstance("support", MakeGrid(1, 1, 1, 0.3, 0.0, 0.0));
    slicer_core::SceneInstanceRaster varnish =
        MakeInstance("varnish", MakeGrid(1, 1, 1, 0.4, 0.0, 0.0));
    AddModel(model, 0, 0, 0, {7U, 8U, 9U, 255U});
    AddModel(white, 0, 0, 0, {255U, 255U, 255U, 0U});
    AddSupport(support, 0, 0, 0);
    AddOuterVarnish(varnish, 0, 0, 0);
    request.instances = {model, white, support, varnish};

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);
    bool emptyGap{true};
    for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
    {
        emptyGap = emptyGap
            && OutputChannel(result, 0, 1, 0, channel) == 255U;
    }
    return ExpectTrue(result.IsValid(), "isolated channels compose")
        && ExpectTrue(emptyGap, "gap is six-channel 255")
        && ExpectTrue(
            OutputChannel(result, 0, 0, 0, 0U) == 7U
                && OutputChannel(result, 0, 0, 0, 3U) == 255U
                && OutputChannel(result, 0, 0, 0, 4U) == 255U,
            "RGB model does not leak W or S")
        && ExpectTrue(
            OutputChannel(result, 0, 2, 0, 3U) == 0U
                && OutputChannel(result, 0, 2, 0, 0U) == 255U,
            "white model only writes W")
        && ExpectTrue(
            OutputChannel(result, 0, 3, 0, 4U) == 0U
                && OutputChannel(result, 0, 3, 0, 5U) == 255U,
            "support only writes S")
        && ExpectTrue(
            OutputChannel(result, 0, 4, 0, 5U) == 0U
                && OutputChannel(result, 0, 4, 0, 4U) == 255U,
            "outer varnish only writes V");
}

bool LocalPriorityIsModelThenVarnishThenSupport()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid = MakeGrid(3, 1, 1);
    slicer_core::SceneInstanceRaster instance =
        MakeInstance("priority", request.globalgrid);

    AddSupport(instance, 0, 0, 0);
    AddOuterVarnish(instance, 0, 0, 0);
    AddModel(instance, 0, 0, 0, {4U, 5U, 6U, 255U});
    AddSupport(instance, 0, 1, 0);
    AddOuterVarnish(instance, 0, 1, 0);
    AddVarnishModel(instance, 0, 2, 0);
    request.instances.push_back(instance);

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);
    return ExpectTrue(result.IsValid(), "priority fixture composes")
        && ExpectTrue(
            OutputChannel(result, 0, 0, 0, 0U) == 4U
                && OutputChannel(result, 0, 0, 0, 4U) == 255U
                && OutputChannel(result, 0, 0, 0, 5U) == 255U,
            "RGB model wins over support and outer varnish")
        && ExpectTrue(
            OutputChannel(result, 0, 1, 0, 5U) == 0U
                && OutputChannel(result, 0, 1, 0, 4U) == 255U,
            "outer varnish wins over support")
        && ExpectTrue(
            OutputChannel(result, 0, 2, 0, 5U) == 0U,
            "V-only model material remains a valid model pixel");
}

bool CrossInstancePriorityStatisticsCountOnlyWinner()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid = MakeGrid(1, 1, 1);
    slicer_core::SceneInstanceRaster support =
        MakeInstance("support", request.globalgrid);
    slicer_core::SceneInstanceRaster varnish =
        MakeInstance("varnish", request.globalgrid);
    AddSupport(support, 0, 0, 0);
    AddOuterVarnish(varnish, 0, 0, 0);
    request.instances = {support, varnish};

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);
    return ExpectTrue(result.IsValid(), "cross-instance priority composes")
        && ExpectTrue(
            result.statistics.instances.at(0).supportpixels == 0U,
            "overwritten support is not counted as a contribution")
        && ExpectTrue(
            result.statistics.instances.at(1).outervarnishpixels == 1U,
            "winning varnish is counted once")
        && ExpectTrue(
            result.statistics.outervarnishpixels == 1U
                && result.statistics.supportpixels == 0U,
            "per-instance and scene statistics agree");
}

bool BoundaryContactDoesNotOverlap()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid = MakeGrid(4, 1, 1);
    slicer_core::SceneInstanceRaster first =
        MakeInstance("left", MakeGrid(2, 1, 1));
    slicer_core::SceneInstanceRaster second =
        MakeInstance("right", MakeGrid(2, 1, 1, 0.2, 0.0, 0.0));
    AddModel(first, 0, 1, 0, {10U, 20U, 30U, 255U});
    AddModel(second, 0, 0, 0, {40U, 50U, 60U, 255U});
    request.instances = {first, second};

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);
    return ExpectTrue(result.IsValid(), "pixel-boundary contact is valid")
        && ExpectTrue(
            OutputChannel(result, 0, 1, 0, 0U) == 10U,
            "left boundary pixel retained")
        && ExpectTrue(
            OutputChannel(result, 0, 2, 0, 0U) == 40U,
            "right boundary pixel retained");
}

bool ModelOverlapFailsClosed()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid = MakeGrid(1, 1, 1);
    slicer_core::SceneInstanceRaster first =
        MakeInstance("first", request.globalgrid);
    slicer_core::SceneInstanceRaster second =
        MakeInstance("second", request.globalgrid);
    AddModel(first, 0, 0, 0, {1U, 2U, 3U, 255U});
    AddModel(second, 0, 0, 0, {4U, 5U, 6U, 255U});
    request.instances = {first, second};

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);
    return ExpectTrue(
               HasError(
                   result,
                   slicer_core::SceneRasterErrorCode::InstanceOverlap),
               "model overlap fails closed")
        && ExpectTrue(
            result.error->instanceid == "second"
                && result.error->otherinstanceid == "first"
                && result.error->layerindex == 0,
            "overlap identifies both instances and layer");
}

bool CrossInstanceMaterialConflictFailsClosed()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid = MakeGrid(1, 1, 1);
    slicer_core::SceneInstanceRaster support =
        MakeInstance("support", request.globalgrid);
    slicer_core::SceneInstanceRaster model =
        MakeInstance("model", request.globalgrid);
    AddSupport(support, 0, 0, 0);
    AddModel(model, 0, 0, 0, {4U, 5U, 6U, 255U});
    request.instances = {support, model};

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);
    return ExpectTrue(
               HasError(
                   result,
                   slicer_core::SceneRasterErrorCode::MaterialConflict),
               "cross-instance model/support conflict fails")
        && ExpectTrue(
            result.error->instanceid == "model"
                && result.error->otherinstanceid == "support",
            "material conflict has both instance IDs");
}

bool AdmissionAndIdentityFailuresAreStable()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.admissionpassed = false;
    const auto admission = slicer_core::ComposeSceneLayers(request);

    request.admissionpassed = true;
    request.globalgrid = MakeGrid(1, 1, 1);
    slicer_core::SceneInstanceRaster instance =
        MakeInstance("", request.globalgrid);
    request.instances = {instance};
    const auto identity = slicer_core::ComposeSceneLayers(request);

    request.instances.clear();
    const auto empty = slicer_core::ComposeSceneLayers(request);

    return ExpectTrue(
               HasError(
                   admission,
                   slicer_core::SceneRasterErrorCode::AdmissionRequired),
               "scene admission required")
        && ExpectTrue(
            HasError(
                identity,
                slicer_core::SceneRasterErrorCode::InstanceIdentityInvalid),
            "instance identity required")
        && ExpectTrue(
            HasError(
                empty,
                slicer_core::SceneRasterErrorCode::AdmissionRequired),
            "at least one visible admitted instance required");
}

bool ProtocolMutationsFailClosed()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.protocol.schema = "p0.rgbwsv.1";
    const auto schema = slicer_core::ComposeSceneLayers(request);

    request = MakeRequest();
    request.protocol.bit_depth = 16;
    const auto bitDepth = slicer_core::ComposeSceneLayers(request);

    request = MakeRequest();
    request.protocol.polarity = "white_is_print";
    const auto polarity = slicer_core::ComposeSceneLayers(request);

    request = MakeRequest();
    std::swap(
        request.protocol.channel_order.at(3),
        request.protocol.channel_order.at(4));
    const auto order = slicer_core::ComposeSceneLayers(request);

    return ExpectTrue(
               HasError(
                   schema,
                   slicer_core::SceneRasterErrorCode::ProtocolMismatch),
               "schema mismatch rejected")
        && ExpectTrue(
            HasError(
                bitDepth,
                slicer_core::SceneRasterErrorCode::ProtocolMismatch),
            "bit depth mismatch rejected")
        && ExpectTrue(
            HasError(
                polarity,
                slicer_core::SceneRasterErrorCode::ProtocolMismatch),
            "polarity mismatch rejected")
        && ExpectTrue(
            HasError(
                order,
                slicer_core::SceneRasterErrorCode::ProtocolMismatch),
            "channel order mismatch rejected");
}

bool ResolutionAndNonIntegralOffsetsFailClosed()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid = MakeGrid(3, 2, 2);
    slicer_core::SceneInstanceRaster resolution =
        MakeInstance("resolution", MakeGrid(1, 1, 1));
    resolution.localgrid.pitchxmm = 0.11;
    request.instances = {resolution};
    const auto resolutionResult =
        slicer_core::ComposeSceneLayers(request);

    slicer_core::SceneInstanceRaster xy =
        MakeInstance(
            "xy",
            MakeGrid(1, 1, 1, 0.15, 0.0, 0.0));
    request.instances = {xy};
    const auto xyResult = slicer_core::ComposeSceneLayers(request);

    slicer_core::SceneInstanceRaster z =
        MakeInstance(
            "z",
            MakeGrid(1, 1, 1, 0.0, 0.0, 0.025));
    request.instances = {z};
    const auto zResult = slicer_core::ComposeSceneLayers(request);

    return ExpectTrue(
               HasError(
                   resolutionResult,
                   slicer_core::SceneRasterErrorCode::ResolutionMismatch),
               "resolution mismatch rejected")
        && ExpectTrue(
            HasError(
                xyResult,
                slicer_core::SceneRasterErrorCode::OffsetNotIntegral),
            "non-integral XY offset rejected")
        && ExpectTrue(
            HasError(
                zResult,
                slicer_core::SceneRasterErrorCode::OffsetNotIntegral),
            "non-integral Z offset rejected");
}

bool LayerSequenceAndByteFailuresAreStable()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid = MakeGrid(2, 1, 2);
    slicer_core::SceneInstanceRaster missing =
        MakeInstance("missing", request.globalgrid);
    missing.layers.pop_back();
    request.instances = {missing};
    const auto missingResult = slicer_core::ComposeSceneLayers(request);

    slicer_core::SceneInstanceRaster duplicate =
        MakeInstance("duplicate", request.globalgrid);
    duplicate.layers.at(1).layerindex = 0;
    duplicate.layers.at(1).output.layerIndex = 0;
    request.instances = {duplicate};
    const auto duplicateResult =
        slicer_core::ComposeSceneLayers(request);

    slicer_core::SceneInstanceRaster bytes =
        MakeInstance("bytes", request.globalgrid);
    bytes.layers.at(0).output.channels.pop_back();
    request.instances = {bytes};
    const auto bytesResult = slicer_core::ComposeSceneLayers(request);

    return ExpectTrue(
               HasError(
                   missingResult,
                   slicer_core::SceneRasterErrorCode::LayerSequenceMismatch),
               "missing layer rejected")
        && ExpectTrue(
            HasError(
                duplicateResult,
                slicer_core::SceneRasterErrorCode::LayerSequenceMismatch),
            "duplicate layer index rejected")
        && ExpectTrue(
            HasError(
                bytesResult,
                slicer_core::SceneRasterErrorCode::LayerSizeInvalid),
            "wrong byte count rejected");
}

bool RevisionStaleFailuresAreStable()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.currentscenerevision = 8U;
    const auto scene = slicer_core::ComposeSceneLayers(request);

    request = MakeRequest();
    request.globalgrid = MakeGrid(1, 1, 1);
    slicer_core::SceneInstanceRaster instance =
        MakeInstance("stale", request.globalgrid);
    instance.admittedtransformrevision = 10U;
    request.instances = {instance};
    const auto transform = slicer_core::ComposeSceneLayers(request);

    return ExpectTrue(
               HasError(
                   scene,
                   slicer_core::SceneRasterErrorCode::RevisionStale),
               "stale scene revision rejected")
        && ExpectTrue(
            HasError(
                transform,
                slicer_core::SceneRasterErrorCode::RevisionStale),
            "stale transform revision rejected");
}

bool ClosureFailureIsAtomicAndInputIsImmutable()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid = MakeGrid(2, 1, 2);
    slicer_core::SceneInstanceRaster instance =
        MakeInstance("closure", request.globalgrid);
    AddModel(instance, 0, 0, 0, {1U, 2U, 3U, 255U});
    instance.layers.at(1).output.channels.at(0) = 0U;
    request.instances = {instance};
    const std::vector<std::uint8_t> original =
        request.instances.at(0).layers.at(1).output.channels;

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);
    return ExpectTrue(
               HasError(
                   result,
                   slicer_core::SceneRasterErrorCode::ClosureFailed),
               "closure mismatch fails")
        && ExpectTrue(
            result.layers.empty()
                && result.statistics.outputlayercount == 0U,
            "failure returns no partial writer-ready layers")
        && ExpectTrue(
            request.instances.at(0).layers.at(1).output.channels
                == original,
            "failure does not mutate source bytes");
}

bool OpaqueWhiteRgbOnlyModelPixelHasActionableFailure()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid = MakeGrid(1, 1, 1);
    slicer_core::SceneInstanceRaster instance =
        MakeInstance("opaque-white-rgb-only", request.globalgrid);
    AddModel(instance, 0, 0, 0, {255U, 255U, 255U, 255U});
    request.instances = {instance};

    const slicer_core::SceneLayerComposeResult result =
        slicer_core::ComposeSceneLayers(request);
    return ExpectTrue(
               HasError(
                   result,
                   slicer_core::SceneRasterErrorCode::ClosureFailed),
               "opaque white RGB-only model pixel fails closed")
        && ExpectTrue(
            result.error->message.find("RGB-only")
                != std::string::npos,
            "failure identifies the RGB-only protocol limitation")
        && ExpectTrue(
            result.error->message.find("white or varnish model fill")
                != std::string::npos,
            "failure recommends a printable model-fill profile");
}

bool DeterministicCompositionProducesIdenticalBytes()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    slicer_core::SceneInstanceRaster first =
        MakeInstance("first", MakeGrid(2, 1, 2));
    slicer_core::SceneInstanceRaster second =
        MakeInstance("second", MakeGrid(2, 1, 1, 0.4, 0.2, 0.05));
    AddModel(first, 0, 0, 0, {9U, 8U, 7U, 255U});
    AddSupport(first, 1, 1, 0);
    AddOuterVarnish(second, 0, 1, 0);
    request.instances = {first, second};

    const slicer_core::SceneLayerComposeResult firstResult =
        slicer_core::ComposeSceneLayers(request);
    const slicer_core::SceneLayerComposeResult secondResult =
        slicer_core::ComposeSceneLayers(request);
    if (!ExpectTrue(
            firstResult.IsValid() && secondResult.IsValid(),
            "determinism fixtures compose")
        || !ExpectTrue(
            firstResult.layers.size() == secondResult.layers.size(),
            "deterministic layer count"))
    {
        return false;
    }
    for (std::size_t index{0U}; index < firstResult.layers.size(); ++index)
    {
        if (!ExpectTrue(
                firstResult.layers.at(index).layerIndex
                    == secondResult.layers.at(index).layerIndex,
                "deterministic layer index")
            || !ExpectTrue(
                firstResult.layers.at(index).zMm
                    == secondResult.layers.at(index).zMm,
                "deterministic layer Z")
            || !ExpectTrue(
                firstResult.layers.at(index).channels
                    == secondResult.layers.at(index).channels,
                "deterministic RGBWSV bytes"))
        {
            return false;
        }
    }
    return true;
}

bool GridAndInstanceProtocolFailuresAreStable()
{
    slicer_core::SceneLayerComposeRequest request = MakeRequest();
    request.globalgrid.widthpx = 0;
    const auto grid = slicer_core::ComposeSceneLayers(request);

    request = MakeRequest();
    request.globalgrid = MakeGrid(1, 1, 1);
    slicer_core::SceneInstanceRaster instance =
        MakeInstance("protocol", request.globalgrid);
    instance.protocol.empty_value = 0U;
    request.instances = {instance};
    const auto protocol = slicer_core::ComposeSceneLayers(request);

    instance = MakeInstance("layer-order", request.globalgrid);
    std::swap(
        instance.layers.at(0).output.channelOrder.at(3),
        instance.layers.at(0).output.channelOrder.at(4));
    request.instances = {instance};
    const auto layerOrder = slicer_core::ComposeSceneLayers(request);

    return ExpectTrue(
               HasError(
                   grid,
                   slicer_core::SceneRasterErrorCode::GridInvalid),
               "invalid global grid rejected")
        && ExpectTrue(
            HasError(
                protocol,
                slicer_core::SceneRasterErrorCode::ProtocolMismatch),
            "instance protocol mismatch rejected")
        && ExpectTrue(
            HasError(
                layerOrder,
                slicer_core::SceneRasterErrorCode::ProtocolMismatch),
            "layer channel order mismatch rejected");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"single_instance_preserves_writer_ready_bytes", SingleInstancePreservesWriterReadyBytes},
        {"separated_offsets_different_sizes_and_heights_compose", SeparatedOffsetsDifferentSizesAndHeightsCompose},
        {"hidden_instance_is_skipped_without_validation", HiddenInstanceIsSkippedWithoutValidation},
        {"empty_and_channel_isolation_are_preserved", EmptyAndChannelIsolationArePreserved},
        {"local_priority_is_model_then_varnish_then_support", LocalPriorityIsModelThenVarnishThenSupport},
        {"cross_instance_priority_statistics_count_only_winner", CrossInstancePriorityStatisticsCountOnlyWinner},
        {"boundary_contact_does_not_overlap", BoundaryContactDoesNotOverlap},
        {"model_overlap_fails_closed", ModelOverlapFailsClosed},
        {"cross_instance_material_conflict_fails_closed", CrossInstanceMaterialConflictFailsClosed},
        {"admission_and_identity_failures_are_stable", AdmissionAndIdentityFailuresAreStable},
        {"protocol_mutations_fail_closed", ProtocolMutationsFailClosed},
        {"resolution_and_non_integral_offsets_fail_closed", ResolutionAndNonIntegralOffsetsFailClosed},
        {"layer_sequence_and_byte_failures_are_stable", LayerSequenceAndByteFailuresAreStable},
        {"revision_stale_failures_are_stable", RevisionStaleFailuresAreStable},
        {"closure_failure_is_atomic_and_input_is_immutable", ClosureFailureIsAtomicAndInputIsImmutable},
        {"opaque_white_rgb_only_model_pixel_has_actionable_failure", OpaqueWhiteRgbOnlyModelPixelHasActionableFailure},
        {"deterministic_composition_produces_identical_bytes", DeterministicCompositionProducesIdenticalBytes},
        {"grid_and_instance_protocol_failures_are_stable", GridAndInstanceProtocolFailuresAreStable},
    };

    bool passed{true};
    for (const auto& test : tests)
    {
        std::cout << "RUN: " << test.first << '\n' << std::flush;
        const bool current = test.second();
        std::cout << (current ? "PASS: " : "FAIL: ")
                  << test.first << '\n';
        passed = current && passed;
    }
    return passed ? 0 : 1;
}
