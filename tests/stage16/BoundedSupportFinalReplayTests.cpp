#include "slicer_core/support/BoundedSupportFinalReplay.h"
#include "slicer_core/support/SupportBaseProjection.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

using namespace slicer_core;

static_assert(!std::is_copy_constructible_v<BoundedSupportFinalReplayScanner>);
static_assert(!std::is_copy_constructible_v<BoundedSupportFinalReplayResult>);

constexpr int kWidth{4};
constexpr int kHeight{3};
constexpr int kLayerCount{3};
constexpr std::size_t kPixelCount{12U};
constexpr std::size_t kFirstFootprintPixel{0U};
constexpr std::size_t kSecondFootprintPixel{11U};

bool Expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

template <typename Exception>
bool ExpectThrows(
    const std::function<void()>& operation,
    const std::string& message)
{
    try
    {
        operation();
    }
    catch (const Exception&)
    {
        return true;
    }
    catch (...)
    {
    }
    std::cerr << "FAIL " << message << '\n';
    return false;
}

BoundedSupportShapeScanRequest MakeShapeRequest()
{
    BoundedSupportShapeScanRequest request;
    request.widthPx = kWidth;
    request.heightPx = kHeight;
    request.layerCount = kLayerCount;
    request.connectivity = 4;
    request.internalVoid.enabled = false;
    request.shape.enabled = false;
    return request;
}

BoundedSupportDemandPlan BuildUpperProjectionPlan()
{
    std::vector<int> lower(kPixelCount, -1);
    std::vector<int> modelLast(kPixelCount, -1);
    std::vector<int> upper(kPixelCount, -1);
    std::vector<int> unsupported(kPixelCount, 0);
    upper[kFirstFootprintPixel] = 1;
    upper[kSecondFootprintPixel] = 1;

    BoundedSupportDemandRequest request;
    request.layerCount = kLayerCount;
    request.lowerSourceLayers = lower;
    request.modelLastLayers = modelLast;
    request.upperBoundaryLastLayers = upper;
    request.unsupportedTopExclusiveLayers = unsupported;
    request.upperEnabled = true;
    return BuildBoundedSupportDemandPlan(request);
}

BoundedSupportDemandPlan BuildDifferentUpperProjectionPlan()
{
    std::vector<int> lower(kPixelCount, -1);
    std::vector<int> modelLast(kPixelCount, -1);
    std::vector<int> upper(kPixelCount, -1);
    std::vector<int> unsupported(kPixelCount, 0);
    upper[1U] = 1;
    upper[kSecondFootprintPixel] = 1;

    BoundedSupportDemandRequest request;
    request.layerCount = kLayerCount;
    request.lowerSourceLayers = lower;
    request.modelLastLayers = modelLast;
    request.upperBoundaryLastLayers = upper;
    request.unsupportedTopExclusiveLayers = unsupported;
    request.upperEnabled = true;
    return BuildBoundedSupportDemandPlan(request);
}

BoundedSupportDemandPlan BuildDiagonalUpperProjectionPlan()
{
    std::vector<int> lower(kPixelCount, -1);
    std::vector<int> modelLast(kPixelCount, -1);
    std::vector<int> upper(kPixelCount, -1);
    std::vector<int> unsupported(kPixelCount, 0);
    upper[0U] = 1;
    upper[5U] = 1;

    BoundedSupportDemandRequest request;
    request.layerCount = kLayerCount;
    request.lowerSourceLayers = lower;
    request.modelLastLayers = modelLast;
    request.upperBoundaryLastLayers = upper;
    request.unsupportedTopExclusiveLayers = unsupported;
    request.upperEnabled = true;
    return BuildBoundedSupportDemandPlan(request);
}

std::vector<std::vector<std::uint8_t>> EmptyMasks()
{
    return std::vector<std::vector<std::uint8_t>>(
        kLayerCount,
        std::vector<std::uint8_t>(kPixelCount, 0U));
}

std::vector<std::uint8_t> FlattenMasks(
    const std::vector<std::vector<std::uint8_t>>& masks)
{
    std::vector<std::uint8_t> volume;
    volume.reserve(masks.size() * kPixelCount);
    for (const auto& mask : masks)
    {
        volume.insert(volume.end(), mask.begin(), mask.end());
    }
    return volume;
}

