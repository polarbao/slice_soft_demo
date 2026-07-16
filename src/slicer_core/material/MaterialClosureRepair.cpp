#include "slicer_core/material/MaterialClosureRepair.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
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
    const std::vector<std::uint8_t>& mask,
    const std::size_t pixelCount,
    const char* name)
{
    if (mask.size() != pixelCount)
    {
        throw std::invalid_argument(std::string{"material closure repair mask size mismatch: "} + name);
    }
}

void ValidatePlanInput(
    const MaterialClosureSemanticLayerInput& input,
    const MaterialClosureSemanticLayerAnalysis& analysis,
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
    const std::vector<std::uint8_t>& mask,
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
            && mask.at(PixelIndex(widthPx, nextX, nextY)) != 0U)
        {
            return true;
        }
    }
    return false;
}

bool ContainsTwoByTwoBlock(
    const std::vector<std::uint8_t>& componentMask,
    const int widthPx,
    const int heightPx)
{
    for (int y{0}; y + 1 < heightPx; ++y)
    {
        for (int x{0}; x + 1 < widthPx; ++x)
        {
            if (componentMask.at(PixelIndex(widthPx, x, y)) != 0U
                && componentMask.at(PixelIndex(widthPx, x + 1, y)) != 0U
                && componentMask.at(PixelIndex(widthPx, x, y + 1)) != 0U
                && componentMask.at(PixelIndex(widthPx, x + 1, y + 1)) != 0U)
            {
                return true;
            }
        }
    }
    return false;
}

template <std::size_t DirectionCount>
std::vector<std::size_t> CollectComponent(
    const std::vector<std::uint8_t>& candidateGapMask,
    const int widthPx,
    const int heightPx,
    const std::size_t startIndex,
    const std::array<Direction, DirectionCount>& directions,
    std::vector<std::uint8_t>& visited)
{
    std::vector<std::size_t> component;
    std::deque<std::size_t> pending;
    visited.at(startIndex) = 1U;
    pending.push_back(startIndex);
    while (!pending.empty())
    {
        const std::size_t index = pending.front();
        pending.pop_front();
        component.push_back(index);
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
            if (candidateGapMask.at(nextIndex) != 0U && visited.at(nextIndex) == 0U)
            {
                visited.at(nextIndex) = 1U;
                pending.push_back(nextIndex);
            }
        }
    }
    return component;
}

template <std::size_t DirectionCount>
void AddComponentToPlan(
    const MaterialClosureSemanticLayerInput& input,
    const MaterialClosureSemanticLayerAnalysis& analysis,
    const std::vector<std::size_t>& component,
    const std::array<Direction, DirectionCount>& directions,
    std::vector<std::uint8_t>& componentMask,
    MaterialClosureRepairPlan& plan)
{
    bool allPixelsExplicitlyClassified{true};
    bool nearTexture{false};
    bool nearModelFill{false};
    bool nearModel{false};
    bool nearSupport{false};
    bool nearOuterVarnish{false};

    for (const std::size_t index : component)
    {
        componentMask.at(index) = 1U;
        const bool explicitlyRepairable = analysis.colorFillGapMask.at(index) != 0U
            || analysis.modelSupportGapMask.at(index) != 0U
            || analysis.varnishSupportGapMask.at(index) != 0U;
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
    const bool tooWide = ContainsTwoByTwoBlock(componentMask, input.widthPx, input.heightPx)
        || (hasExplicitRepairRelationship && !allPixelsExplicitlyClassified);
    if (tooWide)
    {
        for (const std::size_t index : component)
        {
            plan.rejectedTooWideMask.at(index) = 1U;
            ++plan.rejectedTooWidePixels;
            componentMask.at(index) = 0U;
        }
        return;
    }

    for (const std::size_t index : component)
    {
        if (analysis.externalBackgroundMask.at(index) != 0U
            || input.expectedOccupiedDomainMask.at(index) == 0U)
        {
            componentMask.at(index) = 0U;
            continue;
        }

        if (analysis.colorFillGapMask.at(index) != 0U)
        {
            plan.modelFillRepairMask.at(index) = 1U;
            plan.colorFillRepairMask.at(index) = 1U;
        }
        else if (analysis.modelSupportGapMask.at(index) != 0U)
        {
            plan.modelSupportRepairMask.at(index) = 1U;
            if (input.modelEnvelopeMask.at(index) != 0U)
            {
                plan.modelFillRepairMask.at(index) = 1U;
            }
            else if (input.supportRequiredMask.at(index) != 0U)
            {
                plan.supportRepairMask.at(index) = 1U;
            }
        }
        else if (analysis.varnishSupportGapMask.at(index) != 0U
                 && input.supportRequiredMask.at(index) != 0U)
        {
            plan.supportRepairMask.at(index) = 1U;
            plan.varnishSupportRepairMask.at(index) = 1U;
        }
        else if (analysis.internalVoidGapMask.at(index) != 0U
                 && analysis.colorSupportGapMask.at(index) == 0U)
        {
            plan.supportRepairMask.at(index) = 1U;
            plan.internalVoidSupportRepairMask.at(index) = 1U;
        }
        componentMask.at(index) = 0U;
    }
}

void WriteModelFill(
    const MaterialClosureRepairValues& values,
    const std::size_t base,
    std::vector<std::uint8_t>& layer)
{
    switch (values.modelFillMaterial)
    {
        case MaterialClosureModelFillMaterial::Rgb:
            layer.at(base + 0U) = values.modelFillRgb.at(0);
            layer.at(base + 1U) = values.modelFillRgb.at(1);
            layer.at(base + 2U) = values.modelFillRgb.at(2);
            return;
        case MaterialClosureModelFillMaterial::White:
            layer.at(base + 3U) = values.modelFillValue;
            return;
        case MaterialClosureModelFillMaterial::Varnish:
            layer.at(base + 5U) = values.modelFillValue;
            return;
        case MaterialClosureModelFillMaterial::None:
            return;
    }
}

}  // namespace

