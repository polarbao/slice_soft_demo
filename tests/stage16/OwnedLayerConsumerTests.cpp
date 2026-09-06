#include "slicer_core/pipeline/LegacySceneLayerAdapter.h"
#include "slicer_core/pipeline/GlobalSurfaceShellProductionPipeline.h"
#include "slicer_core/pipeline/SlicePipeline.h"
#include "slicer_core/slicer.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using slicer_core::MaterialClosureSemanticLayerInput;
using slicer_core::RgbwsvProductionLayer;
using slicer_core::SliceRunLayerConsumeResult;
using slicer_core::SliceRunLayerConsumeStatus;
using slicer_core::SliceRunLayerConsumerError;
using slicer_core::SliceRunOptions;
using slicer_core::SliceRunOwnedLayer;

struct LayerSnapshot
{
    RgbwsvProductionLayer output;
    MaterialClosureSemanticLayerInput semantic;
};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

std::filesystem::path LegacyFixture()
{
    return std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "samples/configs/golden/material_process_top2_fixture.json";
}

std::filesystem::path GlobalFixture()
{
    return std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "samples/configs/texture_fill_partition/global_production_xiao_ma_white_fill.json";
}

SliceRunOptions NoOutputOptions()
{
    SliceRunOptions options;
    options.write_tiff_layers = false;
    options.write_preview_files = false;
    options.write_reports = false;
    return options;
}

bool SameSemantic(
    const MaterialClosureSemanticLayerInput& left,
    const MaterialClosureSemanticLayerInput& right)
{
    return left.layerIndex == right.layerIndex
        && left.zMm == right.zMm
        && left.widthPx == right.widthPx
        && left.heightPx == right.heightPx
        && left.textureSurfaceMask == right.textureSurfaceMask
        && left.modelFillMask == right.modelFillMask
        && left.modelMaterialMask == right.modelMaterialMask
        && left.supportFillMask == right.supportFillMask
        && left.internalVoidSupportMask == right.internalVoidSupportMask
        && left.surfaceVarnishMask == right.surfaceVarnishMask
        && left.outerVarnishShellMask == right.outerVarnishShellMask
        && left.modelEnvelopeMask == right.modelEnvelopeMask
        && left.supportRequiredMask == right.supportRequiredMask
        && left.expectedOccupiedDomainMask
            == right.expectedOccupiedDomainMask
        && left.layerEmptyMask == right.layerEmptyMask;
}

bool SameLayer(const LayerSnapshot& left, const LayerSnapshot& right)
{
    return left.output.layerIndex == right.output.layerIndex
        && left.output.zMm == right.output.zMm
        && left.output.widthPx == right.output.widthPx
        && left.output.heightPx == right.output.heightPx
        && left.output.channelOrder == right.output.channelOrder
        && left.output.channels == right.output.channels
        && SameSemantic(left.semantic, right.semantic);
}

bool OwnedConsumerMatchesConstCallbackAndMovesBuffers()
{
    std::vector<LayerSnapshot> borrowed;
    SliceRunOptions borrowedOptions = NoOutputOptions();
    borrowedOptions.layercallback =
        [&borrowed](
            const RgbwsvProductionLayer& output,
            const MaterialClosureSemanticLayerInput& semantic)
        {
            borrowed.push_back(LayerSnapshot{output, semantic});
        };
    const auto borrowedRun =
        slicer_core::run_slicer(LegacyFixture(), borrowedOptions);

    std::vector<LayerSnapshot> owned;
    bool rgbwsvMovePreservedAddress{true};
    bool semanticMovePreservedAddress{true};
    bool callbackActive{false};
    SliceRunOptions ownedOptions = NoOutputOptions();
    ownedOptions.ownedlayercallback =
        [&](SliceRunOwnedLayer&& produced)
        {
            const std::uint8_t* const rgbwsvAddress =
                produced.output.channels.data();
            const std::uint8_t* const semanticAddress =
                produced.semantic.modelMaterialMask.data();
            callbackActive = true;
            LayerSnapshot snapshot{
                std::move(produced.output),
                std::move(produced.semantic)};
            rgbwsvMovePreservedAddress =
                rgbwsvMovePreservedAddress
                && snapshot.output.channels.data() == rgbwsvAddress;
            semanticMovePreservedAddress =
                semanticMovePreservedAddress
                && snapshot.semantic.modelMaterialMask.data()
                    == semanticAddress;
            owned.push_back(std::move(snapshot));
            callbackActive = false;
            return SliceRunLayerConsumeResult{};
        };
    const auto ownedRun =
        slicer_core::run_slicer(LegacyFixture(), ownedOptions);

    bool sameLayers = borrowed.size() == owned.size();
    for (std::size_t index{0U}; sameLayers && index < borrowed.size(); ++index)
    {
        sameLayers = SameLayer(borrowed[index], owned[index]);
    }
    return ExpectTrue(!callbackActive, "owned callback returns synchronously")
        && ExpectTrue(
            borrowedRun.layer_count == ownedRun.layer_count
                && ownedRun.layer_count
                    == static_cast<int>(owned.size()),
            "owned callback consumes every produced layer")
        && ExpectTrue(sameLayers, "owned and const callbacks are byte-identical")
        && ExpectTrue(
            rgbwsvMovePreservedAddress,
            "RGBWSV storage survives the consumer move")
        && ExpectTrue(
            semanticMovePreservedAddress,
            "semantic storage survives the consumer move");
}

