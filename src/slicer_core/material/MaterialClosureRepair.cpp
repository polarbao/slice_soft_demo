#include "slicer_core/material/MaterialClosureRepair.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
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
    const char* name)
{
    if (mask.size() != pixelCount)
    {
        throw std::invalid_argument(std::string{"material closure repair mask size mismatch: "} + name);
    }
}

void ValidatePlanInput(
    const MaterialClosureSemanticLayerInputView& input,
    const MaterialClosureSemanticLayerAnalysisView& analysis,
    const int connectivity)
{
    if (input.widthPx <= 0 || input.heightPx <= 0
        || analysis.widthPx != input.widthPx || analysis.heightPx != input.heightPx)
    {
        throw std::invalid_argument("material closure repair dimensions mismatch");
    }
    if (connectivity != 4 && connectivity != 8)
    {
        throw std::invalid_argument("material closure repair connectivity must be 4 or 8");
    }
    const std::size_t pixelCount = static_cast<std::size_t>(input.widthPx)
        * static_cast<std::size_t>(input.heightPx);
    ValidateMask(analysis.externalBackgroundMask, pixelCount, "externalBackgroundMask");
    ValidateMask(analysis.candidateGapMask, pixelCount, "candidateGapMask");
    ValidateMask(analysis.colorFillGapMask, pixelCount, "colorFillGapMask");
    ValidateMask(analysis.modelSupportGapMask, pixelCount, "modelSupportGapMask");
    ValidateMask(analysis.colorSupportGapMask, pixelCount, "colorSupportGapMask");
    ValidateMask(analysis.internalVoidGapMask, pixelCount, "internalVoidGapMask");
    ValidateMask(analysis.varnishSupportGapMask, pixelCount, "varnishSupportGapMask");
    ValidateMask(input.modelFillMask, pixelCount, "input.modelFillMask");
    ValidateMask(input.modelMaterialMask, pixelCount, "input.modelMaterialMask");
    ValidateMask(input.supportFillMask, pixelCount, "input.supportFillMask");
    ValidateMask(input.internalVoidSupportMask, pixelCount, "input.internalVoidSupportMask");
    ValidateMask(input.modelEnvelopeMask, pixelCount, "input.modelEnvelopeMask");
    ValidateMask(input.supportRequiredMask, pixelCount, "input.supportRequiredMask");
    ValidateMask(input.expectedOccupiedDomainMask, pixelCount, "input.expectedOccupiedDomainMask");
    ValidateMask(input.layerEmptyMask, pixelCount, "input.layerEmptyMask");
}

template <std::size_t DirectionCount>
bool HasMaskNeighbor(
    const std::span<const std::uint8_t> mask,
    const int widthPx,
    const int heightPx,
    const std::size_t index,
    const std::array<Direction, DirectionCount>& directions)
{
    const int x = static_cast<int>(index % static_cast<std::size_t>(widthPx));
    const int y = static_cast<int>(index / static_cast<std::size_t>(widthPx));
    for (const Direction& direction : directions)
    {
        const int nextX = x + direction.dx;
        const int nextY = y + direction.dy;
        if (IsInside(widthPx, heightPx, nextX, nextY)
            && mask[PixelIndex(widthPx, nextX, nextY)] != 0U)
        {
            return true;
        }
    }
    return false;
}

bool ContainsTwoByTwoBlock(
    const std::span<const std::uint8_t> componentMask,
    const int widthPx,
    const int heightPx)
{
    for (int y{0}; y + 1 < heightPx; ++y)
    {
        for (int x{0}; x + 1 < widthPx; ++x)
        {
            if (componentMask[PixelIndex(widthPx, x, y)] != 0U
                && componentMask[PixelIndex(widthPx, x + 1, y)] != 0U
                && componentMask[PixelIndex(widthPx, x, y + 1)] != 0U
                && componentMask[PixelIndex(widthPx, x + 1, y + 1)] != 0U)
            {
                return true;
            }
        }
    }
    return false;
}

