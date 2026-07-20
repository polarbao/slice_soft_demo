#include "slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.h"

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
constexpr std::size_t kChannelCount{6U};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool HasIssueCode(
    const std::vector<slicer_core::ValidationIssue>& issues,
    const std::string& code)
{
    for (const slicer_core::ValidationIssue& issue : issues)
    {
        if (issue.code == code)
        {
            return true;
        }
    }
    return false;
}

std::size_t PixelIndex(const int x, const int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(kWidth)
        + static_cast<std::size_t>(x);
}

std::size_t ChannelIndex(const int x, const int y, const std::size_t channel)
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

void ClearPixel(std::vector<std::uint8_t>& channels, const int x, const int y)
{
    for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
    {
        channels.at(ChannelIndex(x, y, channel)) = 255U;
    }
}

struct FullClosureFixture
{
    slicer_core::TextureFillPartitionRasterMappingResult mapping;
    std::vector<slicer_core::TextureFillPartitionFullClosureLayerEvidence> layers;
};

FullClosureFixture MakeFixture(const bool allTexture = false)
{
    FullClosureFixture fixture;
    fixture.mapping.available = true;
    fixture.mapping.status = "diagnostic";
    fixture.mapping.allTexture = allTexture;
    fixture.mapping.grid.width = kWidth;
    fixture.mapping.grid.height = kHeight;
    fixture.mapping.grid.depth = 1;
    fixture.mapping.grid.originZMm = 0.0;
    fixture.mapping.grid.pixelPitchXMm = 0.05;
    fixture.mapping.grid.pixelPitchYMm = 0.05;
    fixture.mapping.grid.layerThicknessMm = 0.01;
    fixture.mapping.stats.partitionPass = true;
    fixture.mapping.layers.resize(1U);

    const std::size_t pixelCount = static_cast<std::size_t>(kWidth * kHeight);
    slicer_core::TextureFillPartitionRasterLayer& mappingLayer =
        fixture.mapping.layers.at(0);
    mappingLayer.layerIndex = 0;
    mappingLayer.zMm = 0.005;
    mappingLayer.modelMask.assign(pixelCount, 0U);
    mappingLayer.textureSurfaceMask.assign(pixelCount, 0U);
    mappingLayer.modelFillMask.assign(pixelCount, 0U);
    mappingLayer.textureRgb.assign(pixelCount, {255U, 255U, 255U});

    slicer_core::TextureFillPartitionFullClosureLayerEvidence evidence;
    evidence.layerIndex = 0;
    evidence.zMm = 0.005;
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
            const bool texture = allTexture || boundary;
            mappingLayer.textureSurfaceMask.at(index) = texture ? 1U : 0U;
            mappingLayer.modelFillMask.at(index) = texture ? 0U : 1U;
            if (texture)
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

    fixture.layers.push_back(std::move(evidence));
    return fixture;
}

slicer_core::TextureFillPartitionFullClosureAdapterRequest MakeRequest(
    const FullClosureFixture& fixture)
{
    slicer_core::TextureFillPartitionFullClosureAdapterRequest request;
    request.rasterMapping = &fixture.mapping;
    request.layers = &fixture.layers;
    request.connectivity = 8;
    request.maxGapPx = 1;
    return request;
}

bool ExactFullClosurePasses()
{
    const FullClosureFixture fixture = MakeFixture();
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(result.available, "full closure evidence is available")
        && ExpectTrue(result.status == "diagnostic", "passing full closure remains diagnostic")
        && ExpectTrue(result.scope == "full_material_domain", "full scope is explicit")
        && ExpectTrue(result.source == "semantic_masks", "source is exact masks")
        && ExpectTrue(result.confidence == "exact", "confidence is exact")
        && ExpectTrue(result.fullClosurePass, "full closure passes")
        && ExpectTrue(result.modelClosureStatus == "pass", "model closure is evaluated")
        && ExpectTrue(result.supportClosureStatus == "pass", "support closure is evaluated")
        && ExpectTrue(result.varnishClosureStatus == "pass", "varnish closure is evaluated")
        && ExpectTrue(result.totalExpectedDomainGapPixels == 0U, "expected domain has no gap")
        && ExpectTrue(result.layers.at(0).sidecar.expectedOccupiedDomainMask.at(PixelIndex(3, 4)) == 1U, "internal void belongs to expected domain")
        && ExpectTrue(!result.productionOutputWritten, "full closure writes no production output")
        && ExpectTrue(result.productionAcceptance == "not_evaluated", "full closure is not admitted");
}

