#include "slicer_core/diagnostics/TextureFillPartitionClosureAdapter.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

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

std::size_t PixelIndex(const int width, const int x, const int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x);
}

std::size_t ChannelIndex(const std::size_t pixelIndex, const std::size_t channel)
{
    return pixelIndex * 6U + channel;
}

slicer_core::GlobalTextureFillPartitionResult MakePartition(
    const bool allTexture,
    const int depth = 1)
{
    slicer_core::GlobalTextureFillPartitionResult partition;
    partition.available = true;
    partition.partitionPass = true;
    partition.status = "diagnostic";
    partition.backend = "closure_fixture";
    partition.grid.width = 7;
    partition.grid.height = 7;
    partition.grid.depth = depth;
    partition.grid.originZMm = 0.0;
    partition.grid.spacingXMm = 0.05;
    partition.grid.spacingYMm = 0.05;
    partition.grid.spacingZMm = 0.01;
    partition.modelMask.grid = partition.grid;
    partition.textureSurfaceMask.grid = partition.grid;
    partition.modelFillMask.grid = partition.grid;

    const std::size_t layerArea = 49U;
    const std::size_t voxelCount = layerArea * static_cast<std::size_t>(depth);
    partition.modelMask.values.assign(voxelCount, 0U);
    partition.textureSurfaceMask.values.assign(voxelCount, 0U);
    partition.modelFillMask.values.assign(voxelCount, 0U);
    for (int layerIndex{0}; layerIndex < depth; ++layerIndex)
    {
        const std::size_t layerBegin = static_cast<std::size_t>(layerIndex) * layerArea;
        for (int y{1}; y <= 5; ++y)
        {
            for (int x{1}; x <= 5; ++x)
            {
                const std::size_t index = layerBegin + PixelIndex(7, x, y);
                const bool boundary = x == 1 || x == 5 || y == 1 || y == 5;
                partition.modelMask.values.at(index) = 1U;
                partition.textureSurfaceMask.values.at(index) =
                    allTexture || boundary ? 1U : 0U;
                partition.modelFillMask.values.at(index) =
                    !allTexture && !boundary ? 1U : 0U;
            }
        }
    }
    partition.stats.modelVoxels = 25U * static_cast<std::uint64_t>(depth);
    partition.stats.textureSurfaceVoxels =
        (allTexture ? 25U : 16U) * static_cast<std::uint64_t>(depth);
    partition.stats.modelFillVoxels =
        (allTexture ? 0U : 9U) * static_cast<std::uint64_t>(depth);
    return partition;
}

slicer_core::TextureFillPartitionDiagnosticComposerResult MakeComposer(
    const slicer_core::GlobalTextureFillPartitionResult& partition)
{
    slicer_core::TextureFillPartitionDiagnosticComposerResult composer;
    composer.available = true;
    composer.status = "diagnostic";
    composer.width = partition.grid.width;
    composer.height = partition.grid.height;
    composer.depth = partition.grid.depth;
    const std::size_t layerArea = static_cast<std::size_t>(composer.width)
        * static_cast<std::size_t>(composer.height);
    for (int layerIndex{0}; layerIndex < composer.depth; ++layerIndex)
    {
        slicer_core::TextureFillPartitionDiagnosticLayer layer;
        layer.layerIndex = layerIndex;
        layer.zMm = partition.grid.originZMm
            + (static_cast<double>(layerIndex) + 0.5)
                * partition.grid.spacingZMm;
        layer.textureSurfaceMask.assign(layerArea, 0U);
        layer.modelFillMask.assign(layerArea, 0U);
        layer.composed.width = composer.width;
        layer.composed.height = composer.height;
        layer.composed.channels.assign(layerArea * 6U, 255U);
        const std::size_t layerBegin = static_cast<std::size_t>(layerIndex)
            * layerArea;
        for (std::size_t pixelIndex{0U}; pixelIndex < layerArea; ++pixelIndex)
        {
            const std::size_t voxelIndex = layerBegin + pixelIndex;
            const bool texture = partition.textureSurfaceMask.values.at(voxelIndex) != 0U;
            const bool fill = partition.modelFillMask.values.at(voxelIndex) != 0U;
            layer.textureSurfaceMask.at(pixelIndex) = texture ? 1U : 0U;
            layer.modelFillMask.at(pixelIndex) = fill ? 1U : 0U;
            if (texture)
            {
                layer.composed.channels.at(ChannelIndex(pixelIndex, 0U)) = 10U;
                layer.composed.channels.at(ChannelIndex(pixelIndex, 1U)) = 20U;
                layer.composed.channels.at(ChannelIndex(pixelIndex, 2U)) = 30U;
                ++composer.stats.textureSurfaceVoxels;
            }
            if (fill)
            {
                layer.composed.channels.at(ChannelIndex(pixelIndex, 3U)) = 0U;
                ++composer.stats.modelFillVoxels;
                ++composer.stats.modelFillWhiteVoxels;
            }
        }
        composer.layers.push_back(std::move(layer));
    }
    return composer;
}

