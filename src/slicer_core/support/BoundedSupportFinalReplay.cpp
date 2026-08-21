#include "slicer_core/support/BoundedSupportFinalReplay.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace slicer_core
{
namespace
{

[[nodiscard]] std::size_t CheckedPixelCount(
    const int widthPx,
    const int heightPx)
{
    if (widthPx <= 0 || heightPx <= 0)
    {
        throw std::invalid_argument(
            "bounded support final replay dimensions must be positive");
    }
    const std::size_t width{static_cast<std::size_t>(widthPx)};
    const std::size_t height{static_cast<std::size_t>(heightPx)};
    if (width > std::numeric_limits<std::size_t>::max() / height)
    {
        throw std::overflow_error(
            "bounded support final replay pixel count overflow");
    }
    const std::size_t pixelCount{width * height};
    if (pixelCount
        > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument(
            "bounded support final replay exceeds index capacity");
    }
    return pixelCount;
}

template <typename Left, typename Right>
[[nodiscard]] bool ByteRangesOverlap(
    const std::span<Left> left,
    const std::span<Right> right) noexcept
{
    if (left.empty() || right.empty())
    {
        return false;
    }
    const auto leftBegin{reinterpret_cast<std::uintptr_t>(left.data())};
    const auto rightBegin{reinterpret_cast<std::uintptr_t>(right.data())};
    if (left.size_bytes()
            > std::numeric_limits<std::uintptr_t>::max() - leftBegin
        || right.size_bytes()
            > std::numeric_limits<std::uintptr_t>::max() - rightBegin)
    {
        return true;
    }
    const std::uintptr_t leftEnd{leftBegin + left.size_bytes()};
    const std::uintptr_t rightEnd{rightBegin + right.size_bytes()};
    return leftBegin < rightEnd && rightBegin < leftEnd;
}

void ValidateBinaryMask(
    const std::span<const std::uint8_t> mask,
    const std::string_view name)
{
    if (std::any_of(mask.begin(), mask.end(), [](const std::uint8_t value)
        {
            return value > 1U;
        }))
    {
        throw std::invalid_argument(std::string(name) + " must be binary");
    }
}

void ValidateBaseProjection(const SupportBaseProjectionConfig& config)
{
    if (config.layer_count < 0 || config.layer_count > 1000)
    {
        throw std::invalid_argument(
            "bounded support base layer count is invalid");
    }
    if (config.source != "max_support_footprint")
    {
        throw std::invalid_argument(
            "bounded support base source is invalid");
    }
    if (config.layer_placement != "overlay_existing"
        && config.layer_placement != "prepend_below_model")
    {
        throw std::invalid_argument(
            "bounded support base layer placement is invalid");
    }
}

void ValidateLayerArguments(
    const int expectedLayerIndex,
    const int layerIndex,
    const std::size_t pixelCount,
    const std::span<const std::uint8_t> modelMask,
    const std::span<const std::uint8_t> upperBoundaryMask,
    const std::span<const std::uint8_t> outerVarnishMask,
    const std::span<std::uint8_t> outputSupportMask,
    const std::span<SupportType> outputTypeMap,
    const std::span<std::uint8_t> outputClearedMask)
{
    if (layerIndex != expectedLayerIndex)
    {
        throw std::invalid_argument(
            "bounded support final layers must be consumed in order");
    }
    if (modelMask.size() != pixelCount
        || upperBoundaryMask.size() != pixelCount
        || outerVarnishMask.size() != pixelCount
        || outputSupportMask.size() != pixelCount
        || outputTypeMap.size() != pixelCount
        || outputClearedMask.size() != pixelCount)
    {
        throw std::invalid_argument(
            "bounded support final layer dimensions do not match");
    }
    ValidateBinaryMask(modelMask, "modelMask");
    ValidateBinaryMask(upperBoundaryMask, "upperBoundaryMask");
    ValidateBinaryMask(outerVarnishMask, "outerVarnishMask");

    const bool aliases =
        ByteRangesOverlap(modelMask, outputSupportMask)
        || ByteRangesOverlap(modelMask, outputTypeMap)
        || ByteRangesOverlap(modelMask, outputClearedMask)
        || ByteRangesOverlap(upperBoundaryMask, outputSupportMask)
        || ByteRangesOverlap(upperBoundaryMask, outputTypeMap)
        || ByteRangesOverlap(upperBoundaryMask, outputClearedMask)
        || ByteRangesOverlap(outerVarnishMask, outputSupportMask)
        || ByteRangesOverlap(outerVarnishMask, outputTypeMap)
        || ByteRangesOverlap(outerVarnishMask, outputClearedMask)
        || ByteRangesOverlap(outputSupportMask, outputTypeMap)
        || ByteRangesOverlap(outputSupportMask, outputClearedMask)
        || ByteRangesOverlap(outputTypeMap, outputClearedMask);
    if (aliases)
    {
        throw std::invalid_argument(
            "bounded support final outputs must not alias inputs or each other");
    }
}

[[nodiscard]] std::size_t MaskIndex(
    const int width,
    const int x,
    const int y) noexcept
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x);
}