bool InternalVoidSupportIsPreserved()
{
    const FullClosureFixture fixture = MakeFixture();
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    const auto& sidecar = result.layers.at(0).sidecar;
    return ExpectTrue(sidecar.internalVoidSupportMask.at(PixelIndex(3, 4)) == 1U, "internal void mask is preserved")
        && ExpectTrue(sidecar.supportFillMask.at(PixelIndex(3, 4)) == 1U, "internal void uses support")
        && ExpectTrue(result.totalInternalVoidGapPixels == 0U, "filled internal void has no gap");
}

bool AllTextureKeepsZeroFill()
{
    const FullClosureFixture fixture = MakeFixture(true);
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(result.available && result.fullClosurePass, "all-texture full closure passes")
        && ExpectTrue(result.allTexture, "all-texture state is retained")
        && ExpectTrue(result.colorFillApplicability == "not_applicable", "color/fill gap is not applicable")
        && ExpectTrue(result.allTextureReason == "all_texture_partition", "all-texture reason is stable");
}

bool MissingInternalVoidSupportFails()
{
    FullClosureFixture fixture = MakeFixture();
    auto& layer = fixture.layers.at(0);
    const std::size_t index = PixelIndex(3, 4);
    layer.supportFillMask.at(index) = 0U;
    layer.internalVoidSupportMask.at(index) = 0U;
    ClearPixel(layer.channels, 3, 4);
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(result.available, "internal-void gap remains inspectable")
        && ExpectTrue(result.status == "fail", "internal-void gap fails")
        && ExpectTrue(result.totalExpectedDomainGapPixels == 1U, "one expected-domain gap is counted")
        && ExpectTrue(result.totalInternalVoidGapPixels == 1U, "one internal-void gap is classified")
        && ExpectTrue(result.supportClosureStatus == "fail", "support closure fails");
}

bool MissingModelSupportBridgeFails()
{
    FullClosureFixture fixture = MakeFixture();
    auto& layer = fixture.layers.at(0);
    layer.supportFillMask.at(PixelIndex(5, 4)) = 0U;
    ClearPixel(layer.channels, 5, 4);
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(result.available && result.status == "fail", "model/support gap fails")
        && ExpectTrue(result.totalModelSupportGapPixels == 1U, "model/support gap is classified")
        && ExpectTrue(result.totalExpectedDomainGapPixels == 1U, "model/support expected gap is counted");
}

bool MissingColorSupportBridgeFails()
{
    FullClosureFixture fixture = MakeFixture();
    auto& layer = fixture.layers.at(0);
    layer.supportFillMask.at(PixelIndex(5, 3)) = 0U;
    ClearPixel(layer.channels, 5, 3);
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(result.available && result.status == "fail", "color/support gap fails")
        && ExpectTrue(result.totalColorSupportGapPixels == 1U, "color/support gap is classified");
}

bool MissingVarnishSupportBridgeFails()
{
    FullClosureFixture fixture = MakeFixture();
    auto& layer = fixture.layers.at(0);
    layer.supportFillMask.at(PixelIndex(7, 4)) = 0U;
    ClearPixel(layer.channels, 7, 4);
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(result.available && result.status == "fail", "varnish/support gap fails")
        && ExpectTrue(result.totalVarnishSupportGapPixels == 1U, "varnish/support gap is classified")
        && ExpectTrue(result.varnishClosureStatus == "fail", "varnish closure fails");
}

bool FinalPriorityConflictBlocks()
{
    FullClosureFixture fixture = MakeFixture();
    auto& layer = fixture.layers.at(0);
    layer.supportFillMask.at(PixelIndex(2, 2)) = 1U;
    layer.supportRequiredMask.at(PixelIndex(2, 2)) = 1U;
    layer.channels.at(ChannelIndex(2, 2, 4U)) = 0U;
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(!result.available, "final model/support conflict blocks")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_FULL_CLOSURE_PRIORITY_CONFLICT"),
            "priority conflict uses stable code");
}

bool SemanticChannelMismatchFails()
{
    FullClosureFixture fixture = MakeFixture();
    fixture.layers.at(0).channels.at(ChannelIndex(6, 4, 4U)) = 255U;
    fixture.layers.at(0).channels.at(ChannelIndex(6, 4, 0U)) = 0U;
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(result.available && result.status == "fail", "semantic/channel mismatch fails")
        && ExpectTrue(result.totalSemanticChannelMismatchPixels == 1U, "one semantic mismatch is counted")
        && ExpectTrue(result.supportClosureStatus == "fail", "support mismatch fails support closure")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_FULL_CLOSURE_SEMANTIC_MISMATCH"),
            "semantic mismatch uses stable code");
}

bool VarnishChannelMismatchFails()
{
    FullClosureFixture fixture = MakeFixture();
    fixture.layers.at(0).channels.at(ChannelIndex(8, 4, 5U)) = 255U;
    fixture.layers.at(0).channels.at(ChannelIndex(8, 4, 0U)) = 0U;
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(result.available && result.status == "fail", "varnish/channel mismatch fails")
        && ExpectTrue(result.totalSemanticChannelMismatchPixels == 1U, "one varnish mismatch is counted")
        && ExpectTrue(result.varnishClosureStatus == "fail", "varnish mismatch fails varnish closure")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_FULL_CLOSURE_SEMANTIC_MISMATCH"),
            "varnish mismatch uses stable code");
}

