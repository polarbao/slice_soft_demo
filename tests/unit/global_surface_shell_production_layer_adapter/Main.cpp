#include "slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.h"
#include "slicer_core/pipeline/GlobalSurfaceShellProductionLayerAdapter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr int kWidth{11};
constexpr int kHeight{9};
constexpr int kDepth{2};
constexpr std::size_t kChannelCount{6U};

struct AdapterFixture
{
    slicer_core::TextureFillPartitionRasterMappingResult mapping;
    std::vector<slicer_core::TextureFillPartitionFullClosureLayerEvidence> evidence;
    slicer_core::TextureFillPartitionFullClosureAdapterResult closure;
};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::size_t PixelIndex(const int x, const int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(kWidth)
        + static_cast<std::size_t>(x);
}

std::size_t ChannelIndex(
    const int x,
    const int y,
    const std::size_t channel)
{
    return PixelIndex(x, y) * kChannelCount + channel;
}

void WriteRgb(
    std::vector<std::uint8_t>& channels,
    const int x,
    const int y,
    const std::array<std::uint8_t, 3>& rgb)
{
    channels.at(ChannelIndex(x, y, 0U)) = rgb.at(0);
    channels.at(ChannelIndex(x, y, 1U)) = rgb.at(1);
    channels.at(ChannelIndex(x, y, 2U)) = rgb.at(2);
}

AdapterFixture MakeFixture()
{
    AdapterFixture fixture;
    fixture.mapping.available = true;
    fixture.mapping.status = "diagnostic";
    fixture.mapping.grid.width = kWidth;
    fixture.mapping.grid.height = kHeight;
    fixture.mapping.grid.depth = kDepth;
    fixture.mapping.grid.originZMm = 0.0;
    fixture.mapping.grid.pixelPitchXMm = 0.05;
    fixture.mapping.grid.pixelPitchYMm = 0.05;
    fixture.mapping.grid.layerThicknessMm = 0.01;
    fixture.mapping.stats.partitionPass = true;

    const std::size_t pixelCount = static_cast<std::size_t>(kWidth * kHeight);
    for (int layerIndex{0}; layerIndex < kDepth; ++layerIndex)
    {
        const double zMm = (static_cast<double>(layerIndex) + 0.5) * 0.01;
        slicer_core::TextureFillPartitionRasterLayer mappingLayer;
        mappingLayer.layerIndex = layerIndex;
        mappingLayer.zMm = zMm;
        mappingLayer.modelMask.assign(pixelCount, 0U);
        mappingLayer.textureSurfaceMask.assign(pixelCount, 0U);
        mappingLayer.modelFillMask.assign(pixelCount, 0U);
        mappingLayer.textureRgb.assign(pixelCount, {255U, 255U, 255U});

        slicer_core::TextureFillPartitionFullClosureLayerEvidence evidence;
        evidence.layerIndex = layerIndex;
        evidence.zMm = zMm;
        evidence.widthPx = kWidth;
        evidence.heightPx = kHeight;
        evidence.supportFillMask.assign(pixelCount, 0U);
        evidence.internalVoidSupportMask.assign(pixelCount, 0U);
        evidence.surfaceVarnishMask.assign(pixelCount, 0U);
        evidence.outerVarnishShellMask.assign(pixelCount, 0U);
        evidence.modelEnvelopeMask.assign(pixelCount, 0U);
        evidence.supportRequiredMask.assign(pixelCount, 0U);
        evidence.channels.assign(pixelCount * kChannelCount, 255U);

        for (int y{2}; y <= 6; ++y)
        {
            for (int x{2}; x <= 4; ++x)
            {
                const std::size_t index = PixelIndex(x, y);
                evidence.modelEnvelopeMask.at(index) = 1U;
                const bool internalVoid = x == 3 && y == 4;
                if (internalVoid)
                {
                    evidence.supportFillMask.at(index) = 1U;
                    evidence.internalVoidSupportMask.at(index) = 1U;
                    evidence.supportRequiredMask.at(index) = 1U;
                    evidence.channels.at(ChannelIndex(x, y, 4U)) = 0U;
                    continue;
                }

                mappingLayer.modelMask.at(index) = 1U;
                const bool boundary = x == 2 || x == 4 || y == 2 || y == 6;
                mappingLayer.textureSurfaceMask.at(index) = boundary ? 1U : 0U;
                mappingLayer.modelFillMask.at(index) = boundary ? 0U : 1U;
                if (boundary)
                {
                    mappingLayer.textureRgb.at(index) = {20U, 40U, 60U};
                    WriteRgb(evidence.channels, x, y, {20U, 40U, 60U});
                }
                else
                {
                    evidence.channels.at(ChannelIndex(x, y, 3U)) = 0U;
                }
                if (y == 2)
                {
                    evidence.surfaceVarnishMask.at(index) = 1U;
                    evidence.channels.at(ChannelIndex(x, y, 5U)) = 0U;
                }
            }
        }

        for (int y{2}; y <= 6; ++y)
        {
            for (int x{5}; x <= 7; ++x)
            {
                const std::size_t index = PixelIndex(x, y);
                evidence.supportFillMask.at(index) = 1U;
                evidence.supportRequiredMask.at(index) = 1U;
                evidence.channels.at(ChannelIndex(x, y, 4U)) = 0U;
            }
            const std::size_t varnishIndex = PixelIndex(8, y);
            evidence.outerVarnishShellMask.at(varnishIndex) = 1U;
            evidence.channels.at(ChannelIndex(8, y, 5U)) = 0U;
        }

        fixture.mapping.layers.push_back(std::move(mappingLayer));
        fixture.evidence.push_back(std::move(evidence));
    }

    slicer_core::TextureFillPartitionFullClosureAdapterRequest closureRequest;
    closureRequest.rasterMapping = &fixture.mapping;
    closureRequest.layers = &fixture.evidence;
    fixture.closure = slicer_core::AdaptTextureFillPartitionFullClosure(
        closureRequest);
    return fixture;
}