BoundedSupportConnectivityStats AnalyzeConnectivity(
    const std::vector<std::uint8_t>& supportMask,
    const int width,
    const int height,
    const int connectivity,
    std::vector<std::uint8_t>& visited,
    std::vector<int>& stack,
    std::vector<BoundedSupportComponentSummary>& components)
{
    constexpr int tinyComponentAreaPx{8};
    constexpr int smallComponentAreaPx{512};
    constexpr std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}},
        {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};
    constexpr std::array<std::array<int, 2>, 4> neighbors4{{
        {{0, -1}}, {{-1, 0}}, {{1, 0}}, {{0, 1}},
    }};
    std::fill(visited.begin(), visited.end(), static_cast<std::uint8_t>(0U));
    stack.clear();
    components.clear();
    BoundedSupportConnectivityStats stats;
    for (std::size_t start{0U}; start < supportMask.size(); ++start)
    {
        if (supportMask[start] == 0U || visited[start] != 0U)
        {
            continue;
        }
        int area{0};
        BoundedSupportComponentSummary component;
        component.minX = width;
        component.minY = height;
        component.maxX = -1;
        component.maxY = -1;
        visited[start] = 1U;
        stack.push_back(static_cast<int>(start));
        while (!stack.empty())
        {
            const int current{stack.back()};
            stack.pop_back();
            ++area;
            const int x{current % width};
            const int y{current / width};
            component.minX = std::min(component.minX, x);
            component.minY = std::min(component.minY, y);
            component.maxX = std::max(component.maxX, x);
            component.maxY = std::max(component.maxY, y);
            const auto visit = [&](const int nx, const int ny)
            {
                if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                {
                    return;
                }
                const std::size_t next{MaskIndex(width, nx, ny)};
                if (supportMask[next] != 0U && visited[next] == 0U)
                {
                    visited[next] = 1U;
                    stack.push_back(static_cast<int>(next));
                }
            };
            if (connectivity == 8)
            {
                for (const auto& delta : neighbors8)
                {
                    visit(x + delta[0], y + delta[1]);
                }
            }
            else
            {
                for (const auto& delta : neighbors4)
                {
                    visit(x + delta[0], y + delta[1]);
                }
            }
        }
        ++stats.componentCount;
        component.areaPx = area;
        components.push_back(component);
        stats.largestComponentPixels =
            std::max(stats.largestComponentPixels, area);
        if (area <= tinyComponentAreaPx)
        {
            ++stats.tinyComponentCount;
        }
        else if (area <= smallComponentAreaPx)
        {
            ++stats.smallComponentCount;
        }
    }
    std::sort(
        components.begin(),
        components.end(),
        [](const BoundedSupportComponentSummary& left,
           const BoundedSupportComponentSummary& right)
        {
            return left.areaPx > right.areaPx;
        });
    stats.components = components;
    return stats;
}

}  // namespace

