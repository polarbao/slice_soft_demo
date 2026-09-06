#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
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
    const std::span<const std::uint8_t> mask,
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
    const MaterialClosureSemanticLayerInputView& input,
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

void ValidateWorkspace(
    const MaterialClosureSemanticWorkspace& workspace,
    const std::size_t pixelCount)
{
    ValidateMask(workspace.externalBackgroundMask, pixelCount, "workspace.externalBackgroundMask");
    ValidateMask(workspace.candidateGapMask, pixelCount, "workspace.candidateGapMask");
    ValidateMask(workspace.colorFillGapMask, pixelCount, "workspace.colorFillGapMask");
    ValidateMask(workspace.modelSupportGapMask, pixelCount, "workspace.modelSupportGapMask");
    ValidateMask(workspace.colorSupportGapMask, pixelCount, "workspace.colorSupportGapMask");
    ValidateMask(workspace.internalVoidGapMask, pixelCount, "workspace.internalVoidGapMask");
    ValidateMask(workspace.varnishSupportGapMask, pixelCount, "workspace.varnishSupportGapMask");
    if (workspace.traversalQueue.capacity() < pixelCount)
    {
        throw std::invalid_argument(
            "material closure semantic traversal workspace is not prepared");
    }
}

template <std::size_t DirectionCount>
void BuildExternalBackgroundMask(
    const std::span<const std::uint8_t> layerEmptyMask,
    const int widthPx,
    const int heightPx,
    const std::array<Direction, DirectionCount>& directions,
    int& protectedPixels,
    std::vector<std::uint8_t>& external,
    std::vector<std::size_t>& pending)
{
    std::fill(external.begin(), external.end(), static_cast<std::uint8_t>(0U));
    pending.clear();

    const auto enqueue = [&](const int x, const int y)
    {
        const std::size_t index = PixelIndex(widthPx, x, y);
        if (layerEmptyMask[index] != 0U && external.at(index) == 0U)
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
    for (std::size_t cursor{0U}; cursor < pending.size(); ++cursor)
    {
        const std::size_t index = pending[cursor];
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
}

bool HasMaskNeighbor(
    const std::span<const std::uint8_t> mask,
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
                && mask[PixelIndex(widthPx, x, y)] != 0U)
            {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

void MaterialClosureSemanticWorkspace::Prepare(const std::size_t pixelCount)
{
    externalBackgroundMask.resize(pixelCount);
    candidateGapMask.resize(pixelCount);
    colorFillGapMask.resize(pixelCount);
    modelSupportGapMask.resize(pixelCount);
    colorSupportGapMask.resize(pixelCount);
    internalVoidGapMask.resize(pixelCount);
    varnishSupportGapMask.resize(pixelCount);
    traversalQueue.reserve(pixelCount);
}

MaterialClosureSemanticLayerInputView ViewMaterialClosureSemanticLayerInput(
    const MaterialClosureSemanticLayerInput& input) noexcept
{
    return MaterialClosureSemanticLayerInputView{
        input.layerIndex,
        input.zMm,
        input.widthPx,
        input.heightPx,
        input.textureSurfaceMask,
        input.modelFillMask,
        input.modelMaterialMask,
        input.supportFillMask,
        input.internalVoidSupportMask,
        input.surfaceVarnishMask,
        input.outerVarnishShellMask,
        input.modelEnvelopeMask,
        input.supportRequiredMask,
        input.expectedOccupiedDomainMask,
        input.layerEmptyMask};
}

MaterialClosureSemanticLayerAnalysisView AnalyzeMaterialClosureSemanticLayer(
    const MaterialClosureSemanticLayerInputView& input,
    const int connectivity,
    const int maxGapPx,
    MaterialClosureSemanticWorkspace& workspace)
{
    ValidateInput(input, connectivity, maxGapPx);

    MaterialClosureSemanticLayerAnalysisView analysis;
    analysis.widthPx = input.widthPx;
    analysis.heightPx = input.heightPx;
    analysis.summary.layerIndex = input.layerIndex;
    analysis.summary.zMm = input.zMm;
    const std::size_t pixelCount = static_cast<std::size_t>(input.widthPx)
        * static_cast<std::size_t>(input.heightPx);
    ValidateWorkspace(workspace, pixelCount);
    std::fill(workspace.candidateGapMask.begin(), workspace.candidateGapMask.end(), 0U);
    std::fill(workspace.colorFillGapMask.begin(), workspace.colorFillGapMask.end(), 0U);
    std::fill(workspace.modelSupportGapMask.begin(), workspace.modelSupportGapMask.end(), 0U);
    std::fill(workspace.colorSupportGapMask.begin(), workspace.colorSupportGapMask.end(), 0U);
    std::fill(workspace.internalVoidGapMask.begin(), workspace.internalVoidGapMask.end(), 0U);
    std::fill(workspace.varnishSupportGapMask.begin(), workspace.varnishSupportGapMask.end(), 0U);

    if (connectivity == 8)
    {
        BuildExternalBackgroundMask(
            input.layerEmptyMask,
            input.widthPx,
            input.heightPx,
            directions8,
            analysis.summary.externalBackgroundProtectedPixels,
            workspace.externalBackgroundMask,
            workspace.traversalQueue);
    }
    else
    {
        BuildExternalBackgroundMask(
            input.layerEmptyMask,
            input.widthPx,
            input.heightPx,
            directions4,
            analysis.summary.externalBackgroundProtectedPixels,
            workspace.externalBackgroundMask,
            workspace.traversalQueue);
    }

    for (int y{0}; y < input.heightPx; ++y)
    {
        for (int x{0}; x < input.widthPx; ++x)
        {
            const std::size_t index = PixelIndex(input.widthPx, x, y);
            const bool candidateGap = input.layerEmptyMask[index] != 0U
                && input.expectedOccupiedDomainMask[index] != 0U
                && workspace.externalBackgroundMask.at(index) == 0U;
            if (!candidateGap)
            {
                continue;
            }
            workspace.candidateGapMask.at(index) = 1U;

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
            const bool internalVoidGap = input.modelEnvelopeMask[index] != 0U;
            const bool varnishSupportGap = input.supportRequiredMask[index] != 0U
                && nearOuterVarnish && nearSupport;

            workspace.colorFillGapMask.at(index) = colorFillGap ? 1U : 0U;
            workspace.modelSupportGapMask.at(index) = modelSupportGap ? 1U : 0U;
            workspace.colorSupportGapMask.at(index) = colorSupportGap ? 1U : 0U;
            workspace.internalVoidGapMask.at(index) = internalVoidGap ? 1U : 0U;
            workspace.varnishSupportGapMask.at(index) = varnishSupportGap ? 1U : 0U;
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
    analysis.externalBackgroundMask = workspace.externalBackgroundMask;
    analysis.candidateGapMask = workspace.candidateGapMask;
    analysis.colorFillGapMask = workspace.colorFillGapMask;
    analysis.modelSupportGapMask = workspace.modelSupportGapMask;
    analysis.colorSupportGapMask = workspace.colorSupportGapMask;
    analysis.internalVoidGapMask = workspace.internalVoidGapMask;
    analysis.varnishSupportGapMask = workspace.varnishSupportGapMask;
    return analysis;
}

MaterialClosureSemanticLayerResult DetectMaterialClosureSemanticLayer(
    const MaterialClosureSemanticLayerInputView& input,
    const int connectivity,
    const int maxGapPx,
    MaterialClosureSemanticWorkspace& workspace)
{
    return AnalyzeMaterialClosureSemanticLayer(
        input,
        connectivity,
        maxGapPx,
        workspace).summary;
}

MaterialClosureSemanticLayerAnalysis AnalyzeMaterialClosureSemanticLayer(
    const MaterialClosureSemanticLayerInput& input,
    const int connectivity,
    const int maxGapPx)
{
    const MaterialClosureSemanticLayerInputView inputView =
        ViewMaterialClosureSemanticLayerInput(input);
    ValidateInput(inputView, connectivity, maxGapPx);
    const std::size_t pixelCount = static_cast<std::size_t>(input.widthPx)
        * static_cast<std::size_t>(input.heightPx);
    MaterialClosureSemanticWorkspace workspace;
    workspace.Prepare(pixelCount);
    const MaterialClosureSemanticLayerAnalysisView view =
        AnalyzeMaterialClosureSemanticLayer(
            inputView,
            connectivity,
            maxGapPx,
            workspace);

    MaterialClosureSemanticLayerAnalysis analysis;
    analysis.widthPx = view.widthPx;
    analysis.heightPx = view.heightPx;
    analysis.summary = view.summary;
    analysis.externalBackgroundMask.assign(
        view.externalBackgroundMask.begin(), view.externalBackgroundMask.end());
    analysis.candidateGapMask.assign(
        view.candidateGapMask.begin(), view.candidateGapMask.end());
    analysis.colorFillGapMask.assign(
        view.colorFillGapMask.begin(), view.colorFillGapMask.end());
    analysis.modelSupportGapMask.assign(
        view.modelSupportGapMask.begin(), view.modelSupportGapMask.end());
    analysis.colorSupportGapMask.assign(
        view.colorSupportGapMask.begin(), view.colorSupportGapMask.end());
    analysis.internalVoidGapMask.assign(
        view.internalVoidGapMask.begin(), view.internalVoidGapMask.end());
    analysis.varnishSupportGapMask.assign(
        view.varnishSupportGapMask.begin(), view.varnishSupportGapMask.end());
    return analysis;
}

MaterialClosureSemanticLayerResult DetectMaterialClosureSemanticLayer(
    const MaterialClosureSemanticLayerInput& input,
    const int connectivity,
    const int maxGapPx)
{
    const MaterialClosureSemanticLayerInputView inputView =
        ViewMaterialClosureSemanticLayerInput(input);
    ValidateInput(inputView, connectivity, maxGapPx);
    const std::size_t pixelCount = static_cast<std::size_t>(input.widthPx)
        * static_cast<std::size_t>(input.heightPx);
    MaterialClosureSemanticWorkspace workspace;
    workspace.Prepare(pixelCount);
    return DetectMaterialClosureSemanticLayer(
        inputView,
        connectivity,
        maxGapPx,
        workspace);
}

}  // namespace slicer_core