template <std::size_t DirectionCount>
void CollectComponent(
    const std::span<const std::uint8_t> candidateGapMask,
    const int widthPx,
    const int heightPx,
    const std::size_t startIndex,
    const std::array<Direction, DirectionCount>& directions,
    std::vector<std::uint8_t>& visited,
    std::vector<std::size_t>& component)
{
    component.clear();
    visited.at(startIndex) = 1U;
    component.push_back(startIndex);
    for (std::size_t cursor{0U}; cursor < component.size(); ++cursor)
    {
        const std::size_t index = component[cursor];
        const int x = static_cast<int>(index % static_cast<std::size_t>(widthPx));
        const int y = static_cast<int>(index / static_cast<std::size_t>(widthPx));
        for (const Direction& direction : directions)
        {
            const int nextX = x + direction.dx;
            const int nextY = y + direction.dy;
            if (!IsInside(widthPx, heightPx, nextX, nextY))
            {
                continue;
            }
            const std::size_t nextIndex = PixelIndex(widthPx, nextX, nextY);
            if (candidateGapMask[nextIndex] != 0U && visited.at(nextIndex) == 0U)
            {
                visited.at(nextIndex) = 1U;
                component.push_back(nextIndex);
            }
        }
    }
}

template <std::size_t DirectionCount>
void AddComponentToPlan(
    const MaterialClosureSemanticLayerInputView& input,
    const MaterialClosureSemanticLayerAnalysisView& analysis,
    const std::span<const std::size_t> component,
    const std::array<Direction, DirectionCount>& directions,
    MaterialClosureRepairWorkspace& workspace,
    int& rejectedTooWidePixels)
{
    bool allPixelsExplicitlyClassified{true};
    bool nearTexture{false};
    bool nearModelFill{false};
    bool nearModel{false};
    bool nearSupport{false};
    bool nearOuterVarnish{false};

    for (const std::size_t index : component)
    {
        workspace.componentMask.at(index) = 1U;
        const bool explicitlyRepairable = analysis.colorFillGapMask[index] != 0U
            || analysis.modelSupportGapMask[index] != 0U
            || analysis.varnishSupportGapMask[index] != 0U;
        allPixelsExplicitlyClassified =
            allPixelsExplicitlyClassified && explicitlyRepairable;
        nearTexture = nearTexture || HasMaskNeighbor(
            input.textureSurfaceMask,
            input.widthPx,
            input.heightPx,
            index,
            directions);
        nearModelFill = nearModelFill || HasMaskNeighbor(
            input.modelFillMask,
            input.widthPx,
            input.heightPx,
            index,
            directions);
        nearModel = nearModel || HasMaskNeighbor(
            input.modelMaterialMask,
            input.widthPx,
            input.heightPx,
            index,
            directions);
        nearSupport = nearSupport || HasMaskNeighbor(
            input.supportFillMask,
            input.widthPx,
            input.heightPx,
            index,
            directions);
        nearOuterVarnish = nearOuterVarnish || HasMaskNeighbor(
            input.outerVarnishShellMask,
            input.widthPx,
            input.heightPx,
            index,
            directions);
    }

    const bool hasExplicitRepairRelationship = (nearTexture && nearModelFill)
        || (nearModel && nearSupport)
        || (nearOuterVarnish && nearSupport);
    const bool tooWide = ContainsTwoByTwoBlock(
                             workspace.componentMask,
                             input.widthPx,
                             input.heightPx)
        || (hasExplicitRepairRelationship && !allPixelsExplicitlyClassified);
    if (tooWide)
    {
        for (const std::size_t index : component)
        {
            workspace.rejectedTooWideMask.at(index) = 1U;
            ++rejectedTooWidePixels;
            workspace.componentMask.at(index) = 0U;
        }
        return;
    }

    for (const std::size_t index : component)
    {
        if (analysis.externalBackgroundMask[index] != 0U
            || input.expectedOccupiedDomainMask[index] == 0U)
        {
            workspace.componentMask.at(index) = 0U;
            continue;
        }

        if (analysis.colorFillGapMask[index] != 0U)
        {
            workspace.modelFillRepairMask.at(index) = 1U;
            workspace.colorFillRepairMask.at(index) = 1U;
        }
        else if (analysis.modelSupportGapMask[index] != 0U)
        {
            workspace.modelSupportRepairMask.at(index) = 1U;
            if (input.modelEnvelopeMask[index] != 0U)
            {
                workspace.modelFillRepairMask.at(index) = 1U;
            }
            else if (input.supportRequiredMask[index] != 0U)
            {
                workspace.supportRepairMask.at(index) = 1U;
            }
        }
        else if (analysis.varnishSupportGapMask[index] != 0U
                 && input.supportRequiredMask[index] != 0U)
        {
            workspace.supportRepairMask.at(index) = 1U;
            workspace.varnishSupportRepairMask.at(index) = 1U;
        }
        else if (analysis.internalVoidGapMask[index] != 0U
                 && analysis.colorSupportGapMask[index] == 0U)
        {
            workspace.supportRepairMask.at(index) = 1U;
            workspace.internalVoidSupportRepairMask.at(index) = 1U;
        }
        workspace.componentMask.at(index) = 0U;
    }
}