slicer_core::GlobalSurfaceShellProductionLayerAdapterRequest MakeRequest(
    const AdapterFixture& fixture)
{
    slicer_core::GlobalSurfaceShellProductionLayerAdapterRequest request;
    request.rasterMapping = &fixture.mapping;
    request.fullClosure = &fixture.closure;
    request.closureEvidence = &fixture.evidence;
    return request;
}

bool PassingClosureProducesWriterReadyLayers()
{
    const AdapterFixture fixture = MakeFixture();
    const auto result = slicer_core::AdaptGlobalSurfaceShellProductionLayers(
        MakeRequest(fixture));

    return ExpectTrue(fixture.closure.fullClosurePass, "fixture closure passes")
        && ExpectTrue(result.available, "production layer adapter is available")
        && ExpectTrue(result.status == "ready_for_writer", "adapter is writer-ready")
        && ExpectTrue(
            result.productionAcceptance == "not_evaluated",
            "adapter does not grant production admission")
        && ExpectTrue(
            !result.productionOutputWritten,
            "adapter writes no TIFF or package")
        && ExpectTrue(result.fullClosurePass, "full closure evidence is retained")
        && ExpectTrue(result.widthPx == kWidth, "width is retained")
        && ExpectTrue(result.heightPx == kHeight, "height is retained")
        && ExpectTrue(result.layerCount == kDepth, "layer count is retained")
        && ExpectTrue(
            result.protocol.schema == "p0.rgbwsv.2",
            "package schema remains fixed")
        && ExpectTrue(
            result.protocol.bit_depth == 8,
            "bit depth remains uint8")
        && ExpectTrue(
            result.protocol.polarity == "black_is_print",
            "polarity remains black_is_print")
        && ExpectTrue(
            result.layers.at(1).output.layerIndex == 1,
            "true layer index is retained")
        && ExpectTrue(
            result.layers.at(1).output.zMm == 0.015,
            "true Z is retained")
        && ExpectTrue(
            result.layers.at(0).output.channels
                == fixture.evidence.at(0).channels,
            "final RGBWSV bytes are copied without recomposition")
        && ExpectTrue(
            result.layers.at(0).semantic.internalVoidSupportMask
                    .at(PixelIndex(3, 4))
                == 1U,
            "internal-void support semantics are retained")
        && ExpectTrue(
            result.layers.at(0).semantic.outerVarnishShellMask
                    .at(PixelIndex(8, 4))
                == 1U,
            "outer varnish semantics are retained");
}