slicer_core::TextureFillPartitionClosureAdapterRequest MakeRequest(
    const slicer_core::GlobalTextureFillPartitionResult& partition,
    const slicer_core::TextureFillPartitionDiagnosticComposerResult& composer)
{
    slicer_core::TextureFillPartitionClosureAdapterRequest request;
    request.partition = &partition;
    request.composer = &composer;
    request.connectivity = 8;
    request.maxGapPx = 1;
    return request;
}

bool OrdinaryPartitionExactPass()
{
    const auto partition = MakePartition(false);
    const auto composer = MakeComposer(partition);
    const auto result = slicer_core::AdaptTextureFillPartitionClosure(
        MakeRequest(partition, composer));
    return ExpectTrue(result.available, "ordinary closure linkage is available")
        && ExpectTrue(result.status == "diagnostic", "ordinary linkage is diagnostic")
        && ExpectTrue(result.scope == "texture_model_fill_only", "scope is explicit")
        && ExpectTrue(result.source == "semantic_masks", "source is exact masks")
        && ExpectTrue(result.confidence == "exact", "confidence is exact")
        && ExpectTrue(result.colorFillApplicability == "applicable", "color/fill is applicable")
        && ExpectTrue(result.totalColorFillGapVoxels == 0U, "ordinary color/fill has no gap")
        && ExpectTrue(result.totalModelDomainGapVoxels == 0U, "ordinary model domain has no gap")
        && ExpectTrue(result.supportClosureStatus == "not_evaluated", "support is not fabricated")
        && ExpectTrue(result.varnishClosureStatus == "not_evaluated", "varnish is not fabricated");
}

bool AllTextureIsNotApplicable()
{
    const auto partition = MakePartition(true);
    const auto composer = MakeComposer(partition);
    const auto result = slicer_core::AdaptTextureFillPartitionClosure(
        MakeRequest(partition, composer));
    return ExpectTrue(result.available, "all-texture linkage is available")
        && ExpectTrue(result.allTexture, "all-texture is detected")
        && ExpectTrue(
            result.colorFillApplicability == "not_applicable",
            "color/fill gap is not applicable")
        && ExpectTrue(
            result.allTextureReason == "all_texture_partition",
            "all-texture reason is stable")
        && ExpectTrue(result.totalColorFillGapVoxels == 0U, "all-texture has no false gap");
}

bool OverlapMaskBlocks()
{
    const auto partition = MakePartition(false);
    auto composer = MakeComposer(partition);
    const std::size_t texturePixel = PixelIndex(7, 1, 1);
    composer.layers.at(0).modelFillMask.at(texturePixel) = 1U;
    const auto result = slicer_core::AdaptTextureFillPartitionClosure(
        MakeRequest(partition, composer));
    return ExpectTrue(!result.available, "overlap masks block linkage")
        && ExpectTrue(result.source == "unavailable", "blocked linkage has no exact source")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_CLOSURE_MASK_INVALID"),
            "overlap uses stable mask error");
}

bool UnassignedModelBlocks()
{
    const auto partition = MakePartition(false);
    auto composer = MakeComposer(partition);
    const std::size_t fillPixel = PixelIndex(7, 3, 3);
    composer.layers.at(0).modelFillMask.at(fillPixel) = 0U;
    const auto result = slicer_core::AdaptTextureFillPartitionClosure(
        MakeRequest(partition, composer));
    return ExpectTrue(!result.available, "unassigned model voxel blocks linkage")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_CLOSURE_MASK_INVALID"),
            "unassigned model uses stable mask error");
}