std::vector<std::uint8_t> ReplayShapeSupportVolume(
    const BoundedSupportDemandPlan& plan,
    const BoundedSupportShapeScanRequest& request,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    const std::vector<std::vector<std::uint8_t>>& upperMasks)
{
    BoundedSupportShapeScanner scanner{request, plan};
    std::vector<std::uint8_t> volume;
    volume.reserve(static_cast<std::size_t>(kLayerCount) * kPixelCount);
    std::vector<std::uint8_t> support(kPixelCount, 0U);
    std::vector<SupportType> types(kPixelCount, SupportType::None);
    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        scanner.ConsumeLayer(
            layerIndex,
            modelMasks[static_cast<std::size_t>(layerIndex)],
            upperMasks[static_cast<std::size_t>(layerIndex)],
            support,
            types);
        volume.insert(volume.end(), support.begin(), support.end());
    }
    static_cast<void>(std::move(scanner).Finish());
    return volume;
}

BoundedSupportShapeScanResult BuildVerifiedShapeEvidence(
    const BoundedSupportDemandPlan& plan,
    const BoundedSupportShapeScanRequest& request,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    const std::vector<std::vector<std::uint8_t>>& upperMasks)
{
    BoundedSupportShapeScanner scanner{request, plan};
    std::vector<std::uint8_t> support(kPixelCount, 0U);
    std::vector<SupportType> types(kPixelCount, SupportType::None);
    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        scanner.ConsumeLayer(
            layerIndex,
            modelMasks[static_cast<std::size_t>(layerIndex)],
            upperMasks[static_cast<std::size_t>(layerIndex)],
            support,
            types);
    }
    return std::move(scanner).Finish();
}

BoundedSupportFinalReplayRequest MakeFinalRequest(
    const BoundedSupportShapeScanRequest& shape,
    const bool baseEnabled,
    const std::string& placement,
    const int baseLayers)
{
    BoundedSupportFinalReplayRequest request;
    request.shapeReplay = shape;
    request.baseProjection.enabled = baseEnabled;
    request.baseProjection.layer_count = baseLayers;
    request.baseProjection.layer_placement = placement;
    request.baseProjection.source = "max_support_footprint";
    return request;
}

class CapturingSink final : public BoundedSupportFinalLayerSink
{
public:
    void Consume(const BoundedSupportFinalLayerStats& stats) override
    {
        componentSnapshots.emplace_back(
            stats.connectivity.components.begin(),
            stats.connectivity.components.end());
        BoundedSupportFinalLayerStats snapshot{stats};
        snapshot.connectivity.components = {};
        layers.push_back(std::move(snapshot));
    }

    std::vector<BoundedSupportFinalLayerStats> layers;
    std::vector<std::vector<BoundedSupportComponentSummary>>
        componentSnapshots;
};

class ThrowingSink final : public BoundedSupportFinalLayerSink
{
public:
    void Consume(const BoundedSupportFinalLayerStats&) override
    {
        throw std::runtime_error("test final replay sink failure");
    }
};

