#include "slicer_core/pipeline/TextureFillPartitionDiagnosticComposer.h"

#include <array>
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

std::size_t ChannelIndex(
    const std::size_t pixelIndex,
    const slicer_core::MaterialChannelOffset channel)
{
    return pixelIndex * 6U + static_cast<std::size_t>(channel);
}

slicer_core::GlobalTextureFillPartitionResult MakePartition()
{
    slicer_core::GlobalTextureFillPartitionResult partition;
    partition.available = true;
    partition.partitionPass = true;
    partition.status = "diagnostic";
    partition.grid.width = 2;
    partition.grid.height = 1;
    partition.grid.depth = 2;
    partition.grid.originZMm = 0.0;
    partition.grid.spacingXMm = 0.05;
    partition.grid.spacingYMm = 0.05;
    partition.grid.spacingZMm = 0.01;
    partition.modelMask.grid = partition.grid;
    partition.textureSurfaceMask.grid = partition.grid;
    partition.modelFillMask.grid = partition.grid;
    partition.modelMask.values = {1U, 1U, 1U, 1U};
    partition.textureSurfaceMask.values = {1U, 0U, 0U, 1U};
    partition.modelFillMask.values = {0U, 1U, 1U, 0U};
    return partition;
}

slicer_core::TextureFillPartitionTextureTransferResult MakeTransfer()
{
    slicer_core::TextureFillPartitionTextureTransferResult transfer;
    transfer.available = true;
    transfer.status = "diagnostic";
    transfer.voxelRgb = {
        std::array<std::uint8_t, 3>{10, 20, 30},
        std::array<std::uint8_t, 3>{255, 255, 255},
        std::array<std::uint8_t, 3>{255, 255, 255},
        std::array<std::uint8_t, 3>{40, 50, 60},
    };
    transfer.colorSources = {
        slicer_core::TextureFillColorSource::Texture,
        slicer_core::TextureFillColorSource::NotColored,
        slicer_core::TextureFillColorSource::NotColored,
        slicer_core::TextureFillColorSource::MaterialDiffuse,
    };
    transfer.stats.reusedReferenceCount = 2U;
    return transfer;
}

slicer_core::TextureFillPartitionDiagnosticComposerRequest MakeRequest(
    const slicer_core::GlobalTextureFillPartitionResult& partition,
    const slicer_core::TextureFillPartitionTextureTransferResult& transfer,
    const std::string& material)
{
    slicer_core::TextureFillPartitionDiagnosticComposerRequest request;
    request.partition = &partition;
    request.transfer = &transfer;
    request.modelFillMaterial = material;
    request.modelFillValue = 0U;
    request.modelFillRgb = {70, 80, 90};
    return request;
}

bool WhiteFillMapsOnlyToW()
{
    const auto partition = MakePartition();
    const auto transfer = MakeTransfer();
    const auto result = slicer_core::ComposeTextureFillPartitionDiagnostic(
        MakeRequest(partition, transfer, "white"));
    if (!ExpectTrue(result.available, "white diagnostic composer is available"))
    {
        return false;
    }
    const auto& layer0 = result.layers.at(0).composed.channels;
    return ExpectTrue(result.layers.size() == 2U, "two true Z layers emitted")
        && ExpectTrue(result.layers.at(1).layerIndex == 1, "ascending layer index retained")
        && ExpectTrue(result.layers.at(1).zMm == 0.015, "cell-center Z retained")
        && ExpectTrue(layer0.at(ChannelIndex(0U, slicer_core::MaterialChannelOffset::R)) == 10U, "texture R mapped")
        && ExpectTrue(layer0.at(ChannelIndex(1U, slicer_core::MaterialChannelOffset::R)) == 255U, "white fill leaves RGB empty")
        && ExpectTrue(layer0.at(ChannelIndex(1U, slicer_core::MaterialChannelOffset::W)) == 0U, "white fill maps W")
        && ExpectTrue(layer0.at(ChannelIndex(1U, slicer_core::MaterialChannelOffset::S)) == 255U, "S remains empty")
        && ExpectTrue(result.stats.modelFillWhiteVoxels == 2U, "white fill total counted")
        && ExpectTrue(result.stats.supportPrintVoxels == 0U, "no support is composed");
}

