#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace slicer_core
{
namespace
{

struct Direction
{
    int dx{0};
    int dy{0};
};

constexpr std::array<Direction, 4> directions4{{
    {1, 0},
    {-1, 0},
    {0, 1},
    {0, -1},
}};

constexpr std::array<Direction, 8> directions8{{
    {1, 0},
    {-1, 0},
    {0, 1},
    {0, -1},
    {1, 1},
    {-1, -1},
    {1, -1},
    {-1, 1},
}};

std::size_t PixelIndex(const int widthPx, const int x, const int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(widthPx)
        + static_cast<std::size_t>(x);
}

bool IsInside(const int widthPx, const int heightPx, const int x, const int y)
{
    return x >= 0 && x < widthPx && y >= 0 && y < heightPx;
}

void ValidateMask(
    const std::vector<std::uint8_t>& mask,
    const std::size_t pixelCount,
    const std::string_view name)
{
    if (mask.size() != pixelCount)
    {
        throw std::invalid_argument(
            "material closure semantic mask size mismatch: " + std::string{name});
    }
}

void ValidateInput(
    const MaterialClosureSemanticLayerInput& input,
    const int connectivity,
    const int maxGapPx)
{
    if (input.widthPx <= 0 || input.heightPx <= 0)
    {
        throw std::invalid_argument("material closure semantic dimensions must be positive");
    }
    if (input.layerIndex < 0)
    {
        throw std::invalid_argument("material closure semantic layer index must not be negative");
    }
    if (connectivity != 4 && connectivity != 8)
    {
        throw std::invalid_argument("material closure semantic connectivity must be 4 or 8");
    }
    if (maxGapPx <= 0)
    {
        throw std::invalid_argument("material closure semantic max gap must be positive");
    }

    const std::size_t pixelCount = static_cast<std::size_t>(input.widthPx)
        * static_cast<std::size_t>(input.heightPx);
    ValidateMask(input.textureSurfaceMask, pixelCount, "textureSurfaceMask");
    ValidateMask(input.modelFillMask, pixelCount, "modelFillMask");
    ValidateMask(input.modelMaterialMask, pixelCount, "modelMaterialMask");
    ValidateMask(input.supportFillMask, pixelCount, "supportFillMask");
    ValidateMask(input.internalVoidSupportMask, pixelCount, "internalVoidSupportMask");
    ValidateMask(input.surfaceVarnishMask, pixelCount, "surfaceVarnishMask");
    ValidateMask(input.outerVarnishShellMask, pixelCount, "outerVarnishShellMask");
    ValidateMask(input.modelEnvelopeMask, pixelCount, "modelEnvelopeMask");
    ValidateMask(input.supportRequiredMask, pixelCount, "supportRequiredMask");
    ValidateMask(input.expectedOccupiedDomainMask, pixelCount, "expectedOccupiedDomainMask");
    ValidateMask(input.layerEmptyMask, pixelCount, "layerEmptyMask");
}

template <std::size_t DirectionCount>
std::vector<std::uint8_t> BuildExternalBackgroundMask(
    const std::vector<std::uint8_t>& layerEmptyMask,
    const int widthPx,
    const int heightPx,
    const std::array<Direction, DirectionCount>& directions,
    int& protectedPixels)
{
    std::vector<std::uint8_t> external(layerEmptyMask.size(), 0U);
    std::deque<std::size_t> pending;

    const auto enqueue = [&](const int x, const int y)
    {
        const std::size_t index = PixelIndex(widthPx, x, y);
        if (layerEmptyMask.at(index) != 0U && external.at(index) == 0U)
        {
            external.at(index) = 1U;
            pending.push_back(index);
        }
    };

    for (int x{0}; x < widthPx; ++x)
    {
        enqueue(x, 0);
        enqueue(x, heightPx - 1);
    }
    for (int y{0}; y < heightPx; ++y)
    {
        enqueue(0, y);
        enqueue(widthPx - 1, y);
    }

    protectedPixels = 0;
    while (!pending.empty())
    {
        const std::size_t index = pending.front();
        pending.pop_front();
        ++protectedPixels;

        const int x = static_cast<int>(index % static_cast<std::size_t>(widthPx));
        const int y = static_cast<int>(index / static_cast<std::size_t>(widthPx));
        for (const Direction& direction : directions)
        {
            const int nextX = x + direction.dx;
            const int nextY = y + direction.dy;
            if (IsInside(widthPx, heightPx, nextX, nextY))
            {
                enqueue(nextX, nextY);
            }
        }
    }
    return external;
}

bool HasMaskNeighbor(
    const std::vector<std::uint8_t>& mask,
    const int widthPx,
    const int heightPx,
    const int centerX,
    const int centerY,
    const int connectivity,
    const int maxGapPx)
{
    for (int dy{-maxGapPx}; dy <= maxGapPx; ++dy)
    {
        for (int dx{-maxGapPx}; dx <= maxGapPx; ++dx)
        {
            if (dx == 0 && dy == 0)
            {
                continue;
            }
            if (connectivity == 4 && (std::abs(dx) + std::abs(dy) > maxGapPx))
            {
                continue;
            }

            const int x = centerX + dx;
            const int y = centerY + dy;
            if (IsInside(widthPx, heightPx, x, y)
                && mask.at(PixelIndex(widthPx, x, y)) != 0U)
            {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

MaterialClosureSemanticLayerAnalysis AnalyzeMaterialClosureSemanticLayer(
    const MaterialClosureSemanticLayerInput& input,
    const int connectivity,
    const int maxGapPx)
{
    ValidateInput(input, connectivity, maxGapPx);

    MaterialClosureSemanticLayerAnalysis analysis;
    analysis.widthPx = input.widthPx;
    analysis.heightPx = input.heightPx;
    analysis.summary.layerIndex = input.layerIndex;
    analysis.summary.zMm = input.zMm;
    const std::size_t pixelCount = static_cast<std::size_t>(input.widthPx)
        * static_cast<std::size_t>(input.heightPx);
    analysis.candidateGapMask.assign(pixelCount, 0U);
    analysis.colorFillGapMask.assign(pixelCount, 0U);
    analysis.modelSupportGapMask.assign(pixelCount, 0U);
    analysis.colorSupportGapMask.assign(pixelCount, 0U);
    analysis.internalVoidGapMask.assign(pixelCount, 0U);
    analysis.varnishSupportGapMask.assign(pixelCount, 0U);

    if (connectivity == 8)
    {
        analysis.externalBackgroundMask = BuildExternalBackgroundMask(
            input.layerEmptyMask,
            input.widthPx,
            input.heightPx,
            directions8,
            analysis.summary.externalBackgroundProtectedPixels);
    }
    else
    {
        analysis.externalBackgroundMask = BuildExternalBackgroundMask(
            input.layerEmptyMask,
            input.widthPx,
            input.heightPx,
            directions4,
            analysis.summary.externalBackgroundProtectedPixels);
    }

    for (int y{0}; y < input.heightPx; ++y)
    {
        for (int x{0}; x < input.widthPx; ++x)
        {
            const std::size_t index = PixelIndex(input.widthPx, x, y);
            const bool candidateGap = input.layerEmptyMask.at(index) != 0U
                && input.expectedOccupiedDomainMask.at(index) != 0U
                && analysis.externalBackgroundMask.at(index) == 0U;
            if (!candidateGap)
            {
                continue;
            }
            analysis.candidateGapMask.at(index) = 1U;

            const bool nearTexture = HasMaskNeighbor(
                input.textureSurfaceMask,
                input.widthPx,
                input.heightPx,
                x,
                y,
                connectivity,
                maxGapPx);
            const bool nearModelFill = HasMaskNeighbor(
                input.modelFillMask,
                input.widthPx,
                input.heightPx,
                x,
                y,
                connectivity,
                maxGapPx);
            const bool nearModel = HasMaskNeighbor(
                input.modelMaterialMask,
                input.widthPx,
                input.heightPx,
                x,
                y,
                connectivity,
                maxGapPx);
            const bool nearSupport = HasMaskNeighbor(
                input.supportFillMask,
                input.widthPx,
                input.heightPx,
                x,
                y,
                connectivity,
                maxGapPx);
            const bool nearOuterVarnish = HasMaskNeighbor(
                input.outerVarnishShellMask,
                input.widthPx,
                input.heightPx,
                x,
                y,
                connectivity,
                maxGapPx);

            const bool colorFillGap = nearTexture && nearModelFill;
            const bool modelSupportGap = nearModel && nearSupport;
            const bool colorSupportGap = nearTexture && nearSupport;
            const bool internalVoidGap = input.modelEnvelopeMask.at(index) != 0U;
            const bool varnishSupportGap = input.supportRequiredMask.at(index) != 0U
                && nearOuterVarnish && nearSupport;

            analysis.colorFillGapMask.at(index) = colorFillGap ? 1U : 0U;
            analysis.modelSupportGapMask.at(index) = modelSupportGap ? 1U : 0U;
            analysis.colorSupportGapMask.at(index) = colorSupportGap ? 1U : 0U;
            analysis.internalVoidGapMask.at(index) = internalVoidGap ? 1U : 0U;
            analysis.varnishSupportGapMask.at(index) = varnishSupportGap ? 1U : 0U;
            analysis.summary.colorFillGapPixels += colorFillGap ? 1 : 0;
            analysis.summary.modelSupportGapPixels += modelSupportGap ? 1 : 0;
            analysis.summary.colorSupportGapPixels += colorSupportGap ? 1 : 0;
            analysis.summary.internalVoidGapPixels += internalVoidGap ? 1 : 0;
            analysis.summary.varnishSupportGapPixels += varnishSupportGap ? 1 : 0;
            analysis.summary.gapPixels += (
                colorFillGap
                || modelSupportGap
                || colorSupportGap
                || internalVoidGap
                || varnishSupportGap)
                ? 1
                : 0;
        }
    }

    analysis.summary.remainingGapPixels = analysis.summary.gapPixels;
    analysis.summary.remainingColorFillGapPixels = analysis.summary.colorFillGapPixels;
    analysis.summary.remainingModelSupportGapPixels = analysis.summary.modelSupportGapPixels;
    analysis.summary.remainingColorSupportGapPixels = analysis.summary.colorSupportGapPixels;
    analysis.summary.remainingInternalVoidGapPixels = analysis.summary.internalVoidGapPixels;
    analysis.summary.remainingVarnishSupportGapPixels = analysis.summary.varnishSupportGapPixels;
    return analysis;
}

MaterialClosureSemanticLayerResult DetectMaterialClosureSemanticLayer(
    const MaterialClosureSemanticLayerInput& input,
    const int connectivity,
    const int maxGapPx)
{
    return AnalyzeMaterialClosureSemanticLayer(input, connectivity, maxGapPx).summary;
}

}  // namespace slicer_core
