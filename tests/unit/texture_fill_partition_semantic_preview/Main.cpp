#include "slicer_core/preview/TextureFillPartitionSemanticPreview.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
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

std::size_t VoxelIndex(
    const int x,
    const int y,
    const int z,
    const int width,
    const int height)
{
    return static_cast<std::size_t>(z * width * height + y * width + x);
}

slicer_core::GlobalTextureFillPartitionResult MakePartition()
{
    slicer_core::GlobalTextureFillPartitionResult partition;
    partition.available = true;
    partition.partitionPass = true;
    partition.status = "diagnostic";
    partition.grid.width = 4;
    partition.grid.height = 3;
    partition.grid.depth = 2;
    partition.grid.originXMm = 1.0;
    partition.grid.originYMm = 2.0;
    partition.grid.originZMm = 3.0;
    partition.grid.spacingXMm = 0.04;
    partition.grid.spacingYMm = 0.05;
    partition.grid.spacingZMm = 0.10;
    partition.widthMetrics.allTexture = false;

    const std::size_t voxelCount{24U};
    partition.modelMask.grid = partition.grid;
    partition.textureSurfaceMask.grid = partition.grid;
    partition.modelFillMask.grid = partition.grid;
    partition.modelMask.values.assign(voxelCount, 0U);
    partition.textureSurfaceMask.values.assign(voxelCount, 0U);
    partition.modelFillMask.values.assign(voxelCount, 0U);

    const std::size_t texture =
        VoxelIndex(1, 1, 0, 4, 3);
    const std::size_t fill =
        VoxelIndex(2, 1, 0, 4, 3);
    partition.modelMask.values.at(texture) = 1U;
    partition.textureSurfaceMask.values.at(texture) = 1U;
    partition.modelMask.values.at(fill) = 1U;
    partition.modelFillMask.values.at(fill) = 1U;
    return partition;
}

slicer_core::RgbwsvLayerBuffer MakeProductionLayer()
{
    slicer_core::RgbwsvLayerBuffer layer;
    layer.sourceIdentity = "package-layer-17";
    layer.layerIndex = 17;
    layer.zMm = 3.05;
    layer.width = 4U;
    layer.height = 3U;
    layer.dpiX = 635;
    layer.dpiY = 508;
    layer.originxmm = 1.0;
    layer.originymm = 2.0;
    layer.originzmm = 3.0;
    layer.pixelsizexmm = 0.04;
    layer.pixelsizeymm = 0.05;
    layer.layerthicknessmm = 0.10;
    layer.pixels.assign(4U * 3U * kChannelCount, 255U);

    const std::size_t supportPixel{0U};
    const std::size_t whitePixel{10U};
    const std::size_t varnishPixel{11U};
    layer.pixels.at(whitePixel * kChannelCount + 3U) = 0U;
    layer.pixels.at(supportPixel * kChannelCount + 4U) = 0U;
    layer.pixels.at(varnishPixel * kChannelCount + 5U) = 127U;
    return layer;
}

slicer_core::TextureFillPartitionSemanticPreviewClosureEvidence
MakeClosureEvidence()
{
    slicer_core::
        TextureFillPartitionSemanticPreviewClosureEvidence
            evidence;
    evidence.available = true;
    evidence.exact = true;
    slicer_core::
        TextureFillPartitionSemanticPreviewClosureLayer
            layer;
    layer.layerindex = 17;
    layer.zmm = 3.05;
    layer.closurepass = true;
    evidence.layers.push_back(layer);
    return evidence;
}