bool OverlayBaseOuterPriorityAndStatsMatchRetainedOrder()
{
    auto plan{BuildUpperProjectionPlan()};
    const auto shape{MakeShapeRequest()};
    const auto models{EmptyMasks()};
    const auto uppers{EmptyMasks()};
    const auto verified{BuildVerifiedShapeEvidence(plan, shape, models, uppers)};
    auto outer{EmptyMasks()};
    outer[0][kFirstFootprintPixel] = 1U;

    CapturingSink sink;
    BoundedSupportFinalReplayScanner scanner{
        MakeFinalRequest(shape, true, "overlay_existing", 3),
        plan,
        verified,
        &sink};
    std::vector<std::vector<std::uint8_t>> outputs;
    std::vector<std::vector<SupportType>> outputTypes;
    std::vector<std::vector<std::uint8_t>> clearedOutputs;
    bool passed{true};
    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        std::vector<std::uint8_t> output(kPixelCount, 9U);
        std::vector<SupportType> types(
            kPixelCount,
            SupportType::InternalVoid);
        std::vector<std::uint8_t> cleared(kPixelCount, 9U);
        scanner.ConsumeLayer(
            layerIndex,
            models[static_cast<std::size_t>(layerIndex)],
            uppers[static_cast<std::size_t>(layerIndex)],
            outer[static_cast<std::size_t>(layerIndex)],
            output,
            types,
            cleared);
        outputs.push_back(std::move(output));
        outputTypes.push_back(std::move(types));
        clearedOutputs.push_back(std::move(cleared));
    }
    const auto result{std::move(scanner).Finish()};

    passed = Expect(
                 outputs[0][kFirstFootprintPixel] == 0U
                     && outputTypes[0][kFirstFootprintPixel]
                         == SupportType::None
                     && clearedOutputs[0][kFirstFootprintPixel] == 1U,
                 "outer varnish clears Base support and its type")
        && passed;
    passed = Expect(
                 outputs[0][kSecondFootprintPixel] == 1U
                     && outputs[1][kFirstFootprintPixel] == 1U
                     && outputs[1][kSecondFootprintPixel] == 1U
                     && outputTypes[0][kSecondFootprintPixel]
                         == SupportType::ProjectionBase
                     && outputTypes[1][kFirstFootprintPixel]
                         == SupportType::ProjectionBase,
                 "overlay base fills empty early layers from the B3 footprint")
        && passed;
    passed = Expect(
                 outputs[2][kFirstFootprintPixel] == 1U
                     && outputs[2][kSecondFootprintPixel] == 1U
                     && outputTypes[2][kFirstFootprintPixel]
                         == SupportType::UpperProjection
                     && outputTypes[2][kSecondFootprintPixel]
                         == SupportType::UpperProjection,
                 "overlay base preserves existing support types")
        && passed;
    passed = Expect(
                 result.BaseProjection().enabled
                     && result.BaseProjection().configuredLayerCount == 3
                     && result.BaseProjection().effectiveLayerCount == 3
                     && result.BaseProjection().footprintPixels == 2U
                     && result.BaseProjection().addedSupportPixels == 4U,
                 "base summary matches retained maximum-footprint semantics")
        && passed;

    const auto& totals{result.Totals()};
    passed = Expect(
                 totals.completedLayerCount == 3
                     && totals.layersWithSupport == 3
                     && totals.supportPixels == 5U
                     && totals.clearedOuterVarnishSupportPixels == 1U,
                 "final totals are collected after outer-varnish priority")
        && passed;
    passed = Expect(
                 totals.typePixels[static_cast<std::size_t>(
                     SupportType::ProjectionBase)] == 3U
                     && totals.typePixels[static_cast<std::size_t>(
                         SupportType::UpperProjection)] == 2U,
                 "final type totals distinguish Base from retained support")
        && passed;
    passed = Expect(
                 totals.connectivityComponentCount == 5U
                     && totals.largestConnectivityComponentPixels == 1
                     && totals.tinyConnectivityComponentCount == 5U
                     && totals.smallConnectivityComponentCount == 0U,
                 "connectivity is scanned from final post-clear layers")
        && passed;
    passed = Expect(
                 sink.layers.size() == 3U
                     && sink.layers[0].supportPixels == 1U
                     && sink.layers[1].supportPixels == 2U
                     && sink.layers[2].supportPixels == 2U
                     && sink.layers[0].connectivity.componentCount == 1
                     && sink.layers[1].connectivity.componentCount == 2,
                 "layer sink receives compact final statistics in order")
        && passed;
    passed = Expect(
                 sink.componentSnapshots.size() == 3U
                     && sink.componentSnapshots[0].size() == 1U
                     && sink.componentSnapshots[0][0].areaPx == 1
                     && sink.componentSnapshots[0][0].minX == 3
                     && sink.componentSnapshots[0][0].minY == 2
                     && sink.componentSnapshots[0][0].maxX == 3
                     && sink.componentSnapshots[0][0].maxY == 2
                     && sink.componentSnapshots[1].size() == 2U
                     && std::all_of(
                         sink.componentSnapshots[1].begin(),
                         sink.componentSnapshots[1].end(),
                         [](const BoundedSupportComponentSummary& component)
                         {
                             return component.areaPx == 1;
                         }),
                 "sink deep-copies retained-equivalent component area and bbox evidence")
        && passed;
    return passed;
}

