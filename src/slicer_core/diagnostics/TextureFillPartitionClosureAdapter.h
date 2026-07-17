#pragma once

#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"
#include "slicer_core/diagnostics/ValidationIssue.h"
#include "slicer_core/pipeline/TextureFillPartitionDiagnosticComposer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Inputs for linking Stage 12E exact masks to the Stage 12D detector.
 */
struct TextureFillPartitionClosureAdapterRequest
{
    const GlobalTextureFillPartitionResult* partition{nullptr};
    const TextureFillPartitionDiagnosticComposerResult* composer{nullptr};
    int connectivity{8};
    int maxGapPx{1};
};

/**
 * @brief One true-Z model-domain closure result.
 */
struct TextureFillPartitionClosureLayerResult
{
    int layerIndex{0};
    double zMm{0.0};
    std::uint64_t colorFillGapVoxels{0U};
    std::uint64_t modelDomainGapVoxels{0U};
    MaterialClosureSemanticLayerResult semantic;
};

/**
 * @brief Diagnostic-only Stage 12E/12D closure linkage evidence.
 */
struct TextureFillPartitionClosureAdapterResult
{
    bool available{false};
    std::string status{"blocked"};
    std::string scope{"texture_model_fill_only"};
    std::string source{"unavailable"};
    std::string confidence{"unavailable"};
    std::string productionAcceptance{"not_evaluated"};
    std::string supportClosureStatus{"not_evaluated"};
    std::string varnishClosureStatus{"not_evaluated"};
    bool allTexture{false};
    std::string colorFillApplicability{"applicable"};
    std::string allTextureReason;
    std::uint64_t totalColorFillGapVoxels{0U};
    std::uint64_t totalModelDomainGapVoxels{0U};
    bool repairAttempted{false};
    bool productionOutputWritten{false};
    std::vector<TextureFillPartitionClosureLayerResult> layers;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Adapt exact Stage 12E texture/fill evidence to Stage 12D closure analysis.
 * @param request Validated partition, diagnostic composer, and detector options.
 * @return Model-domain closure evidence without repair or production output.
 */
TextureFillPartitionClosureAdapterResult AdaptTextureFillPartitionClosure(
    const TextureFillPartitionClosureAdapterRequest& request);

}  // namespace slicer_core