bool ConsumerOutcomeStopsAtRejectedLayer(
    const SliceRunLayerConsumeStatus status,
    const std::string& detail)
{
    int callbackCount{0};
    SliceRunOptions options = NoOutputOptions();
    options.ownedlayercallback =
        [&](SliceRunOwnedLayer&&)
        {
            ++callbackCount;
            return SliceRunLayerConsumeResult{status, detail};
        };
    try
    {
        (void)slicer_core::run_slicer(LegacyFixture(), options);
    }
    catch (const SliceRunLayerConsumerError& error)
    {
        return ExpectTrue(callbackCount == 1, "consumer stops after rejection")
            && ExpectTrue(error.Status() == status, "consumer status is stable")
            && ExpectTrue(error.LayerIndex() == 0, "consumer layer index is stable")
            && ExpectTrue(
                std::string{error.what()}.find(detail) != std::string::npos,
                "consumer detail is retained");
    }
    catch (...)
    {
    }
    return ExpectTrue(false, "consumer rejection must raise the typed error");
}

bool CancelledAndFailedOutcomesAreExplicit()
{
    return ConsumerOutcomeStopsAtRejectedLayer(
               SliceRunLayerConsumeStatus::Cancelled,
               "requested by test")
        && ConsumerOutcomeStopsAtRejectedLayer(
            SliceRunLayerConsumeStatus::Failed,
            "sink rejected test layer");
}

bool InvalidConsumerOutcomeMapsToStableFailure()
{
    SliceRunOptions options = NoOutputOptions();
    options.ownedlayercallback =
        [](SliceRunOwnedLayer&&)
        {
            return SliceRunLayerConsumeResult{
                static_cast<SliceRunLayerConsumeStatus>(999),
                "untrusted status"};
        };
    try
    {
        (void)slicer_core::run_slicer(LegacyFixture(), options);
    }
    catch (const SliceRunLayerConsumerError& error)
    {
        return ExpectTrue(
                   error.Status() == SliceRunLayerConsumeStatus::Failed,
                   "invalid consumer status maps to stable Failed")
            && ExpectTrue(
                error.LayerIndex() == 0,
                "invalid consumer status retains the layer index")
            && ExpectTrue(
                std::string{error.what()}.find("invalid status")
                    != std::string::npos,
                "invalid consumer status reports the contract violation");
    }
    return ExpectTrue(false, "invalid consumer status must fail closed");
}

bool ConsumerExceptionStopsProduction()
{
    int callbackCount{0};
    SliceRunOptions options = NoOutputOptions();
    options.ownedlayercallback =
        [&](SliceRunOwnedLayer&&) -> SliceRunLayerConsumeResult
        {
            ++callbackCount;
            throw std::runtime_error("test consumer exception");
        };
    try
    {
        (void)slicer_core::run_slicer(LegacyFixture(), options);
    }
    catch (const std::runtime_error& error)
    {
        return ExpectTrue(callbackCount == 1, "throwing consumer stops immediately")
            && ExpectTrue(
                std::string{error.what()} == "test consumer exception",
                "consumer exception is not hidden");
    }
    return ExpectTrue(false, "consumer exception must escape run_slicer");
}

bool InvalidCallbackCombinationsFailBeforeConfigLoad()
{
    const std::filesystem::path missing = "missing-owned-layer-config.json";
    SliceRunOptions both = NoOutputOptions();
    both.layercallback =
        [](const RgbwsvProductionLayer&,
           const MaterialClosureSemanticLayerInput&) {};
    both.ownedlayercallback =
        [](SliceRunOwnedLayer&&) { return SliceRunLayerConsumeResult{}; };

    bool bothRejected{false};
    try
    {
        (void)slicer_core::run_slicer(missing, both);
    }
    catch (const std::invalid_argument& error)
    {
        bothRejected = std::string{error.what()}.find("mutually exclusive")
            != std::string::npos;
    }

    SliceRunOptions writes = NoOutputOptions();
    writes.write_reports = true;
    writes.ownedlayercallback =
        [](SliceRunOwnedLayer&&) { return SliceRunLayerConsumeResult{}; };
    bool writesRejected{false};
    try
    {
        (void)slicer_core::run_slicer(missing, writes);
    }
    catch (const std::invalid_argument& error)
    {
        writesRejected = std::string{error.what()}.find("file output")
            != std::string::npos;
    }
    return ExpectTrue(bothRejected, "dual callbacks fail before config load")
        && ExpectTrue(writesRejected, "owned producer output fails before config load");
}