void WriteModelFill(
    const MaterialClosureRepairValues& values,
    const std::size_t base,
    const std::span<std::uint8_t> layer)
{
    switch (values.modelFillMaterial)
    {
        case MaterialClosureModelFillMaterial::Rgb:
            layer[base + 0U] = values.modelFillRgb.at(0);
            layer[base + 1U] = values.modelFillRgb.at(1);
            layer[base + 2U] = values.modelFillRgb.at(2);
            return;
        case MaterialClosureModelFillMaterial::White:
            layer[base + 3U] = values.modelFillValue;
            return;
        case MaterialClosureModelFillMaterial::Varnish:
            layer[base + 5U] = values.modelFillValue;
            return;
        case MaterialClosureModelFillMaterial::None:
            return;
    }
}

}  // namespace

void MaterialClosureRepairWorkspace::Prepare(const std::size_t pixelCount)
{
    externalBackgroundMask.resize(pixelCount);
    expectedOccupiedDomainMask.resize(pixelCount);
    modelFillRepairMask.resize(pixelCount);
    supportRepairMask.resize(pixelCount);
    internalVoidSupportRepairMask.resize(pixelCount);
    colorFillRepairMask.resize(pixelCount);
    modelSupportRepairMask.resize(pixelCount);
    varnishSupportRepairMask.resize(pixelCount);
    rejectedTooWideMask.resize(pixelCount);
    visited.resize(pixelCount);
    componentMask.resize(pixelCount);
    componentPixels.reserve(pixelCount);
}