bool UnexpectedVarnishChannelFails()
{
    FullClosureFixture fixture = MakeFixture();
    fixture.layers.at(0).channels.at(ChannelIndex(2, 3, 5U)) = 0U;
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(result.available && result.status == "fail", "unexpected varnish channel fails")
        && ExpectTrue(result.totalVarnishChannelMismatchPixels == 1U, "one unexpected varnish pixel is counted")
        && ExpectTrue(result.varnishClosureStatus == "fail", "unexpected varnish fails varnish closure");
}

bool VarnishModelFillIsAllowed()
{
    FullClosureFixture fixture = MakeFixture();
    fixture.layers.at(0).channels.at(ChannelIndex(3, 3, 3U)) = 255U;
    fixture.layers.at(0).channels.at(ChannelIndex(3, 3, 5U)) = 0U;
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(result.available && result.status == "diagnostic", "varnish model fill remains valid")
        && ExpectTrue(result.totalVarnishChannelMismatchPixels == 0U, "varnish fill is not a surface mismatch")
        && ExpectTrue(result.fullClosurePass, "varnish model fill closes exactly");
}

bool LayerOrderMismatchBlocks()
{
    FullClosureFixture fixture = MakeFixture();
    fixture.layers.at(0).layerIndex = 1;
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(!result.available, "layer order mismatch blocks")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_FULL_CLOSURE_LAYER_ORDER_INVALID"),
            "layer order uses stable code");
}

bool NonBinaryMaskBlocks()
{
    FullClosureFixture fixture = MakeFixture();
    fixture.layers.at(0).supportFillMask.at(PixelIndex(6, 4)) = 2U;
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(!result.available, "non-binary mask blocks")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_FULL_CLOSURE_MASK_INVALID"),
            "non-binary mask uses stable code");
}

bool UnexpectedMaterialFails()
{
    FullClosureFixture fixture = MakeFixture();
    fixture.layers.at(0).channels.at(ChannelIndex(0, 0, 4U)) = 0U;
    const auto result = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(result.available && result.status == "fail", "unexpected material fails")
        && ExpectTrue(result.totalUnexpectedOccupiedPixels == 1U, "unexpected material is counted")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_FULL_CLOSURE_UNEXPECTED_MATERIAL"),
            "unexpected material uses stable code");
}

bool RepeatResultIsDeterministic()
{
    const FullClosureFixture fixture = MakeFixture();
    const auto first = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    const auto second = slicer_core::AdaptTextureFillPartitionFullClosure(
        MakeRequest(fixture));
    return ExpectTrue(first.available && second.available, "repeat full closure is available")
        && ExpectTrue(first.status == second.status, "repeat status is stable")
        && ExpectTrue(first.totalExpectedDomainGapPixels == second.totalExpectedDomainGapPixels, "repeat gap count is stable")
        && ExpectTrue(first.layers.at(0).sidecar.expectedOccupiedDomainMask == second.layers.at(0).sidecar.expectedOccupiedDomainMask, "repeat sidecar is stable");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"exact_full_closure_passes", ExactFullClosurePasses},
        {"internal_void_support_is_preserved", InternalVoidSupportIsPreserved},
        {"all_texture_keeps_zero_fill", AllTextureKeepsZeroFill},
        {"missing_internal_void_support_fails", MissingInternalVoidSupportFails},
        {"missing_model_support_bridge_fails", MissingModelSupportBridgeFails},
        {"missing_color_support_bridge_fails", MissingColorSupportBridgeFails},
        {"missing_varnish_support_bridge_fails", MissingVarnishSupportBridgeFails},
        {"final_priority_conflict_blocks", FinalPriorityConflictBlocks},
        {"semantic_channel_mismatch_fails", SemanticChannelMismatchFails},
        {"varnish_channel_mismatch_fails", VarnishChannelMismatchFails},
        {"unexpected_varnish_channel_fails", UnexpectedVarnishChannelFails},
        {"varnish_model_fill_is_allowed", VarnishModelFillIsAllowed},
        {"layer_order_mismatch_blocks", LayerOrderMismatchBlocks},
        {"non_binary_mask_blocks", NonBinaryMaskBlocks},
        {"unexpected_material_fails", UnexpectedMaterialFails},
        {"repeat_result_is_deterministic", RepeatResultIsDeterministic},
    };
    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        if (!test.second())
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }
    std::cout << "Texture/fill partition full closure adapter unit tests complete.\n";
    return 0;
}