bool PrependBaseOverridesExistingSupportTypes()
{
    auto plan{BuildUpperProjectionPlan()};
    const auto shape{MakeShapeRequest()};
    const auto models{EmptyMasks()};
    const auto uppers{EmptyMasks()};
    const auto outer{EmptyMasks()};
    const auto verified{BuildVerifiedShapeEvidence(plan, shape, models, uppers)};
    BoundedSupportFinalReplayScanner scanner{
        MakeFinalRequest(shape, true, "prepend_below_model", 3),
        plan,
        verified};

    bool passed{true};
    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        std::vector<std::uint8_t> output(kPixelCount, 0U);
        std::vector<SupportType> types(kPixelCount, SupportType::None);
        std::vector<std::uint8_t> cleared(kPixelCount, 0U);
        scanner.ConsumeLayer(
            layerIndex,
            models[static_cast<std::size_t>(layerIndex)],
            uppers[static_cast<std::size_t>(layerIndex)],
            outer[static_cast<std::size_t>(layerIndex)],
            output,
            types,
            cleared);
        passed = Expect(
                     output[kFirstFootprintPixel] == 1U
                         && output[kSecondFootprintPixel] == 1U
                         && types[kFirstFootprintPixel]
                             == SupportType::ProjectionBase
                         && types[kSecondFootprintPixel]
                             == SupportType::ProjectionBase,
                     "prepend Base owns every support type in its layer range")
            && passed;
    }
    const auto result{std::move(scanner).Finish()};
    return Expect(result.BaseProjection().addedSupportPixels == 4U,
                  "prepend Base reports only newly added pixels")
        && Expect(result.Totals().supportPixels == 6U,
                  "prepend Base retains all footprint pixels")
        && Expect(
            result.Totals().typePixels[static_cast<std::size_t>(
                SupportType::ProjectionBase)] == 6U,
            "prepend Base overwrites existing Upper types")
        && Expect(
            result.Totals().typePixels[static_cast<std::size_t>(
                SupportType::UpperProjection)] == 0U,
            "no pre-Base type survives prepend ownership")
        && passed;
}

bool BaseProjectionMatchesDenseOracleForDisabledClampAndModelPriority()
{
    auto plan{BuildUpperProjectionPlan()};
    const auto shape{MakeShapeRequest()};
    auto models{EmptyMasks()};
    models[0][kFirstFootprintPixel] = 1U;
    auto uppers{EmptyMasks()};
    uppers[0][kFirstFootprintPixel] = 1U;
    const auto outer{EmptyMasks()};
    const auto verified{BuildVerifiedShapeEvidence(plan, shape, models, uppers)};
    const auto modelVolume{FlattenMasks(models)};

    auto denseClamped{ReplayShapeSupportVolume(plan, shape, models, uppers)};
    auto clampedRequest{MakeFinalRequest(
        shape,
        true,
        "overlay_existing",
        kLayerCount + 6)};
    const auto denseClampedSummary{ApplySupportBaseProjectionVolume(
        clampedRequest.baseProjection,
        modelVolume,
        denseClamped,
        kLayerCount,
        kPixelCount)};
    BoundedSupportFinalReplayScanner clampedScanner{
        clampedRequest,
        plan,
        verified};
    std::vector<std::uint8_t> support(kPixelCount, 9U);
    std::vector<SupportType> types(kPixelCount, SupportType::InternalVoid);
    std::vector<std::uint8_t> cleared(kPixelCount, 9U);
    bool passed{true};
    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        clampedScanner.ConsumeLayer(
            layerIndex,
            models[static_cast<std::size_t>(layerIndex)],
            uppers[static_cast<std::size_t>(layerIndex)],
            outer[static_cast<std::size_t>(layerIndex)],
            support,
            types,
            cleared);
        const auto offset{static_cast<std::size_t>(layerIndex) * kPixelCount};
        passed = Expect(
                     std::equal(
                         support.begin(),
                         support.end(),
                         denseClamped.begin() + static_cast<std::ptrdiff_t>(offset)),
                     "clamped bounded Base support matches dense volume oracle")
            && passed;
        passed = Expect(
                     cleared == std::vector<std::uint8_t>(kPixelCount, 0U),
                     "empty outer varnish produces no cleared overlap")
            && passed;
        if (layerIndex == 0)
        {
            passed = Expect(
                         support[kFirstFootprintPixel] == 0U
                             && types[kFirstFootprintPixel]
                                 == SupportType::None,
                         "model priority blocks Base support and type ownership")
                && passed;
        }
    }
    const auto clampedResult{std::move(clampedScanner).Finish()};
    passed = Expect(
                 clampedResult.BaseProjection().effectiveLayerCount
                         == denseClampedSummary.effective_layer_count
                     && clampedResult.BaseProjection().footprintPixels
                         == static_cast<std::size_t>(
                             denseClampedSummary.footprint_pixels)
                     && clampedResult.BaseProjection().addedSupportPixels
                         == static_cast<std::uint64_t>(
                             denseClampedSummary.added_support_pixels),
                 "clamped bounded Base summary matches dense volume oracle")
        && passed;
    passed = Expect(
                 denseClampedSummary.effective_layer_count == kLayerCount
                     && support[kFirstFootprintPixel] == 1U
                     && types[kFirstFootprintPixel]
                         == SupportType::UpperProjection,
                 "layer-count clamp preserves existing support ownership")
        && passed;

    auto denseDisabled{ReplayShapeSupportVolume(plan, shape, models, uppers)};
    auto disabledRequest{MakeFinalRequest(
        shape,
        false,
        "overlay_existing",
        kLayerCount)};
    const auto denseDisabledSummary{ApplySupportBaseProjectionVolume(
        disabledRequest.baseProjection,
        modelVolume,
        denseDisabled,
        kLayerCount,
        kPixelCount)};
    BoundedSupportFinalReplayScanner disabledScanner{
        disabledRequest,
        plan,
        verified};
    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        disabledScanner.ConsumeLayer(
            layerIndex,
            models[static_cast<std::size_t>(layerIndex)],
            uppers[static_cast<std::size_t>(layerIndex)],
            outer[static_cast<std::size_t>(layerIndex)],
            support,
            types,
            cleared);
        const auto offset{static_cast<std::size_t>(layerIndex) * kPixelCount};
        passed = Expect(
                     std::equal(
                         support.begin(),
                         support.end(),
                         denseDisabled.begin()
                             + static_cast<std::ptrdiff_t>(offset)),
                     "disabled bounded Base leaves shape replay identical to dense oracle")
            && passed;
    }
    const auto disabledResult{std::move(disabledScanner).Finish()};
    return Expect(
               !disabledResult.BaseProjection().enabled
                   && disabledResult.BaseProjection().effectiveLayerCount == 0
                   && disabledResult.BaseProjection().footprintPixels == 0U
                   && disabledResult.BaseProjection().addedSupportPixels == 0U
                   && !denseDisabledSummary.enabled
                   && denseDisabledSummary.effective_layer_count == 0,
               "disabled Base reports no effective projection")
        && Expect(
            denseClamped[kFirstFootprintPixel] == 0U,
            "model priority blocks Base projection in the dense and bounded result")
        && passed;
}