bool MapsAnisotropicProductionCentersToTheExactZCell()
{
    const auto partition = MakePartition();
    const auto production = MakeProductionLayer();
    const auto closure = MakeClosureEvidence();
    slicer_core::TextureFillPartitionSemanticPreviewRequest request;
    request.partition = &partition;
    request.productionlayer = &production;
    request.closureevidence = &closure;

    const auto result =
        slicer_core::BuildTextureFillPartitionSemanticPreview(
            request);

    return ExpectTrue(result.available, "semantic preview is available")
        && ExpectTrue(result.status == "diagnostic", "status remains diagnostic")
        && ExpectTrue(result.layerindex == 17, "real production layer index is retained")
        && ExpectTrue(std::abs(result.zmm - 3.05) < 1.0e-9, "real zMm is retained")
        && ExpectTrue(result.width == 4U && result.height == 3U, "production dimensions are retained")
        && ExpectTrue(result.texturesurfacemask.at(5U) == 1U, "texture cell uses independent XY pitch")
        && ExpectTrue(result.modelfillmask.at(6U) == 1U, "fill cell uses independent XY pitch")
        && ExpectTrue(result.texturesurfacepixels == 1U, "texture counter is exact")
        && ExpectTrue(result.modelfillpixels == 1U, "fill counter is exact")
        && ExpectTrue(result.whitemask.at(10U) == 1U, "white comes from the same TIFF layer")
        && ExpectTrue(result.supportmask.at(0U) == 1U, "support comes from the same TIFF layer")
        && ExpectTrue(result.varnishmask.at(11U) == 1U, "varnish comes from the same TIFF layer")
        && ExpectTrue(result.whitepixels == 1U && result.supportpixels == 1U && result.varnishpixels == 1U, "production material counters are exact")
        && ExpectTrue(result.fullclosurelinkageevaluated, "production full-closure linkage is evaluated")
        && ExpectTrue(result.fullclosurepass, "exact production closure passes")
        && ExpectTrue(result.fullclosuregappixels == 0U, "exact production closure has no gaps");
}

bool OutsideZProducesAnExplicitEmptyDiagnosticLayer()
{
    const auto partition = MakePartition();
    auto production = MakeProductionLayer();
    production.zMm = 4.50;
    auto closure = MakeClosureEvidence();
    closure.layers.front().zmm = production.zMm;
    slicer_core::TextureFillPartitionSemanticPreviewRequest request;
    request.partition = &partition;
    request.productionlayer = &production;
    request.closureevidence = &closure;

    const auto result =
        slicer_core::BuildTextureFillPartitionSemanticPreview(
            request);

    return ExpectTrue(result.available, "outside-Z production layer remains displayable")
        && ExpectTrue(result.status == "diagnostic_empty", "outside-Z layer is explicit")
        && ExpectTrue(result.texturesurfacepixels == 0U, "no cross-layer texture fallback")
        && ExpectTrue(result.modelfillpixels == 0U, "no cross-layer fill fallback")
        && ExpectTrue(result.supportpixels == 1U, "same-layer production support remains visible");
}

bool MissingOrInvalidEvidenceFailsClosed()
{
    const auto production = MakeProductionLayer();
    const auto closure = MakeClosureEvidence();
    slicer_core::TextureFillPartitionSemanticPreviewRequest missing;
    missing.productionlayer = &production;
    missing.closureevidence = &closure;
    const auto missingResult =
        slicer_core::BuildTextureFillPartitionSemanticPreview(
            missing);

    auto invalidPartition = MakePartition();
    invalidPartition.partitionPass = false;
    slicer_core::TextureFillPartitionSemanticPreviewRequest invalid;
    invalid.partition = &invalidPartition;
    invalid.productionlayer = &production;
    invalid.closureevidence = &closure;
    const auto invalidResult =
        slicer_core::BuildTextureFillPartitionSemanticPreview(
            invalid);

    return ExpectTrue(!missingResult.available, "missing evidence is unavailable")
        && ExpectTrue(missingResult.errorcode == "SEMANTIC_PREVIEW_EVIDENCE_MISSING", "missing evidence has stable error")
        && ExpectTrue(!invalidResult.available, "invalid partition is unavailable")
        && ExpectTrue(invalidResult.errorcode == "SEMANTIC_PREVIEW_PARTITION_INVALID", "invalid partition has stable error");
}

bool InvalidProductionMetadataFailsClosed()
{
    const auto partition = MakePartition();
    auto production = MakeProductionLayer();
    const auto closure = MakeClosureEvidence();
    production.pixelsizexmm = 0.0;
    slicer_core::TextureFillPartitionSemanticPreviewRequest request;
    request.partition = &partition;
    request.productionlayer = &production;
    request.closureevidence = &closure;

    const auto result =
        slicer_core::BuildTextureFillPartitionSemanticPreview(
            request);

    return ExpectTrue(!result.available, "invalid production metadata is unavailable")
        && ExpectTrue(result.errorcode == "SEMANTIC_PREVIEW_PRODUCTION_GRID_INVALID", "invalid grid has stable error");
}

