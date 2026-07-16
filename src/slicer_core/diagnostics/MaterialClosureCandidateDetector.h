#pragma once

#include <cstdint>
#include <vector>

namespace slicer_core
{

/**
 * @brief Candidate gap counts inferred from one final RGBWSV layer.
 *
 * This structure intentionally carries candidate evidence only. It cannot be
 * used as production material-closure acceptance evidence.
 */
struct MaterialClosureCandidateLayer
{
    int layerIndex{0};
    double zMm{0.0};
    int gapPixels{0};
    int colorFillGapPixels{0};
    int modelSupportGapPixels{0};
    int colorSupportGapPixels{0};
    int externalBackgroundProtectedPixels{0};
};

/**
 * @brief Infers candidate material gaps from a final interleaved RGBWSV layer.
 * @param rgbwsvPixels Final uint8 pixels in R, G, B, W, S, V order.
 * @param widthPx Layer width in pixels.
 * @param heightPx Layer height in pixels.
 * @param layerIndex Manifest layer index.
 * @param zMm Physical Z coordinate in millimetres.
 * @param connectivity Pixel connectivity, either 4 or 8.
 * @param maxGapPx Maximum inferred empty run width.
 * @return Candidate-only gap evidence for the layer.
 */
MaterialClosureCandidateLayer DetectMaterialClosureCandidateLayer(
    const std::vector<std::uint8_t>& rgbwsvPixels,
    int widthPx,
    int heightPx,
    int layerIndex,
    double zMm,
    int connectivity,
    int maxGapPx);

}  // namespace slicer_core
