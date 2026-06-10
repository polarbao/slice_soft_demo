#include "slicer_core/support/SupportShapeOptimizer.h"

#include <algorithm>
#include <cmath>

namespace slicer_core
{
namespace
{

std::size_t MaskIndex(const int width, const int x, const int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

bool CanWriteSupportPixel(
    const std::vector<std::uint8_t>& modelMask,
    const std::vector<std::uint8_t>& supportMask,
    const std::size_t index)
{
    return modelMask.at(index) == 0 && supportMask.at(index) == 0;
}

int CountSupportPixels(const std::vector<std::uint8_t>& mask)
{
    return static_cast<int>(std::count_if(mask.begin(), mask.end(), [](const std::uint8_t value)
    {
        return value != 0;
    }));
}

void AddPixelIfAllowed(
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<std::uint8_t>& addedMask,
    const std::size_t index)
{
    if (CanWriteSupportPixel(modelMask, supportMask, index))
    {
        supportMask.at(index) = 1;
        addedMask.at(index) = 1;
    }
}

void DilateSupport(
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<std::uint8_t>& addedMask,
    const int width,
    const int height,
    const int radius)
{
    if (radius <= 0)
    {
        return;
    }
    const std::vector<std::uint8_t> source = supportMask;
    for (int y{0}; y < height; ++y)
    {
        for (int x{0}; x < width; ++x)
        {
            const std::size_t sourceIndex = MaskIndex(width, x, y);
            if (source.at(sourceIndex) == 0)
            {
                continue;
            }
            for (int dy{-radius}; dy <= radius; ++dy)
            {
                for (int dx{-radius}; dx <= radius; ++dx)
                {
                    const int nx{x + dx};
                    const int ny{y + dy};
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                    {
                        continue;
                    }
                    AddPixelIfAllowed(modelMask, supportMask, addedMask, MaskIndex(width, nx, ny));
                }
            }
        }
    }
}

void FillSimpleClosingGaps(
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<std::uint8_t>& addedMask,
    const int width,
    const int height,
    const int radius)
{
    if (radius <= 0)
    {
        return;
    }
    const std::vector<std::uint8_t> source = supportMask;
    for (int y{0}; y < height; ++y)
    {
        for (int x{0}; x < width; ++x)
        {
            const std::size_t index = MaskIndex(width, x, y);
            if (!CanWriteSupportPixel(modelMask, supportMask, index))
            {
                continue;
            }
            bool left{false};
            bool right{false};
            bool up{false};
            bool down{false};
            for (int step{1}; step <= radius; ++step)
            {
                left = left || (x - step >= 0 && source.at(MaskIndex(width, x - step, y)) != 0);
                right = right || (x + step < width && source.at(MaskIndex(width, x + step, y)) != 0);
                up = up || (y - step >= 0 && source.at(MaskIndex(width, x, y - step)) != 0);
                down = down || (y + step < height && source.at(MaskIndex(width, x, y + step)) != 0);
            }
            if ((left && right) || (up && down))
            {
                AddPixelIfAllowed(modelMask, supportMask, addedMask, index);
            }
        }
    }
}

void BridgeHorizontalGaps(
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<std::uint8_t>& addedMask,
    SupportShapeLayerReport& report,
    const int width,
    const int height,
    const int maxGap)
{
    if (maxGap <= 0)
    {
        return;
    }
    for (int y{0}; y < height; ++y)
    {
        int x{0};
        while (x < width)
        {
            if (supportMask.at(MaskIndex(width, x, y)) == 0)
            {
                ++x;
                continue;
            }
            const int left = x;
            int gapStart{x + 1};
            while (gapStart < width && supportMask.at(MaskIndex(width, gapStart, y)) != 0)
            {
                ++gapStart;
            }
            int gapEnd = gapStart;
            while (gapEnd < width && supportMask.at(MaskIndex(width, gapEnd, y)) == 0)
            {
                ++gapEnd;
            }
            const int gap = gapEnd - gapStart;
            if (gap > 0 && gap <= maxGap && gapEnd < width)
            {
                bool canBridge{true};
                for (int bx{gapStart}; bx < gapEnd; ++bx)
                {
                    if (modelMask.at(MaskIndex(width, bx, y)) != 0)
                    {
                        canBridge = false;
                    }
                }
                if (canBridge)
                {
                    for (int bx{gapStart}; bx < gapEnd; ++bx)
                    {
                        AddPixelIfAllowed(modelMask, supportMask, addedMask, MaskIndex(width, bx, y));
                    }
                    report.bridged_gaps.push_back(BridgedSupportGap{
                        report.layer_index,
                        left,
                        y,
                        gapEnd,
                        y,
                        gap,
                        "horizontal",
                    });
                }
            }
            x = std::max(gapEnd, x + 1);
        }
    }
}

void BridgeVerticalGaps(
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<std::uint8_t>& addedMask,
    SupportShapeLayerReport& report,
    const int width,
    const int height,
    const int maxGap)
{
    if (maxGap <= 0)
    {
        return;
    }
    for (int x{0}; x < width; ++x)
    {
        int y{0};
        while (y < height)
        {
            if (supportMask.at(MaskIndex(width, x, y)) == 0)
            {
                ++y;
                continue;
            }
            const int top = y;
            int gapStart{y + 1};
            while (gapStart < height && supportMask.at(MaskIndex(width, x, gapStart)) != 0)
            {
                ++gapStart;
            }
            int gapEnd = gapStart;
            while (gapEnd < height && supportMask.at(MaskIndex(width, x, gapEnd)) == 0)
            {
                ++gapEnd;
            }
            const int gap = gapEnd - gapStart;
            if (gap > 0 && gap <= maxGap && gapEnd < height)
            {
                bool canBridge{true};
                for (int by{gapStart}; by < gapEnd; ++by)
                {
                    if (modelMask.at(MaskIndex(width, x, by)) != 0)
                    {
                        canBridge = false;
                    }
                }
                if (canBridge)
                {
                    for (int by{gapStart}; by < gapEnd; ++by)
                    {
                        AddPixelIfAllowed(modelMask, supportMask, addedMask, MaskIndex(width, x, by));
                    }
                    report.bridged_gaps.push_back(BridgedSupportGap{
                        report.layer_index,
                        x,
                        top,
                        x,
                        gapEnd,
                        gap,
                        "vertical",
                    });
                }
            }
            y = std::max(gapEnd, y + 1);
        }
    }
}

}  // namespace

SupportShapeOptimizationResult OptimizeSupportShape(
    const SupportShapePolicy& policy,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    std::vector<std::vector<std::uint8_t>>& supportMasks,
    const int width,
    const int height,
    const int connectivity)
{
    SupportShapeOptimizationResult result;
    result.enabled = policy.enabled;
    if (!policy.enabled)
    {
        return result;
    }

    for (std::size_t layerIndex{0}; layerIndex < supportMasks.size(); ++layerIndex)
    {
        SupportShapeLayerReport layerReport;
        layerReport.layer_index = static_cast<int>(layerIndex);
        std::vector<std::uint8_t>& supportMask = supportMasks.at(layerIndex);
        const std::vector<std::uint8_t>& modelMask = modelMasks.at(layerIndex);
        const std::vector<std::uint8_t> originalMask = supportMask;
        layerReport.pre = AnalyzeSupportComponents(supportMask, width, height, connectivity);

        for (const SupportComponentInfo& component : layerReport.pre.components)
        {
            if (policy.min_component_area_px > 0 && component.area_px < policy.min_component_area_px)
            {
                for (const int pixel : component.pixels)
                {
                    supportMask.at(static_cast<std::size_t>(pixel)) = 0;
                }
                layerReport.filtered_components.push_back(FilteredSupportComponent{
                    layerReport.layer_index,
                    component.area_px,
                    component.min_x,
                    component.min_y,
                    component.max_x,
                    component.max_y,
                });
            }
        }

        std::vector<std::uint8_t> addedMask(supportMask.size(), 0);
        DilateSupport(modelMask, supportMask, addedMask, width, height, policy.xy_dilation_px);
        FillSimpleClosingGaps(modelMask, supportMask, addedMask, width, height, policy.closing_radius_px);
        BridgeHorizontalGaps(modelMask, supportMask, addedMask, layerReport, width, height, policy.bridge_gap_px);
        BridgeVerticalGaps(modelMask, supportMask, addedMask, layerReport, width, height, policy.bridge_gap_px);

        const int preSupportPixels = CountSupportPixels(originalMask);
        layerReport.added_support_pixels = 0;
        layerReport.removed_support_pixels = 0;
        for (std::size_t i{0}; i < supportMask.size(); ++i)
        {
            if (originalMask.at(i) == 0 && supportMask.at(i) != 0)
            {
                ++layerReport.added_support_pixels;
            }
            if (originalMask.at(i) != 0 && supportMask.at(i) == 0)
            {
                ++layerReport.removed_support_pixels;
            }
            if (policy.preserve_model_priority && modelMask.at(i) != 0)
            {
                supportMask.at(i) = 0;
            }
        }

        const int maxAdded = static_cast<int>(std::floor(static_cast<double>(preSupportPixels) * policy.max_added_support_ratio));
        if (preSupportPixels > 0 && layerReport.added_support_pixels > maxAdded)
        {
            for (std::size_t i{0}; i < supportMask.size(); ++i)
            {
                if (originalMask.at(i) == 0 && supportMask.at(i) != 0)
                {
                    supportMask.at(i) = 0;
                }
            }
            layerReport.warnings.push_back("added support pixels exceeded maxAddedSupportRatio; additions were reverted");
            result.warnings.push_back("layer " + std::to_string(layerReport.layer_index)
                + ": added support pixels exceeded maxAddedSupportRatio");
            layerReport.added_support_pixels = 0;
        }

        layerReport.post = AnalyzeSupportComponents(supportMask, width, height, connectivity);
        result.added_support_pixels += layerReport.added_support_pixels;
        result.removed_support_pixels += layerReport.removed_support_pixels;
        if (layerReport.pre.component_count > 0 || layerReport.post.component_count > 0
            || layerReport.added_support_pixels > 0 || layerReport.removed_support_pixels > 0
            || !layerReport.bridged_gaps.empty() || !layerReport.filtered_components.empty())
        {
            result.layers.push_back(std::move(layerReport));
        }
    }

    return result;
}

}  // namespace slicer_core