bool ClosureEvidenceFailsClosedWhenMissingCandidateOrCrossLayer()
{
    const auto partition = MakePartition();
    const auto production = MakeProductionLayer();

    slicer_core::TextureFillPartitionSemanticPreviewRequest missing;
    missing.partition = &partition;
    missing.productionlayer = &production;
    const auto missingResult =
        slicer_core::BuildTextureFillPartitionSemanticPreview(
            missing);

    auto candidate = MakeClosureEvidence();
    candidate.exact = false;
    slicer_core::TextureFillPartitionSemanticPreviewRequest candidateRequest;
    candidateRequest.partition = &partition;
    candidateRequest.productionlayer = &production;
    candidateRequest.closureevidence = &candidate;
    const auto candidateResult =
        slicer_core::BuildTextureFillPartitionSemanticPreview(
            candidateRequest);

    auto crossLayer = MakeClosureEvidence();
    crossLayer.layers.front().layerindex = 18;
    slicer_core::TextureFillPartitionSemanticPreviewRequest crossLayerRequest;
    crossLayerRequest.partition = &partition;
    crossLayerRequest.productionlayer = &production;
    crossLayerRequest.closureevidence = &crossLayer;
    const auto crossLayerResult =
        slicer_core::BuildTextureFillPartitionSemanticPreview(
            crossLayerRequest);

    auto staleZ = MakeClosureEvidence();
    staleZ.layers.front().zmm += 0.05;
    slicer_core::TextureFillPartitionSemanticPreviewRequest staleZRequest;
    staleZRequest.partition = &partition;
    staleZRequest.productionlayer = &production;
    staleZRequest.closureevidence = &staleZ;
    const auto staleZResult =
        slicer_core::BuildTextureFillPartitionSemanticPreview(
            staleZRequest);

    return ExpectTrue(
               !missingResult.available
                   && missingResult.errorcode
                       == "SEMANTIC_PREVIEW_CLOSURE_EVIDENCE_MISSING",
               "missing closure evidence fails closed")
        && ExpectTrue(
            !candidateResult.available
                && candidateResult.errorcode
                    == "SEMANTIC_PREVIEW_CLOSURE_NOT_EXACT",
            "candidate closure evidence fails closed")
        && ExpectTrue(
            !crossLayerResult.available
                && crossLayerResult.errorcode
                    == "SEMANTIC_PREVIEW_CLOSURE_LAYER_MISSING",
            "cross-layer closure fallback is rejected")
        && ExpectTrue(
            !staleZResult.available
                && staleZResult.errorcode
                    == "SEMANTIC_PREVIEW_CLOSURE_IDENTITY_MISMATCH",
            "stale closure z identity is rejected");
}

bool FailedExactClosureRemainsVisibleAndExplicit()
{
    const auto partition = MakePartition();
    const auto production = MakeProductionLayer();
    auto closure = MakeClosureEvidence();
    closure.layers.front().closurepass = false;
    closure.layers.front().gappixels = 7U;

    slicer_core::TextureFillPartitionSemanticPreviewRequest request;
    request.partition = &partition;
    request.productionlayer = &production;
    request.closureevidence = &closure;
    const auto result =
        slicer_core::BuildTextureFillPartitionSemanticPreview(
            request);

    return ExpectTrue(result.available, "failed exact closure remains inspectable")
        && ExpectTrue(result.fullclosurelinkageevaluated, "failed closure is linked")
        && ExpectTrue(!result.fullclosurepass, "failed closure is not promoted to pass")
        && ExpectTrue(result.fullclosuregappixels == 7U, "failed closure gap count is retained");
}

}  // namespace

int main()
{
    const bool ok =
        MapsAnisotropicProductionCentersToTheExactZCell()
        && OutsideZProducesAnExplicitEmptyDiagnosticLayer()
        && MissingOrInvalidEvidenceFailsClosed()
        && InvalidProductionMetadataFailsClosed()
        && ClosureEvidenceFailsClosedWhenMissingCandidateOrCrossLayer()
        && FailedExactClosureRemainsVisibleAndExplicit();
    if (!ok)
    {
        return 1;
    }

    std::cout
        << "texture_fill_partition_semantic_preview_unit_tests: PASS\n";
    return 0;
}