BoundedSupportFinalReplayScanner::BoundedSupportFinalReplayScanner(
    const BoundedSupportFinalReplayRequest& request,
    const BoundedSupportDemandPlan& finalPlan,
    const BoundedSupportShapeScanResult& verifiedShapeResult,
    BoundedSupportFinalLayerSink* const layerSink)
    : request_(request),
      verifiedShapeResult_(&verifiedShapeResult),
      layerSink_(layerSink),
      pixelCount_(CheckedPixelCount(
          request.shapeReplay.widthPx,
          request.shapeReplay.heightPx)),
      shapeScanner_(request.shapeReplay, finalPlan)
{
    ValidateBaseProjection(request.baseProjection);
    if (shapeScanner_.ReplayIdentity()
            != verifiedShapeResult.ReplayIdentity()
        || verifiedShapeResult.SupportFootprint().size() != pixelCount_
        || verifiedShapeResult.ReplayDigests().size()
            != static_cast<std::size_t>(request.shapeReplay.layerCount))
    {
        throw std::invalid_argument(
            "bounded support final replay requires matching B3 evidence");
    }

    baseProjection_.enabled = request.baseProjection.enabled;
    baseProjection_.configuredLayerCount = request.baseProjection.layer_count;
    baseProjection_.effectiveLayerCount =
        request.baseProjection.enabled
        ? std::min(
            request.baseProjection.layer_count,
            request.shapeReplay.layerCount)
        : 0;
    baseProjection_.layerPlacement = request.baseProjection.layer_placement;
    if (baseProjection_.effectiveLayerCount > 0)
    {
        baseProjection_.footprintPixels =
            verifiedShapeResult.FootprintPixels();
    }
    supportScratch_.resize(pixelCount_);
    typeScratch_.resize(pixelCount_);
    clearedScratch_.resize(pixelCount_);
    visitedScratch_.resize(pixelCount_);
    traversalStack_.reserve(pixelCount_);
}