bool EmptyModelPixelFailsClosure()
{
    const auto partition = MakePartition(false);
    auto composer = MakeComposer(partition);
    const std::size_t gapPixel = PixelIndex(7, 2, 3);
    for (std::size_t channel{0U}; channel < 6U; ++channel)
    {
        composer.layers.at(0).composed.channels.at(
            ChannelIndex(gapPixel, channel)) = 255U;
    }
    const auto result = slicer_core::AdaptTextureFillPartitionClosure(
        MakeRequest(partition, composer));
    return ExpectTrue(result.available, "gap evidence remains inspectable")
        && ExpectTrue(result.status == "fail", "model-domain gap fails linkage")
        && ExpectTrue(result.totalModelDomainGapVoxels == 1U, "one model-domain gap is counted")
        && ExpectTrue(result.totalColorFillGapVoxels == 1U, "one color/fill gap is classified")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_CLOSURE_MODEL_DOMAIN_GAP"),
            "model-domain gap uses stable issue")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_CLOSURE_COLOR_FILL_GAP"),
            "color/fill gap uses stable issue");
}

bool LayerOrderMismatchBlocks()
{
    const auto partition = MakePartition(false, 2);
    auto composer = MakeComposer(partition);
    composer.layers.at(1).layerIndex = 0;
    const auto result = slicer_core::AdaptTextureFillPartitionClosure(
        MakeRequest(partition, composer));
    return ExpectTrue(!result.available, "layer order mismatch blocks")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_CLOSURE_LAYER_ORDER_INVALID"),
            "layer order uses stable issue");
}

bool LayerZMismatchBlocks()
{
    const auto partition = MakePartition(false);
    auto composer = MakeComposer(partition);
    composer.layers.at(0).zMm += 0.01;
    const auto result = slicer_core::AdaptTextureFillPartitionClosure(
        MakeRequest(partition, composer));
    return ExpectTrue(!result.available, "layer Z mismatch blocks")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_CLOSURE_LAYER_ORDER_INVALID"),
            "layer Z mismatch uses stable layer issue");
}

bool ChannelOrderMismatchBlocks()
{
    const auto partition = MakePartition(false);
    auto composer = MakeComposer(partition);
    composer.channelOrder = {"B", "G", "R", "W", "S", "V"};
    const auto result = slicer_core::AdaptTextureFillPartitionClosure(
        MakeRequest(partition, composer));
    return ExpectTrue(!result.available, "channel order mismatch blocks")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_CLOSURE_CHANNEL_ORDER_INVALID"),
            "channel order uses stable issue");
}

bool RepairAndProductionRemainDisabled()
{
    const auto partition = MakePartition(false);
    const auto composer = MakeComposer(partition);
    const auto result = slicer_core::AdaptTextureFillPartitionClosure(
        MakeRequest(partition, composer));
    return ExpectTrue(!result.repairAttempted, "closure linkage never repairs")
        && ExpectTrue(!result.productionOutputWritten, "closure linkage writes no production output")
        && ExpectTrue(
            result.productionAcceptance == "not_evaluated",
            "closure linkage has no production acceptance");
}

bool RepeatResultIsDeterministic()
{
    const auto partition = MakePartition(false, 2);
    const auto composer = MakeComposer(partition);
    const auto first = slicer_core::AdaptTextureFillPartitionClosure(
        MakeRequest(partition, composer));
    const auto second = slicer_core::AdaptTextureFillPartitionClosure(
        MakeRequest(partition, composer));
    return ExpectTrue(first.available && second.available, "repeat linkage is available")
        && ExpectTrue(first.status == second.status, "repeat status is stable")
        && ExpectTrue(
            first.totalColorFillGapVoxels == second.totalColorFillGapVoxels,
            "repeat color/fill count is stable")
        && ExpectTrue(
            first.totalModelDomainGapVoxels == second.totalModelDomainGapVoxels,
            "repeat model-domain count is stable")
        && ExpectTrue(first.layers.size() == second.layers.size(), "repeat layer count is stable")
        && ExpectTrue(
            std::abs(first.layers.at(1).zMm - second.layers.at(1).zMm) < 1.0e-12,
            "repeat true Z evidence is stable");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"ordinary_partition_exact_pass", OrdinaryPartitionExactPass},
        {"all_texture_is_not_applicable", AllTextureIsNotApplicable},
        {"overlap_mask_blocks", OverlapMaskBlocks},
        {"unassigned_model_blocks", UnassignedModelBlocks},
        {"empty_model_pixel_fails_closure", EmptyModelPixelFailsClosure},
        {"layer_order_mismatch_blocks", LayerOrderMismatchBlocks},
        {"layer_z_mismatch_blocks", LayerZMismatchBlocks},
        {"channel_order_mismatch_blocks", ChannelOrderMismatchBlocks},
        {"repair_and_production_remain_disabled", RepairAndProductionRemainDisabled},
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
    std::cout << "Texture/fill partition closure adapter unit tests complete.\n";
    return 0;
}