bool ConnectivityFourAndEightDistinguishDiagonalComponents()
{
    auto plan{BuildDiagonalUpperProjectionPlan()};
    const auto models{EmptyMasks()};
    const auto uppers{EmptyMasks()};
    const auto outer{EmptyMasks()};
    const auto run = [&](const int connectivity)
    {
        auto shape{MakeShapeRequest()};
        shape.connectivity = connectivity;
        const auto verified{
            BuildVerifiedShapeEvidence(plan, shape, models, uppers)};
        CapturingSink sink;
        BoundedSupportFinalReplayScanner scanner{
            MakeFinalRequest(shape, false, "overlay_existing", 0),
            plan,
            verified,
            &sink};
        std::vector<std::uint8_t> support(kPixelCount, 0U);
        std::vector<SupportType> types(kPixelCount, SupportType::None);
        std::vector<std::uint8_t> cleared(kPixelCount, 0U);
        for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
        {
            scanner.ConsumeLayer(
                layerIndex,
                models[static_cast<std::size_t>(layerIndex)],
                uppers[static_cast<std::size_t>(layerIndex)],
                outer[static_cast<std::size_t>(layerIndex)],
                support,
                types,
                cleared);
        }
        static_cast<void>(std::move(scanner).Finish());
        return sink;
    };

    const auto four{run(4)};
    const auto eight{run(8)};
    return Expect(
               four.layers.size() == 3U
                   && four.layers[2].connectivity.componentCount == 2
                   && four.layers[2].connectivity.largestComponentPixels == 1
                   && four.componentSnapshots[2].size() == 2U,
               "4-connectivity keeps diagonal support pixels separate")
        && Expect(
            eight.layers.size() == 3U
                && eight.layers[2].connectivity.componentCount == 1
                && eight.layers[2].connectivity.largestComponentPixels == 2
                && eight.componentSnapshots[2].size() == 1U
                && eight.componentSnapshots[2][0].areaPx == 2,
            "8-connectivity joins diagonal support pixels");
}