MaterialClosureSemanticLayerMutableInputView
ViewMaterialClosureSemanticLayerInput(
    MaterialClosureSemanticLayerInput& input) noexcept
{
    return MaterialClosureSemanticLayerMutableInputView{
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

MaterialClosureRepairPlanView BuildMaterialClosureRepairPlan(
    const MaterialClosureSemanticLayerInputView& input,
    const MaterialClosureSemanticLayerAnalysisView& analysis,
    const int connectivity,
    MaterialClosureRepairWorkspace& workspace)
{
    ValidatePlanInput(input, analysis, connectivity);
    const std::size_t pixelCount = analysis.candidateGapMask.size();
    ValidateMask(workspace.externalBackgroundMask, pixelCount, "workspace.externalBackgroundMask");
    ValidateMask(workspace.expectedOccupiedDomainMask, pixelCount, "workspace.expectedOccupiedDomainMask");
    ValidateMask(workspace.modelFillRepairMask, pixelCount, "workspace.modelFillRepairMask");
    ValidateMask(workspace.supportRepairMask, pixelCount, "workspace.supportRepairMask");
    ValidateMask(workspace.internalVoidSupportRepairMask, pixelCount, "workspace.internalVoidSupportRepairMask");
    ValidateMask(workspace.colorFillRepairMask, pixelCount, "workspace.colorFillRepairMask");
    ValidateMask(workspace.modelSupportRepairMask, pixelCount, "workspace.modelSupportRepairMask");
    ValidateMask(workspace.varnishSupportRepairMask, pixelCount, "workspace.varnishSupportRepairMask");
    ValidateMask(workspace.rejectedTooWideMask, pixelCount, "workspace.rejectedTooWideMask");
    ValidateMask(workspace.visited, pixelCount, "workspace.visited");
    ValidateMask(workspace.componentMask, pixelCount, "workspace.componentMask");
    if (workspace.componentPixels.capacity() < pixelCount)
    {
        throw std::invalid_argument(
            "material closure repair component workspace is not prepared");
    }

    std::copy(
        analysis.externalBackgroundMask.begin(),
        analysis.externalBackgroundMask.end(),
        workspace.externalBackgroundMask.begin());
    std::copy(
        input.expectedOccupiedDomainMask.begin(),
        input.expectedOccupiedDomainMask.end(),
        workspace.expectedOccupiedDomainMask.begin());
    std::fill(workspace.modelFillRepairMask.begin(), workspace.modelFillRepairMask.end(), 0U);
    std::fill(workspace.supportRepairMask.begin(), workspace.supportRepairMask.end(), 0U);
    std::fill(workspace.internalVoidSupportRepairMask.begin(), workspace.internalVoidSupportRepairMask.end(), 0U);
    std::fill(workspace.colorFillRepairMask.begin(), workspace.colorFillRepairMask.end(), 0U);
    std::fill(workspace.modelSupportRepairMask.begin(), workspace.modelSupportRepairMask.end(), 0U);
    std::fill(workspace.varnishSupportRepairMask.begin(), workspace.varnishSupportRepairMask.end(), 0U);
    std::fill(workspace.rejectedTooWideMask.begin(), workspace.rejectedTooWideMask.end(), 0U);
    std::fill(workspace.visited.begin(), workspace.visited.end(), 0U);
    std::fill(workspace.componentMask.begin(), workspace.componentMask.end(), 0U);

    int rejectedTooWidePixels{0};
    for (std::size_t index{0U}; index < pixelCount; ++index)
    {
        if (analysis.candidateGapMask[index] == 0U
            || workspace.visited.at(index) != 0U)
        {
            continue;
        }
        if (connectivity == 8)
        {
            CollectComponent(
                analysis.candidateGapMask,
                input.widthPx,
                input.heightPx,
                index,
                directions8,
                workspace.visited,
                workspace.componentPixels);
            AddComponentToPlan(
                input,
                analysis,
                workspace.componentPixels,
                directions8,
                workspace,
                rejectedTooWidePixels);
        }
        else
        {
            CollectComponent(
                analysis.candidateGapMask,
                input.widthPx,
                input.heightPx,
                index,
                directions4,
                workspace.visited,
                workspace.componentPixels);
            AddComponentToPlan(
                input,
                analysis,
                workspace.componentPixels,
                directions4,
                workspace,
                rejectedTooWidePixels);
        }
    }

    int modelFillRepairPixels{0};
    int supportRepairPixels{0};
    for (std::size_t index{0U}; index < pixelCount; ++index)
    {
        modelFillRepairPixels += workspace.modelFillRepairMask.at(index) != 0U ? 1 : 0;
        supportRepairPixels += workspace.supportRepairMask.at(index) != 0U ? 1 : 0;
    }
    return MaterialClosureRepairPlanView{
        input.widthPx,
        input.heightPx,
        workspace.externalBackgroundMask,
        workspace.expectedOccupiedDomainMask,
        workspace.modelFillRepairMask,
        workspace.supportRepairMask,
        workspace.internalVoidSupportRepairMask,
        workspace.colorFillRepairMask,
        workspace.modelSupportRepairMask,
        workspace.varnishSupportRepairMask,
        workspace.rejectedTooWideMask,
        modelFillRepairPixels,
        supportRepairPixels,
        rejectedTooWidePixels,
        analysis.summary.externalBackgroundProtectedPixels};
}

MaterialClosureRepairApplicationResult ApplyMaterialClosureRepair(
    const MaterialClosureRepairPlanView& plan,
    const MaterialClosureRepairValues& values,
    const std::span<std::uint8_t> layer,
    MaterialClosureSemanticLayerMutableInputView& input)
{
    constexpr std::size_t channelCount{6U};
    if (plan.widthPx != input.widthPx || plan.heightPx != input.heightPx)
    {
        throw std::invalid_argument("material closure repair application dimensions mismatch");
    }
    const std::size_t pixelCount = static_cast<std::size_t>(plan.widthPx)
        * static_cast<std::size_t>(plan.heightPx);
    if (layer.size() != pixelCount * channelCount)
    {
        throw std::invalid_argument("material closure repair RGBWSV layer size mismatch");
    }
    ValidateMask(plan.modelFillRepairMask, pixelCount, "modelFillRepairMask");
    ValidateMask(plan.supportRepairMask, pixelCount, "supportRepairMask");
    ValidateMask(plan.externalBackgroundMask, pixelCount, "externalBackgroundMask");
    ValidateMask(
        plan.expectedOccupiedDomainMask,
        pixelCount,
        "expectedOccupiedDomainMask");
    ValidateMask(plan.internalVoidSupportRepairMask, pixelCount, "internalVoidSupportRepairMask");
    ValidateMask(plan.colorFillRepairMask, pixelCount, "colorFillRepairMask");
    ValidateMask(plan.modelSupportRepairMask, pixelCount, "modelSupportRepairMask");
    ValidateMask(plan.varnishSupportRepairMask, pixelCount, "varnishSupportRepairMask");
    ValidateMask(plan.rejectedTooWideMask, pixelCount, "rejectedTooWideMask");
    ValidateMask(input.modelFillMask, pixelCount, "input.modelFillMask");
    ValidateMask(input.modelMaterialMask, pixelCount, "input.modelMaterialMask");
    ValidateMask(input.supportFillMask, pixelCount, "input.supportFillMask");
    ValidateMask(input.internalVoidSupportMask, pixelCount, "input.internalVoidSupportMask");
    ValidateMask(input.layerEmptyMask, pixelCount, "input.layerEmptyMask");
    if (plan.modelFillRepairPixels > 0)
    {
        const bool modelFillIsEmpty =
            (values.modelFillMaterial == MaterialClosureModelFillMaterial::Rgb
             && values.modelFillRgb.at(0) == 255U
             && values.modelFillRgb.at(1) == 255U
             && values.modelFillRgb.at(2) == 255U)
            || ((values.modelFillMaterial == MaterialClosureModelFillMaterial::White
                 || values.modelFillMaterial == MaterialClosureModelFillMaterial::Varnish)
                && values.modelFillValue == 255U)
            || values.modelFillMaterial == MaterialClosureModelFillMaterial::None;
        if (modelFillIsEmpty)
        {
            throw std::invalid_argument("material closure repair model fill value must print");
        }
    }
    if (plan.supportRepairPixels > 0 && values.supportValue == 255U)
    {
        throw std::invalid_argument("material closure repair support value must print");
    }

    MaterialClosureRepairApplicationResult result;
    for (std::size_t index{0U}; index < pixelCount; ++index)
    {
        if (input.layerEmptyMask[index] == 0U)
        {
            continue;
        }
        const bool repairModelFill = plan.modelFillRepairMask[index] != 0U;
        const bool repairSupport = plan.supportRepairMask[index] != 0U;
        if (!repairModelFill && !repairSupport)
        {
            continue;
        }
        if (plan.externalBackgroundMask[index] != 0U)
        {
            ++result.blockedExternalBackgroundRepairPixels;
            continue;
        }
        if (plan.expectedOccupiedDomainMask[index] == 0U)
        {
            ++result.blockedOutsideExpectedDomainRepairPixels;
            continue;
        }
        if (plan.rejectedTooWideMask[index] != 0U)
        {
            ++result.blockedRejectedTooWideRepairPixels;
            continue;
        }

        const std::size_t base = index * channelCount;
        if (repairModelFill)
        {
            if (values.modelFillMaterial == MaterialClosureModelFillMaterial::None)
            {
                continue;
            }
            WriteModelFill(values, base, layer);
            input.modelFillMask[index] = 1U;
            input.modelMaterialMask[index] = 1U;
            ++result.repairedModelFillPixels;
        }
        else
        {
            layer[base + 4U] = values.supportValue;
            input.supportFillMask[index] = 1U;
            ++result.repairedSupportPixels;
            if (plan.internalVoidSupportRepairMask[index] != 0U)
            {
                input.internalVoidSupportMask[index] = 1U;
            }
        }
        input.layerEmptyMask[index] = 0U;
        ++result.repairedPixels;
        result.repairedColorFillPixels += plan.colorFillRepairMask[index] != 0U ? 1 : 0;
        result.repairedModelSupportPixels += plan.modelSupportRepairMask[index] != 0U ? 1 : 0;
        result.repairedInternalVoidPixels +=
            plan.internalVoidSupportRepairMask[index] != 0U ? 1 : 0;
        result.repairedVarnishSupportPixels +=
            plan.varnishSupportRepairMask[index] != 0U ? 1 : 0;
    }
    return result;
}

MaterialClosureRepairPlan BuildMaterialClosureRepairPlan(
    const MaterialClosureSemanticLayerInput& input,
    const MaterialClosureSemanticLayerAnalysis& analysis,
    const int connectivity)
{
    const MaterialClosureSemanticLayerInputView inputView =
        ViewMaterialClosureSemanticLayerInput(
            static_cast<const MaterialClosureSemanticLayerInput&>(input));
    const MaterialClosureSemanticLayerAnalysisView analysisView{
        analysis.widthPx,
        analysis.heightPx,
        analysis.summary,
        analysis.externalBackgroundMask,
        analysis.candidateGapMask,
        analysis.colorFillGapMask,
        analysis.modelSupportGapMask,
        analysis.colorSupportGapMask,
        analysis.internalVoidGapMask,
        analysis.varnishSupportGapMask};
    ValidatePlanInput(inputView, analysisView, connectivity);

    MaterialClosureRepairWorkspace workspace;
    workspace.Prepare(analysis.candidateGapMask.size());
    const MaterialClosureRepairPlanView view = BuildMaterialClosureRepairPlan(
        inputView,
        analysisView,
        connectivity,
        workspace);

    MaterialClosureRepairPlan plan;
    plan.widthPx = view.widthPx;
    plan.heightPx = view.heightPx;
    plan.externalBackgroundMask.assign(
        view.externalBackgroundMask.begin(), view.externalBackgroundMask.end());
    plan.expectedOccupiedDomainMask.assign(
        view.expectedOccupiedDomainMask.begin(), view.expectedOccupiedDomainMask.end());
    plan.modelFillRepairMask.assign(
        view.modelFillRepairMask.begin(), view.modelFillRepairMask.end());
    plan.supportRepairMask.assign(
        view.supportRepairMask.begin(), view.supportRepairMask.end());
    plan.internalVoidSupportRepairMask.assign(
        view.internalVoidSupportRepairMask.begin(), view.internalVoidSupportRepairMask.end());
    plan.colorFillRepairMask.assign(
        view.colorFillRepairMask.begin(), view.colorFillRepairMask.end());
    plan.modelSupportRepairMask.assign(
        view.modelSupportRepairMask.begin(), view.modelSupportRepairMask.end());
    plan.varnishSupportRepairMask.assign(
        view.varnishSupportRepairMask.begin(), view.varnishSupportRepairMask.end());
    plan.rejectedTooWideMask.assign(
        view.rejectedTooWideMask.begin(), view.rejectedTooWideMask.end());
    plan.modelFillRepairPixels = view.modelFillRepairPixels;
    plan.supportRepairPixels = view.supportRepairPixels;
    plan.rejectedTooWidePixels = view.rejectedTooWidePixels;
    plan.externalBackgroundProtectedPixels = view.externalBackgroundProtectedPixels;
    return plan;
}

MaterialClosureRepairApplicationResult ApplyMaterialClosureRepair(
    const MaterialClosureRepairPlan& plan,
    const MaterialClosureRepairValues& values,
    std::vector<std::uint8_t>& layer,
    MaterialClosureSemanticLayerInput& input)
{
    const MaterialClosureRepairPlanView planView{
        plan.widthPx,
        plan.heightPx,
        plan.externalBackgroundMask,
        plan.expectedOccupiedDomainMask,
        plan.modelFillRepairMask,
        plan.supportRepairMask,
        plan.internalVoidSupportRepairMask,
        plan.colorFillRepairMask,
        plan.modelSupportRepairMask,
        plan.varnishSupportRepairMask,
        plan.rejectedTooWideMask,
        plan.modelFillRepairPixels,
        plan.supportRepairPixels,
        plan.rejectedTooWidePixels,
        plan.externalBackgroundProtectedPixels};
    MaterialClosureSemanticLayerMutableInputView inputView =
        ViewMaterialClosureSemanticLayerInput(input);
    return ApplyMaterialClosureRepair(planView, values, layer, inputView);
}

}  // namespace slicer_core
