#pragma once

#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"
#include "slicer_core/diagnostics/ValidationIssue.h"
#include "slicer_core/raster/TextureFillPartitionRasterMapper.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Non-model material evidence aligned to one Stage 12E raster layer.
 */
struct TextureFillPartitionFullClosureLayerEvidence
{
    int layerIndex{0};
    double zMm{0.0};
    int widthPx{0};
    int heightPx{0};
    std::vector<std::uint8_t> supportFillMask;
    std::vector<std::uint8_t> internalVoidSupportMask;
    std::vector<std::uint8_t> surfaceVarnishMask;
    std::vector<std::uint8_t> outerVarnishShellMask;
    std::vector<std::uint8_t> modelEnvelopeMask;
    std::vector<std::uint8_t> supportRequiredMask;
    std::array<std::string, 6> channelOrder{"R", "G", "B", "W", "S", "V"};
    std::vector<std::uint8_t> channels;
};

/**
 * @brief Inputs for building an exact full-material sidecar and invoking Stage 12D.
 */
struct TextureFillPartitionFullClosureAdapterRequest
{
    const TextureFillPartitionRasterMappingResult* rasterMapping{nullptr};
    const std::vector<TextureFillPartitionFullClosureLayerEvidence>* layers{nullptr};
    int connectivity{8};
    int maxGapPx{1};
};

/**
 * @brief Exact full-material closure evidence for one true raster layer.
 */
struct TextureFillPartitionFullClosureLayerResult
{
    int layerIndex{0};
    double zMm{0.0};
    std::uint64_t expectedDomainGapPixels{0U};
    std::uint64_t modelDomainGapPixels{0U};
    std::uint64_t supportRequiredGapPixels{0U};
    std::uint64_t outerVarnishGapPixels{0U};
    std::uint64_t unexpectedOccupiedPixels{0U};
    std::uint64_t supportChannelMismatchPixels{0U};
    std::uint64_t varnishChannelMismatchPixels{0U};
    bool closurePass{false};
    MaterialClosureSemanticLayerInput sidecar;
    MaterialClosureSemanticLayerResult semantic;
};

/**
 * @brief Diagnostic full-domain closure result without repair or production output.
 */
struct TextureFillPartitionFullClosureAdapterResult
{
    bool available{false};
    bool allTexture{false};
    bool fullClosurePass{false};
    bool repairAttempted{false};
    bool productionOutputWritten{false};
    std::string status{"blocked"};
    std::string scope{"full_material_domain"};
    std::string source{"unavailable"};
    std::string confidence{"unavailable"};
    std::string productionAcceptance{"not_evaluated"};
    std::string colorFillApplicability{"applicable"};
    std::string allTextureReason;
    std::string modelClosureStatus{"not_evaluated"};
    std::string supportClosureStatus{"not_evaluated"};
    std::string varnishClosureStatus{"not_evaluated"};
    std::uint64_t totalExpectedDomainGapPixels{0U};
    std::uint64_t totalModelDomainGapPixels{0U};
    std::uint64_t totalSupportRequiredGapPixels{0U};
    std::uint64_t totalOuterVarnishGapPixels{0U};
    std::uint64_t totalUnexpectedOccupiedPixels{0U};
    std::uint64_t totalSupportChannelMismatchPixels{0U};
    std::uint64_t totalVarnishChannelMismatchPixels{0U};
    std::uint64_t totalSemanticChannelMismatchPixels{0U};
    std::uint64_t totalColorFillGapPixels{0U};
    std::uint64_t totalModelSupportGapPixels{0U};
    std::uint64_t totalColorSupportGapPixels{0U};
    std::uint64_t totalInternalVoidGapPixels{0U};
    std::uint64_t totalVarnishSupportGapPixels{0U};
    double analysisMs{0.0};
    std::vector<TextureFillPartitionFullClosureLayerResult> layers;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Build exact material sidecars and run Stage 12D full-domain closure analysis.
 * @param request Raster ownership, support/varnish evidence, RGBWSV bytes, and detector options.
 * @return Full-domain diagnostic evidence; no repair, TIFF, manifest, or package is written.
 */
TextureFillPartitionFullClosureAdapterResult AdaptTextureFillPartitionFullClosure(
    const TextureFillPartitionFullClosureAdapterRequest& request);

}  // namespace slicer_core