bool FinishLifecycleAndInvalidBaseConfigurationFailClosed()
{
    auto plan{BuildUpperProjectionPlan()};
    const auto shape{MakeShapeRequest()};
    const auto models{EmptyMasks()};
    const auto uppers{EmptyMasks()};
    const auto outer{EmptyMasks()};
    const auto verified{BuildVerifiedShapeEvidence(plan, shape, models, uppers)};
    BoundedSupportFinalReplayScanner scanner{
        MakeFinalRequest(shape, false, "overlay_existing", 0),
        plan,
        verified};
    bool passed = ExpectThrows<std::logic_error>(
        [&]
        {
            static_cast<void>(std::move(scanner).Finish());
        },
        "Finish rejects an incomplete replay");

    std::vector<std::uint8_t> support(kPixelCount, 0U);
    std::vector<SupportType> types(kPixelCount, SupportType::None);
    std::vector<std::uint8_t> cleared(kPixelCount, 0U);
    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        scanner.ConsumeLayer(
            layerIndex,
            models[static_cast<std::size_t>(layerIndex)],
            uppers[static_cast<std::size_t>(layerIndex)],
            outer[static_cast<std::size_t>(layerIndex)],
            support,
            types,
            cleared);
    }
    static_cast<void>(std::move(scanner).Finish());
    passed = ExpectThrows<std::logic_error>(
                 [&]
                 {
                     static_cast<void>(std::move(scanner).Finish());
                 },
                 "Finish rejects repeated completion")
        && passed;

    const auto rejects = [&](BoundedSupportFinalReplayRequest request)
    {
        return ExpectThrows<std::invalid_argument>(
            [&]
            {
                BoundedSupportFinalReplayScanner invalid{
                    request,
                    plan,
                    verified};
                static_cast<void>(invalid);
            },
            "invalid Base configuration is rejected at construction");
    };
    auto negativeLayers{MakeFinalRequest(
        shape,
        true,
        "overlay_existing",
        -1)};
    auto excessiveLayers{MakeFinalRequest(
        shape,
        true,
        "overlay_existing",
        1001)};
    auto invalidSource{MakeFinalRequest(
        shape,
        true,
        "overlay_existing",
        1)};
    invalidSource.baseProjection.source = "current_layer";
    auto invalidPlacement{MakeFinalRequest(shape, true, "replace", 1)};
    return rejects(negativeLayers)
        && rejects(excessiveLayers)
        && rejects(invalidSource)
        && rejects(invalidPlacement)
        && passed;
}

bool CallerBuffersAreReusedAcrossAllLayers()
{
    auto plan{BuildUpperProjectionPlan()};
    const auto shape{MakeShapeRequest()};
    const auto models{EmptyMasks()};
    const auto uppers{EmptyMasks()};
    const auto outer{EmptyMasks()};
    const auto verified{BuildVerifiedShapeEvidence(plan, shape, models, uppers)};
    BoundedSupportFinalReplayScanner scanner{
        MakeFinalRequest(shape, true, "overlay_existing", kLayerCount),
        plan,
        verified};
    std::vector<std::uint8_t> support(kPixelCount, 0U);
    std::vector<SupportType> types(kPixelCount, SupportType::None);
    std::vector<std::uint8_t> cleared(kPixelCount, 0U);
    const auto* const supportAddress{support.data()};
    const auto* const typeAddress{types.data()};
    const auto* const clearedAddress{cleared.data()};
    bool passed{true};
    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        scanner.ConsumeLayer(
            layerIndex,
            models[static_cast<std::size_t>(layerIndex)],
            uppers[static_cast<std::size_t>(layerIndex)],
            outer[static_cast<std::size_t>(layerIndex)],
            support,
            types,
            cleared);
        passed = Expect(
                     support.data() == supportAddress
                         && types.data() == typeAddress
                         && cleared.data() == clearedAddress,
                     "caller-owned output buffers retain their addresses")
            && passed;
    }
    const auto result{std::move(scanner).Finish()};
    return Expect(
               result.Totals().completedLayerCount == kLayerCount,
               "reused caller buffers complete every layer")
        && passed;
}