MaterialClosureRepairPlan BuildMaterialClosureRepairPlan(
    const MaterialClosureSemanticLayerInput& input,
    const MaterialClosureSemanticLayerAnalysis& analysis,
    const int connectivity)
{
    ValidatePlanInput(input, analysis, connectivity);
    const std::size_t pixelCount = analysis.candidateGapMask.size();
    MaterialClosureRepairPlan plan;
    plan.widthPx = input.widthPx;
    plan.heightPx = input.heightPx;
    plan.modelFillRepairMask.assign(pixelCount, 0U);
    plan.supportRepairMask.assign(pixelCount, 0U);
    plan.internalVoidSupportRepairMask.assign(pixelCount, 0U);
    plan.colorFillRepairMask.assign(pixelCount, 0U);
    plan.modelSupportRepairMask.assign(pixelCount, 0U);
    plan.varnishSupportRepairMask.assign(pixelCount, 0U);
    plan.rejectedTooWideMask.assign(pixelCount, 0U);

    std::vector<std::uint8_t> visited(pixelCount, 0U);
    std::vector<std::uint8_t> componentMask(pixelCount, 0U);
    for (std::size_t index{0U}; index < pixelCount; ++index)
    {
        if (analysis.candidateGapMask.at(index) == 0U || visited.at(index) != 0U)
        {
            continue;
        }
        if (connectivity == 8)
        {
            AddComponentToPlan(
                input,
                analysis,
                CollectComponent(
                    analysis.candidateGapMask,
                    input.widthPx,
                    input.heightPx,
                    index,
                    directions8,
                    visited),
                directions8,
                componentMask,
                plan);
        }
        else
        {
            AddComponentToPlan(
                input,
                analysis,
                CollectComponent(
                    analysis.candidateGapMask,
                    input.widthPx,
                    input.heightPx,
                    index,
                    directions4,
                    visited),
                directions4,
                componentMask,
                plan);
        }
    }

    for (std::size_t index{0U}; index < pixelCount; ++index)
    {
        plan.modelFillRepairPixels += plan.modelFillRepairMask.at(index) != 0U ? 1 : 0;
        plan.supportRepairPixels += plan.supportRepairMask.at(index) != 0U ? 1 : 0;
    }
    return plan;
}

MaterialClosureRepairApplicationResult ApplyMaterialClosureRepair(
    const MaterialClosureRepairPlan& plan,
    const MaterialClosureRepairValues& values,
    std::vector<std::uint8_t>& layer,
    MaterialClosureSemanticLayerInput& input)
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
        if (input.layerEmptyMask.at(index) == 0U)
        {
            continue;
        }
        const bool repairModelFill = plan.modelFillRepairMask.at(index) != 0U;
        const bool repairSupport = plan.supportRepairMask.at(index) != 0U;
        if (!repairModelFill && !repairSupport)
        {
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
            input.modelFillMask.at(index) = 1U;
            input.modelMaterialMask.at(index) = 1U;
            ++result.repairedModelFillPixels;
        }
        else
        {
            layer.at(base + 4U) = values.supportValue;
            input.supportFillMask.at(index) = 1U;
            ++result.repairedSupportPixels;
            if (plan.internalVoidSupportRepairMask.at(index) != 0U)
            {
                input.internalVoidSupportMask.at(index) = 1U;
            }
        }
        input.layerEmptyMask.at(index) = 0U;
        ++result.repairedPixels;
        result.repairedColorFillPixels += plan.colorFillRepairMask.at(index) != 0U ? 1 : 0;
        result.repairedModelSupportPixels += plan.modelSupportRepairMask.at(index) != 0U ? 1 : 0;
        result.repairedInternalVoidPixels +=
            plan.internalVoidSupportRepairMask.at(index) != 0U ? 1 : 0;
        result.repairedVarnishSupportPixels +=
            plan.varnishSupportRepairMask.at(index) != 0U ? 1 : 0;
    }
    return result;
}

}  // namespace slicer_core
