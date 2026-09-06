#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace slicer_core
{

/**
 * @brief Immutable semantic evidence for one composed RGBWSV layer.
 *
 * All masks use one byte per pixel, where zero is false and non-zero is true.
 * The detector reads these masks but never mutates production layer data.
 */
struct MaterialClosureSemanticLayerInput
{
    int layerIndex{0};
    double zMm{0.0};
    int widthPx{0};
    int heightPx{0};
    std::vector<std::uint8_t> textureSurfaceMask;
    std::vector<std::uint8_t> modelFillMask;
    std::vector<std::uint8_t> modelMaterialMask;
    std::vector<std::uint8_t> supportFillMask;
    std::vector<std::uint8_t> internalVoidSupportMask;
    std::vector<std::uint8_t> surfaceVarnishMask;
    std::vector<std::uint8_t> outerVarnishShellMask;
    std::vector<std::uint8_t> modelEnvelopeMask;
    std::vector<std::uint8_t> supportRequiredMask;
    std::vector<std::uint8_t> expectedOccupiedDomainMask;
    std::vector<std::uint8_t> layerEmptyMask;
};

/** @brief Borrowed semantic evidence used by the allocation-free detector core. */
struct MaterialClosureSemanticLayerInputView
{
    int layerIndex{0};
    double zMm{0.0};
    int widthPx{0};
    int heightPx{0};
    std::span<const std::uint8_t> textureSurfaceMask;
    std::span<const std::uint8_t> modelFillMask;
    std::span<const std::uint8_t> modelMaterialMask;
    std::span<const std::uint8_t> supportFillMask;
    std::span<const std::uint8_t> internalVoidSupportMask;
    std::span<const std::uint8_t> surfaceVarnishMask;
    std::span<const std::uint8_t> outerVarnishShellMask;
    std::span<const std::uint8_t> modelEnvelopeMask;
    std::span<const std::uint8_t> supportRequiredMask;
    std::span<const std::uint8_t> expectedOccupiedDomainMask;
    std::span<const std::uint8_t> layerEmptyMask;
};

/**
 * @brief Exact material-closure gap counts for one semantic layer.
 */
struct MaterialClosureSemanticLayerResult
{
    int layerIndex{0};
    double zMm{0.0};
    int gapPixels{0};
    int colorFillGapPixels{0};
    int modelSupportGapPixels{0};
    int colorSupportGapPixels{0};
    int internalVoidGapPixels{0};
    int varnishSupportGapPixels{0};
    int externalBackgroundProtectedPixels{0};
    bool repairAttempted{false};
    int repairedPixels{0};
    int repairedColorFillPixels{0};
    int repairedModelSupportPixels{0};
    int repairedInternalVoidPixels{0};
    int repairedVarnishSupportPixels{0};
    int remainingGapPixels{0};
    int remainingColorFillGapPixels{0};
    int remainingModelSupportGapPixels{0};
    int remainingColorSupportGapPixels{0};
    int remainingInternalVoidGapPixels{0};
    int remainingVarnishSupportGapPixels{0};
    int repairRejectedTooWidePixels{0};
};

/**
 * @brief Exact semantic masks and summary used to plan closure repair.
 */
struct MaterialClosureSemanticLayerAnalysis
{
    int widthPx{0};
    int heightPx{0};
    MaterialClosureSemanticLayerResult summary;
    std::vector<std::uint8_t> externalBackgroundMask;
    std::vector<std::uint8_t> candidateGapMask;
    std::vector<std::uint8_t> colorFillGapMask;
    std::vector<std::uint8_t> modelSupportGapMask;
    std::vector<std::uint8_t> colorSupportGapMask;
    std::vector<std::uint8_t> internalVoidGapMask;
    std::vector<std::uint8_t> varnishSupportGapMask;
};

/** @brief Borrowed analysis masks valid until the workspace is reused. */
struct MaterialClosureSemanticLayerAnalysisView
{
    int widthPx{0};
    int heightPx{0};
    MaterialClosureSemanticLayerResult summary;
    std::span<const std::uint8_t> externalBackgroundMask;
    std::span<const std::uint8_t> candidateGapMask;
    std::span<const std::uint8_t> colorFillGapMask;
    std::span<const std::uint8_t> modelSupportGapMask;
    std::span<const std::uint8_t> colorSupportGapMask;
    std::span<const std::uint8_t> internalVoidGapMask;
    std::span<const std::uint8_t> varnishSupportGapMask;
};

/** @brief Caller-owned storage reused by semantic detector calls. */
struct MaterialClosureSemanticWorkspace
{
    void Prepare(std::size_t pixelCount);

    std::vector<std::uint8_t> externalBackgroundMask;
    std::vector<std::uint8_t> candidateGapMask;
    std::vector<std::uint8_t> colorFillGapMask;
    std::vector<std::uint8_t> modelSupportGapMask;
    std::vector<std::uint8_t> colorSupportGapMask;
    std::vector<std::uint8_t> internalVoidGapMask;
    std::vector<std::uint8_t> varnishSupportGapMask;
    std::vector<std::size_t> traversalQueue;
};

/** @brief Create a borrowed view over the existing owning semantic DTO. */
[[nodiscard]] MaterialClosureSemanticLayerInputView ViewMaterialClosureSemanticLayerInput(
    const MaterialClosureSemanticLayerInput& input) noexcept;

/**
 * @brief Analyze one layer using caller-prepared reusable storage.
 * @throws std::invalid_argument When input or workspace dimensions are invalid.
 */
MaterialClosureSemanticLayerAnalysisView AnalyzeMaterialClosureSemanticLayer(
    const MaterialClosureSemanticLayerInputView& input,
    int connectivity,
    int maxGapPx,
    MaterialClosureSemanticWorkspace& workspace);

/** @brief Detect one layer using caller-prepared reusable storage. */
MaterialClosureSemanticLayerResult DetectMaterialClosureSemanticLayer(
    const MaterialClosureSemanticLayerInputView& input,
    int connectivity,
    int maxGapPx,
    MaterialClosureSemanticWorkspace& workspace);

/**
 * @brief Analyzes exact material gaps and retains masks for a later repair plan.
 * @param input Semantic masks and final empty mask for one layer.
 * @param connectivity Pixel connectivity, either 4 or 8.
 * @param maxGapPx Maximum neighborhood radius used for gap classification.
 * @return Exact per-layer gap analysis and immutable repair evidence.
 * @throws std::invalid_argument When dimensions, connectivity, radius, or mask sizes are invalid.
 */
MaterialClosureSemanticLayerAnalysis AnalyzeMaterialClosureSemanticLayer(
    const MaterialClosureSemanticLayerInput& input,
    int connectivity,
    int maxGapPx);

/**
 * @brief Detects exact material gaps from composer semantic masks.
 * @param input Semantic masks and final empty mask for one layer.
 * @param connectivity Pixel connectivity, either 4 or 8.
 * @param maxGapPx Maximum neighborhood radius used for gap classification.
 * @return Exact per-layer gap evidence.
 * @throws std::invalid_argument When dimensions, connectivity, radius, or mask sizes are invalid.
 */
MaterialClosureSemanticLayerResult DetectMaterialClosureSemanticLayer(
    const MaterialClosureSemanticLayerInput& input,
    int connectivity,
    int maxGapPx);

}  // namespace slicer_core