bool DigestMismatchFailsWithoutCommittingCallerBuffers()
{
    auto plan{BuildUpperProjectionPlan()};
    const auto shape{MakeShapeRequest()};
    const auto models{EmptyMasks()};
    auto uppers{EmptyMasks()};
    const auto outer{EmptyMasks()};
    const auto verified{BuildVerifiedShapeEvidence(plan, shape, models, uppers)};
    BoundedSupportFinalReplayScanner scanner{
        MakeFinalRequest(shape, false, "overlay_existing", 0),
        plan,
        verified};

    std::vector<std::uint8_t> support(kPixelCount, 9U);
    std::vector<SupportType> types(kPixelCount, SupportType::InternalVoid);
    std::vector<std::uint8_t> cleared(kPixelCount, 9U);
    scanner.ConsumeLayer(
        0, models[0], uppers[0], outer[0], support, types, cleared);

    uppers[1][5U] = 1U;
    std::fill(
        support.begin(),
        support.end(),
        static_cast<std::uint8_t>(9U));
    std::fill(types.begin(), types.end(), SupportType::InternalVoid);
    std::fill(
        cleared.begin(),
        cleared.end(),
        static_cast<std::uint8_t>(9U));
    bool passed = ExpectThrows<std::runtime_error>(
        [&]
        {
            scanner.ConsumeLayer(
                1, models[1], uppers[1], outer[1], support, types, cleared);
        },
        "changed replay input must fail its B3 digest");
    passed = Expect(
                 std::all_of(support.begin(), support.end(), [](const auto value)
                     { return value == 9U; })
                     && std::all_of(types.begin(), types.end(), [](const auto value)
                         { return value == SupportType::InternalVoid; })
                     && std::all_of(cleared.begin(), cleared.end(), [](const auto value)
                         { return value == 9U; }),
                 "digest failure leaves all caller outputs untouched")
        && passed;
    passed = ExpectThrows<std::logic_error>(
                 [&]
                 {
                     scanner.ConsumeLayer(
                         1, models[1], uppers[1], outer[1],
                         support, types, cleared);
                 },
                 "digest failure permanently terminates final replay")
        && passed;
    return passed;
}

bool ReplayIdentityMismatchIsRejectedBeforeConsumption()
{
    auto plan{BuildUpperProjectionPlan()};
    const auto shape{MakeShapeRequest()};
    const auto models{EmptyMasks()};
    const auto uppers{EmptyMasks()};
    const auto verified{BuildVerifiedShapeEvidence(plan, shape, models, uppers)};
    auto changedShape{shape};
    changedShape.connectivity = 8;
    bool passed = ExpectThrows<std::invalid_argument>(
        [&]
        {
            BoundedSupportFinalReplayScanner scanner{
                MakeFinalRequest(
                    changedShape,
                    false,
                    "overlay_existing",
                    0),
                plan,
                verified};
            static_cast<void>(scanner);
        },
        "B3 evidence from another replay identity must be rejected");
    auto differentPlan{BuildDifferentUpperProjectionPlan()};
    passed = ExpectThrows<std::invalid_argument>(
                 [&]
                 {
                     BoundedSupportFinalReplayScanner scanner{
                         MakeFinalRequest(
                             shape,
                             false,
                             "overlay_existing",
                             0),
                         differentPlan,
                         verified};
                     static_cast<void>(scanner);
                 },
                 "B3 evidence from another final demand plan must be rejected")
        && passed;
    return passed;
}

bool ValidationErrorsAreRetryableAndDoNotCommit()
{
    auto plan{BuildUpperProjectionPlan()};
    const auto shape{MakeShapeRequest()};
    const auto models{EmptyMasks()};
    const auto uppers{EmptyMasks()};
    const auto outer{EmptyMasks()};
    const auto verified{BuildVerifiedShapeEvidence(plan, shape, models, uppers)};
    BoundedSupportFinalReplayScanner scanner{
        MakeFinalRequest(shape, false, "overlay_existing", 0),
        plan,
        verified};

    std::vector<std::uint8_t> support(kPixelCount, 9U);
    std::vector<SupportType> types(kPixelCount, SupportType::InternalVoid);
    std::vector<std::uint8_t> cleared(kPixelCount, 9U);
    bool passed = ExpectThrows<std::invalid_argument>(
        [&]
        {
            scanner.ConsumeLayer(
                1, models[0], uppers[0], outer[0], support, types, cleared);
        },
        "out-of-order layer is rejected");

    auto nonBinaryOuter{outer[0]};
    nonBinaryOuter[3U] = 2U;
    passed = ExpectThrows<std::invalid_argument>(
                 [&]
                 {
                     scanner.ConsumeLayer(
                         0, models[0], uppers[0], nonBinaryOuter,
                         support, types, cleared);
                 },
                 "non-binary outer varnish input is rejected")
        && passed;

    std::vector<std::uint8_t> shortSupport(kPixelCount - 1U, 9U);
    passed = ExpectThrows<std::invalid_argument>(
                 [&]
                 {
                     scanner.ConsumeLayer(
                         0, models[0], uppers[0], outer[0],
                         shortSupport, types, cleared);
                 },
                 "wrong caller buffer size is rejected")
        && passed;

    auto aliasedModel{models[0]};
    passed = ExpectThrows<std::invalid_argument>(
                 [&]
                 {
                     scanner.ConsumeLayer(
                         0,
                         std::span<const std::uint8_t>{aliasedModel},
                         uppers[0],
                         outer[0],
                         std::span<std::uint8_t>{aliasedModel},
                         types,
                         cleared);
                 },
                 "input/output aliasing is rejected")
        && passed;
    passed = Expect(
                 scanner.ExpectedLayerIndex() == 0
                     && std::all_of(support.begin(), support.end(), [](const auto value)
                         { return value == 9U; })
                     && std::all_of(types.begin(), types.end(), [](const auto value)
                         { return value == SupportType::InternalVoid; })
                     && std::all_of(cleared.begin(), cleared.end(), [](const auto value)
                         { return value == 9U; }),
                 "validation failures do not advance state or commit outputs")
        && passed;

    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        scanner.ConsumeLayer(
            layerIndex,
            models[static_cast<std::size_t>(layerIndex)],
            uppers[static_cast<std::size_t>(layerIndex)],
            outer[static_cast<std::size_t>(layerIndex)],
            support,
            types,
            cleared);
    }
    const auto result{std::move(scanner).Finish()};
    return Expect(
               result.Totals().completedLayerCount == kLayerCount,
               "the same scanner completes after corrected validation inputs")
        && passed;
}