bool GlobalPipelineDoesNotSilentlyIgnoreOwnedConsumer()
{
    SliceRunOptions options = NoOutputOptions();
    options.ownedlayercallback =
        [](SliceRunOwnedLayer&&) { return SliceRunLayerConsumeResult{}; };
    bool routedRejected{false};
    try
    {
        (void)slicer_core::RunSlicePipeline(GlobalFixture(), options);
    }
    catch (const std::invalid_argument& error)
    {
        routedRejected = std::string{error.what()}.find("Legacy pipeline")
            != std::string::npos;
    }
    catch (...)
    {
    }

    bool directRejected{false};
    try
    {
        (void)slicer_core::RunGlobalSurfaceShellProductionPipeline(
            GlobalFixture(),
            options);
    }
    catch (const std::invalid_argument& error)
    {
        directRejected = std::string{error.what()}.find("Legacy pipeline")
            != std::string::npos;
    }
    catch (...)
    {
    }
    return ExpectTrue(
               routedRejected,
               "Global route rejects the Legacy-only owned consumer")
        && ExpectTrue(
            directRejected,
            "direct Global entry rejects the Legacy-only owned consumer");
}

class MutableCancelToken final : public slicer_core::api::ICancelToken
{
public:
    [[nodiscard]] bool IsCancelRequested() const noexcept override
    {
        return cancelled;
    }

    bool cancelled{false};
};

slicer_core::SceneRasterIdentity MakeIdentity()
{
    slicer_core::SceneRasterIdentity identity;
    identity.sceneid = "owned-cancel-scene";
    identity.modelid = "owned-cancel-model";
    identity.instanceid = "owned-cancel-instance";
    identity.scenerevision = 17U;
    identity.transformrevision = 23U;
    identity.admittedtransformrevision = 23U;
    identity.visible = true;
    identity.admitted = true;
    identity.effectivepipelinemode =
        slicer_core::SlicePipelineMode::Legacy;
    return identity;
}

void BindLegacyInstance(
    slicer_core::LegacySceneLayerAdapterRequest& request)
{
    const slicer_core::SliceConfig config =
        slicer_core::load_slice_config(request.configpath);
    const slicer_core::ModelReport source =
        slicer_core::load_model_report(
            config,
            request.configpath.parent_path());
    request.instance.instanceid = request.identity.instanceid;
    request.instance.modelid = request.identity.modelid;
    request.instance.sourcetransformidentity =
        source.model_path.generic_string();
    request.instance.transformrevision = request.identity.transformrevision;
    request.instance.sourcebboxmm = source.bbox_mm;
    request.instance.effectivebboxmm = source.bbox_mm;
    const auto transformHash = slicer_core::ComputeModelTransformHash(
        request.instance.transform,
        request.instance.sourcetransformidentity,
        request.instance.instanceid,
        request.instance.modelid);
    request.identity.transformhash = transformHash.hash;
    request.identity.admittedtransformhash = transformHash.hash;
}

bool AdapterCancellationClearsPartialLayers()
{
    MutableCancelToken token;
    slicer_core::LegacySceneLayerAdapterRequest request;
    request.configpath = LegacyFixture();
    request.identity = MakeIdentity();
    BindLegacyInstance(request);
    request.canceltoken = &token;
    request.progresscallback =
        [&token](const slicer_core::SliceRunProgress& progress)
        {
            if (progress.phase == "layer_processing"
                && progress.current > 0)
            {
                token.cancelled = true;
            }
        };

    const auto result = slicer_core::AdaptLegacySceneLayers(request);
    return ExpectTrue(
               result.error.has_value()
                   && result.error->code
                       == slicer_core::SceneRasterErrorCode::Cancelled,
               "adapter maps cooperative cancellation")
        && ExpectTrue(
            result.raster.layers.empty(),
            "adapter clears partially consumed layers");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, std::function<bool()>>> tests{
        {"owned_consumer_matches_const_callback_and_moves_buffers", OwnedConsumerMatchesConstCallbackAndMovesBuffers},
        {"cancelled_and_failed_outcomes_are_explicit", CancelledAndFailedOutcomesAreExplicit},
        {"invalid_consumer_outcome_maps_to_stable_failure", InvalidConsumerOutcomeMapsToStableFailure},
        {"consumer_exception_stops_production", ConsumerExceptionStopsProduction},
        {"invalid_callback_combinations_fail_before_config_load", InvalidCallbackCombinationsFailBeforeConfigLoad},
        {"global_pipeline_does_not_silently_ignore_owned_consumer", GlobalPipelineDoesNotSilentlyIgnoreOwnedConsumer},
        {"adapter_cancellation_clears_partial_layers", AdapterCancellationClearsPartialLayers},
    };

    int failed{0};
    for (const auto& [name, test] : tests)
    {
        try
        {
            if (test())
            {
                std::cout << "PASS " << name << '\n';
            }
            else
            {
                ++failed;
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
            ++failed;
        }
    }
    return failed == 0 ? 0 : 1;
}