void BoundedSupportFinalReplayScanner::ConsumeLayer(
    const int layerIndex,
    const std::span<const std::uint8_t> modelMask,
    const std::span<const std::uint8_t> upperBoundaryMask,
    const std::span<const std::uint8_t> outerVarnishMask,
    const std::span<std::uint8_t> outputSupportMask,
    const std::span<SupportType> outputTypeMap,
    const std::span<std::uint8_t> outputClearedOuterOverlapMask)
{
    if (failed_ || finished_)
    {
        throw std::logic_error(
            "bounded support final replay scanner is not active");
    }
    ValidateLayerArguments(
        expectedLayerIndex_,
        layerIndex,
        pixelCount_,
        modelMask,
        upperBoundaryMask,
        outerVarnishMask,
        outputSupportMask,
        outputTypeMap,
        outputClearedOuterOverlapMask);

    const auto expectedDigests{verifiedShapeResult_->ReplayDigests()};
    shapeScanner_.ConsumeVerifiedLayer(
        layerIndex,
        modelMask,
        upperBoundaryMask,
        expectedDigests[static_cast<std::size_t>(layerIndex)],
        supportScratch_,
        typeScratch_);

    std::fill(
        clearedScratch_.begin(),
        clearedScratch_.end(),
        static_cast<std::uint8_t>(0U));
    if (layerIndex < baseProjection_.effectiveLayerCount)
    {
        const auto footprint{verifiedShapeResult_->SupportFootprint()};
        for (std::size_t index{0U}; index < pixelCount_; ++index)
        {
            if (footprint[index] != 0U
                && modelMask[index] == 0U
                && supportScratch_[index] == 0U)
            {
                supportScratch_[index] = 1U;
                ++baseProjection_.addedSupportPixels;
            }
        }
        for (std::size_t index{0U}; index < pixelCount_; ++index)
        {
            if (supportScratch_[index] != 0U
                && (baseProjection_.layerPlacement == "prepend_below_model"
                    || typeScratch_[index] == SupportType::None))
            {
                typeScratch_[index] = SupportType::ProjectionBase;
            }
        }
    }

    std::uint64_t clearedPixels{0U};
    for (std::size_t index{0U}; index < pixelCount_; ++index)
    {
        if (outerVarnishMask[index] != 0U
            && supportScratch_[index] != 0U)
        {
            supportScratch_[index] = 0U;
            typeScratch_[index] = SupportType::None;
            clearedScratch_[index] = 1U;
            ++clearedPixels;
        }
    }

    BoundedSupportFinalLayerStats layerStats;
    layerStats.layerIndex = layerIndex;
    for (std::size_t index{0U}; index < pixelCount_; ++index)
    {
        if (supportScratch_[index] == 0U)
        {
            continue;
        }
        ++layerStats.supportPixels;
        const std::size_t typeIndex{
            static_cast<std::size_t>(typeScratch_[index])};
        if (typeIndex >= layerStats.typePixels.size())
        {
            failed_ = true;
            throw std::logic_error(
                "bounded support final replay produced an invalid type");
        }
        ++layerStats.typePixels[typeIndex];
    }
    layerStats.connectivity = AnalyzeConnectivity(
        supportScratch_,
        request_.shapeReplay.widthPx,
        request_.shapeReplay.heightPx,
        request_.shapeReplay.connectivity,
        visitedScratch_,
        traversalStack_,
        componentScratch_);

    if (layerSink_ != nullptr)
    {
        try
        {
            layerSink_->Consume(layerStats);
        }
        catch (...)
        {
            failed_ = true;
            throw;
        }
    }

    std::copy(
        supportScratch_.begin(),
        supportScratch_.end(),
        outputSupportMask.begin());
    std::copy(
        typeScratch_.begin(),
        typeScratch_.end(),
        outputTypeMap.begin());
    std::copy(
        clearedScratch_.begin(),
        clearedScratch_.end(),
        outputClearedOuterOverlapMask.begin());

    ++totals_.completedLayerCount;
    totals_.supportPixels += layerStats.supportPixels;
    totals_.clearedOuterVarnishSupportPixels += clearedPixels;
    if (layerStats.supportPixels > 0U)
    {
        ++totals_.layersWithSupport;
    }
    for (std::size_t index{0U}; index < totals_.typePixels.size(); ++index)
    {
        totals_.typePixels[index] += layerStats.typePixels[index];
    }
    totals_.connectivityComponentCount += static_cast<std::uint64_t>(
        layerStats.connectivity.componentCount);
    totals_.largestConnectivityComponentPixels = std::max(
        totals_.largestConnectivityComponentPixels,
        layerStats.connectivity.largestComponentPixels);
    totals_.smallConnectivityComponentCount += static_cast<std::uint64_t>(
        layerStats.connectivity.smallComponentCount);
    totals_.tinyConnectivityComponentCount += static_cast<std::uint64_t>(
        layerStats.connectivity.tinyComponentCount);
    ++expectedLayerIndex_;
}

BoundedSupportFinalReplayResult
BoundedSupportFinalReplayScanner::Finish() &&
{
    if (failed_ || finished_)
    {
        throw std::logic_error(
            "bounded support final replay scanner is not active");
    }
    if (expectedLayerIndex_ != request_.shapeReplay.layerCount)
    {
        throw std::logic_error(
            "bounded support final replay is incomplete");
    }
    try
    {
        static_cast<void>(std::move(shapeScanner_).Finish());
        BoundedSupportFinalReplayResult result;
        result.baseProjection_ = std::move(baseProjection_);
        result.totals_ = totals_;
        finished_ = true;
        return result;
    }
    catch (...)
    {
        failed_ = true;
        throw;
    }
}

}  // namespace slicer_core