bool VarnishFillMapsOnlyToV()
{
    const auto partition = MakePartition();
    const auto transfer = MakeTransfer();
    const auto result = slicer_core::ComposeTextureFillPartitionDiagnostic(
        MakeRequest(partition, transfer, "varnish"));
    const auto& layer0 = result.layers.at(0).composed.channels;
    return ExpectTrue(result.available, "varnish diagnostic composer is available")
        && ExpectTrue(layer0.at(ChannelIndex(1U, slicer_core::MaterialChannelOffset::V)) == 0U, "varnish fill maps V")
        && ExpectTrue(layer0.at(ChannelIndex(1U, slicer_core::MaterialChannelOffset::W)) == 255U, "varnish fill leaves W empty")
        && ExpectTrue(result.stats.modelFillVarnishVoxels == 2U, "varnish fill total counted");
}

bool RgbFillMapsOnlyToRgb()
{
    const auto partition = MakePartition();
    const auto transfer = MakeTransfer();
    const auto result = slicer_core::ComposeTextureFillPartitionDiagnostic(
        MakeRequest(partition, transfer, "rgb"));
    const auto& layer0 = result.layers.at(0).composed.channels;
    return ExpectTrue(result.available, "RGB diagnostic composer is available")
        && ExpectTrue(layer0.at(ChannelIndex(1U, slicer_core::MaterialChannelOffset::R)) == 70U, "RGB fill maps R")
        && ExpectTrue(layer0.at(ChannelIndex(1U, slicer_core::MaterialChannelOffset::G)) == 80U, "RGB fill maps G")
        && ExpectTrue(layer0.at(ChannelIndex(1U, slicer_core::MaterialChannelOffset::B)) == 90U, "RGB fill maps B")
        && ExpectTrue(layer0.at(ChannelIndex(1U, slicer_core::MaterialChannelOffset::W)) == 255U, "RGB fill leaves W empty")
        && ExpectTrue(result.stats.modelFillRgbVoxels == 2U, "RGB fill total counted");
}

bool InvalidPartitionBlocks()
{
    auto partition = MakePartition();
    const auto transfer = MakeTransfer();
    partition.textureSurfaceMask.values.at(1) = 1U;
    const auto result = slicer_core::ComposeTextureFillPartitionDiagnostic(
        MakeRequest(partition, transfer, "white"));
    return ExpectTrue(!result.available, "overlap partition is unavailable")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_DIAGNOSTIC_COMPOSER_PARTITION_INVALID"),
            "overlap partition uses stable issue");
}

bool ColoredOutsideTextureMaskBlocks()
{
    const auto partition = MakePartition();
    auto transfer = MakeTransfer();
    transfer.colorSources.at(1) = slicer_core::TextureFillColorSource::Fallback;
    transfer.voxelRgb.at(1) = {1, 2, 3};
    transfer.stats.outsideColoredCount = 1U;
    const auto result = slicer_core::ComposeTextureFillPartitionDiagnostic(
        MakeRequest(partition, transfer, "white"));
    return ExpectTrue(!result.available, "outside color blocks composer")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_DIAGNOSTIC_COMPOSER_INPUT_INVALID"),
            "outside color uses stable input issue");
}

bool UnsupportedFillMaterialBlocks()
{
    const auto partition = MakePartition();
    const auto transfer = MakeTransfer();
    const auto result = slicer_core::ComposeTextureFillPartitionDiagnostic(
        MakeRequest(partition, transfer, "support"));
    return ExpectTrue(!result.available, "unsupported fill material blocks")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_DIAGNOSTIC_COMPOSER_INPUT_INVALID"),
            "unsupported fill material uses stable input issue");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"white_fill_maps_only_to_w", WhiteFillMapsOnlyToW},
        {"varnish_fill_maps_only_to_v", VarnishFillMapsOnlyToV},
        {"rgb_fill_maps_only_to_rgb", RgbFillMapsOnlyToRgb},
        {"invalid_partition_blocks", InvalidPartitionBlocks},
        {"colored_outside_texture_mask_blocks", ColoredOutsideTextureMaskBlocks},
        {"unsupported_fill_material_blocks", UnsupportedFillMaterialBlocks},
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
    std::cout << "Texture/fill partition diagnostic composer unit tests complete.\n";
    return 0;
}