bool FailedClosureIsRejected()
{
    AdapterFixture fixture = MakeFixture();
    fixture.closure.fullClosurePass = false;
    fixture.closure.status = "fail";
    const auto result = slicer_core::AdaptGlobalSurfaceShellProductionLayers(
        MakeRequest(fixture));

    return ExpectTrue(!result.available, "failed closure blocks adapter")
        && ExpectTrue(
            result.errorCode
                == slicer_core::SlicePipelineErrorCode::GlobalAdapterClosureRequired,
            "failed closure uses stable error code");
}

bool MutatedEvidenceAfterClosureIsRejected()
{
    AdapterFixture fixture = MakeFixture();
    fixture.evidence.at(0).channels.at(ChannelIndex(2, 2, 0U)) = 99U;
    const auto result = slicer_core::AdaptGlobalSurfaceShellProductionLayers(
        MakeRequest(fixture));

    return ExpectTrue(!result.available, "mutated final bytes block adapter")
        && ExpectTrue(
            result.errorCode
                == slicer_core::SlicePipelineErrorCode::GlobalAdapterLayerMismatch,
            "mutated evidence uses stable mismatch code");
}

bool ProtocolOrderMutationIsRejected()
{
    AdapterFixture fixture = MakeFixture();
    std::swap(
        fixture.evidence.at(0).channelOrder.at(3),
        fixture.evidence.at(0).channelOrder.at(4));
    const auto result = slicer_core::AdaptGlobalSurfaceShellProductionLayers(
        MakeRequest(fixture));

    return ExpectTrue(!result.available, "channel-order mutation blocks adapter")
        && ExpectTrue(
            result.errorCode
                == slicer_core::SlicePipelineErrorCode::GlobalAdapterProtocolMismatch,
            "channel-order mutation uses stable protocol code");
}

bool MalformedLayerShapeIsRejected()
{
    AdapterFixture fixture = MakeFixture();
    fixture.mapping.layers.at(0).textureRgb.pop_back();
    const auto result = slicer_core::AdaptGlobalSurfaceShellProductionLayers(
        MakeRequest(fixture));

    return ExpectTrue(!result.available, "malformed layer shape blocks adapter")
        && ExpectTrue(
            result.errorCode
                == slicer_core::SlicePipelineErrorCode::GlobalAdapterLayerMismatch,
            "malformed layer shape uses stable mismatch code");
}

bool MissingInputsAreRejected()
{
    const auto result = slicer_core::AdaptGlobalSurfaceShellProductionLayers({});
    return ExpectTrue(!result.available, "missing inputs block adapter")
        && ExpectTrue(
            result.errorCode
                == slicer_core::SlicePipelineErrorCode::GlobalAdapterInputInvalid,
            "missing inputs use stable input code");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"passing_closure_produces_writer_ready_layers", PassingClosureProducesWriterReadyLayers},
        {"failed_closure_is_rejected", FailedClosureIsRejected},
        {"mutated_evidence_after_closure_is_rejected", MutatedEvidenceAfterClosureIsRejected},
        {"protocol_order_mutation_is_rejected", ProtocolOrderMutationIsRejected},
        {"malformed_layer_shape_is_rejected", MalformedLayerShapeIsRejected},
        {"missing_inputs_are_rejected", MissingInputsAreRejected},
    };

    bool passed{true};
    for (const auto& test : tests)
    {
        const bool current = test.second();
        std::cout << (current ? "PASS: " : "FAIL: ") << test.first << '\n';
        passed = current && passed;
    }
    return passed ? 0 : 1;
}