bool SinkFailureDoesNotCommitCallerBuffers()
{
    auto plan{BuildUpperProjectionPlan()};
    const auto shape{MakeShapeRequest()};
    const auto models{EmptyMasks()};
    const auto uppers{EmptyMasks()};
    const auto outer{EmptyMasks()};
    const auto verified{BuildVerifiedShapeEvidence(plan, shape, models, uppers)};
    ThrowingSink sink;
    BoundedSupportFinalReplayScanner scanner{
        MakeFinalRequest(shape, true, "overlay_existing", 3),
        plan,
        verified,
        &sink};
    std::vector<std::uint8_t> support(kPixelCount, 9U);
    std::vector<SupportType> types(kPixelCount, SupportType::InternalVoid);
    std::vector<std::uint8_t> cleared(kPixelCount, 9U);

    bool passed = ExpectThrows<std::runtime_error>(
        [&]
        {
            scanner.ConsumeLayer(
                0, models[0], uppers[0], outer[0], support, types, cleared);
        },
        "sink failure escapes final replay");
    passed = Expect(
                 std::all_of(support.begin(), support.end(), [](const auto value)
                     { return value == 9U; })
                     && std::all_of(types.begin(), types.end(), [](const auto value)
                         { return value == SupportType::InternalVoid; })
                     && std::all_of(cleared.begin(), cleared.end(), [](const auto value)
                         { return value == 9U; }),
                 "sink failure happens before caller-output commit")
        && passed;
    return passed;
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"overlay_base_outer_priority_and_stats",
         OverlayBaseOuterPriorityAndStatsMatchRetainedOrder},
        {"prepend_base_overrides_existing_types",
         PrependBaseOverridesExistingSupportTypes},
        {"base_dense_oracle_disabled_clamp_model_priority",
         BaseProjectionMatchesDenseOracleForDisabledClampAndModelPriority},
        {"connectivity_four_eight_diagonal",
         ConnectivityFourAndEightDistinguishDiagonalComponents},
        {"finish_lifecycle_and_invalid_base",
         FinishLifecycleAndInvalidBaseConfigurationFailClosed},
        {"caller_buffers_are_reused", CallerBuffersAreReusedAcrossAllLayers},
        {"digest_mismatch_does_not_commit",
         DigestMismatchFailsWithoutCommittingCallerBuffers},
        {"replay_identity_mismatch_is_rejected",
         ReplayIdentityMismatchIsRejectedBeforeConsumption},
        {"validation_errors_are_retryable",
         ValidationErrorsAreRetryableAndDoNotCommit},
        {"sink_failure_does_not_commit", SinkFailureDoesNotCommitCallerBuffers},
    };

    int failures{0};
    for (const auto& test : tests)
    {
        try
        {
            if (!test.second())
            {
                ++failures;
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << test.first << ": " << error.what() << '\n';
            ++failures;
        }
    }
    if (failures == 0)
    {
        std::cout << "PASS BoundedSupportFinalReplayTests "
                  << tests.size() << '/' << tests.size() << '\n';
        return 0;
    }
    std::cerr << "FAIL BoundedSupportFinalReplayTests "
              << failures << " failed\n";
    return 1;
}
