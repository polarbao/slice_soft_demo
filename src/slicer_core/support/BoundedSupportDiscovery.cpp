#include "slicer_core/support/BoundedSupportDiscovery.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace slicer_core
{
namespace
{

std::size_t ValidateRasterDimensions(
    const int widthPx,
    const int heightPx,
    const int layerCount)
{
    if (widthPx <= 0 || heightPx <= 0 || layerCount <= 0)
    {
        throw std::invalid_argument(
            "bounded support scan dimensions must be positive");
    }
    const std::size_t width{static_cast<std::size_t>(widthPx)};
    const std::size_t height{static_cast<std::size_t>(heightPx)};
    if (width > std::numeric_limits<std::size_t>::max() / height)
    {
        throw std::invalid_argument(
            "bounded support scan raster dimensions overflow");
    }
    const std::size_t pixelCount{width * height};
    if (pixelCount
        > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument(
            "bounded support scan raster exceeds index capacity");
    }
    return pixelCount;
}

void ValidateHeightfieldInput(const GeometryOccupancyInputKind inputKind)
{
    if (inputKind
        != GeometryOccupancyInputKind::SingleIntervalHeightfield)
    {
        throw std::invalid_argument(
            "bounded support scan requires SingleIntervalHeightfield input");
    }
}

bool ByteRangesOverlap(
    const void* const leftData,
    const std::size_t leftBytes,
    const void* const rightData,
    const std::size_t rightBytes) noexcept
{
    if (leftBytes == 0U || rightBytes == 0U)
    {
        return false;
    }
    const auto leftBegin{reinterpret_cast<std::uintptr_t>(leftData)};
    const auto rightBegin{reinterpret_cast<std::uintptr_t>(rightData)};
    if (leftBytes > std::numeric_limits<std::uintptr_t>::max() - leftBegin
        || rightBytes
            > std::numeric_limits<std::uintptr_t>::max() - rightBegin)
    {
        return true;
    }
    const std::uintptr_t leftEnd{leftBegin + leftBytes};
    const std::uintptr_t rightEnd{rightBegin + rightBytes};
    return leftBegin < rightEnd && rightBegin < leftEnd;
}

template <typename Left, typename Right>
bool SpansOverlap(
    const std::span<Left> left,
    const std::span<Right> right) noexcept
{
    return ByteRangesOverlap(
        left.data(),
        left.size_bytes(),
        right.data(),
        right.size_bytes());
}

void ValidateBinaryMask(
    const std::span<const std::uint8_t> mask,
    const char* const message)
{
    if (std::any_of(mask.begin(), mask.end(), [](const std::uint8_t value)
        {
            return value > 1U;
        }))
    {
        throw std::invalid_argument(message);
    }
}

std::size_t MaskIndex(const int widthPx, const int x, const int y) noexcept
{
    return static_cast<std::size_t>(y)
        * static_cast<std::size_t>(widthPx)
        + static_cast<std::size_t>(x);
}

void ValidateOuterVarnish(
    const BoundedOuterBoundaryScanRequest& request)
{
    const OuterVarnishDiscretization& outer{request.outerVarnish};
    if (!outer.enabled)
    {
        if (request.includeOuterVarnishInUpperBoundary)
        {
            throw std::invalid_argument(
                "upper boundary cannot include disabled outer varnish");
        }
        return;
    }
    if (!std::isfinite(outer.requested_thickness_mm)
        || !std::isfinite(outer.pixel_size_x_mm)
        || !std::isfinite(outer.pixel_size_y_mm)
        || outer.requested_thickness_mm <= 0.0
        || outer.pixel_size_x_mm <= 0.0
        || outer.pixel_size_y_mm <= 0.0
        || outer.radius_x_px <= 0
        || outer.radius_y_px <= 0)
    {
        throw std::invalid_argument(
            "bounded outer varnish discretization is invalid");
    }
}

}  // namespace

BoundedOuterBoundaryScanner::BoundedOuterBoundaryScanner(
    const BoundedOuterBoundaryScanRequest& request)
    : request_{request},
      pixelCount_{ValidateRasterDimensions(
          request.widthPx,
          request.heightPx,
          request.layerCount)}
{
    ValidateHeightfieldInput(request.inputKind);
    ValidateOuterVarnish(request);
    upperBoundaryLastLayers_.assign(pixelCount_, -1);
    externalEmptyScratch_.resize(pixelCount_);
    dilationScratch_.resize(pixelCount_);
    floodStack_.reserve(pixelCount_);
}

void BoundedOuterBoundaryScanner::ConsumeLayer(
    const int layerIndex,
    const std::span<const std::uint8_t> modelMask,
    const std::span<std::uint8_t> outputOuterVarnishMask,
    const std::span<std::uint8_t> outputUpperBoundaryMask)
{
    if (layerIndex != expectedLayerIndex_
        || layerIndex < 0
        || layerIndex >= request_.layerCount)
    {
        throw std::invalid_argument(
            "bounded outer-boundary layers must be consumed in order");
    }
    if (modelMask.size() != pixelCount_
        || outputOuterVarnishMask.size() != pixelCount_
        || outputUpperBoundaryMask.size() != pixelCount_)
    {
        throw std::invalid_argument(
            "bounded outer-boundary buffer size is invalid");
    }
    if (SpansOverlap(modelMask, outputOuterVarnishMask)
        || SpansOverlap(modelMask, outputUpperBoundaryMask)
        || SpansOverlap(
            outputOuterVarnishMask,
            outputUpperBoundaryMask))
    {
        throw std::invalid_argument(
            "bounded outer-boundary inputs and outputs must not overlap");
    }
    ValidateBinaryMask(
        modelMask,
        "bounded outer-boundary model mask must be binary");

    std::fill(
        outputOuterVarnishMask.begin(),
        outputOuterVarnishMask.end(),
        0U);
    std::fill(
        outputUpperBoundaryMask.begin(),
        outputUpperBoundaryMask.end(),
        0U);
    std::fill(
        externalEmptyScratch_.begin(),
        externalEmptyScratch_.end(),
        0U);
    std::fill(
        dilationScratch_.begin(),
        dilationScratch_.end(),
        0U);
    floodStack_.clear();

    const bool hasModel{std::any_of(
        modelMask.begin(),
        modelMask.end(),
        [](const std::uint8_t value) { return value != 0U; })};
    if (hasModel && request_.outerVarnish.enabled)
    {
        const auto pushExternal = [this, modelMask](const int x, const int y)
        {
            if (x < 0 || x >= request_.widthPx
                || y < 0 || y >= request_.heightPx)
            {
                return;
            }
            const std::size_t index{MaskIndex(request_.widthPx, x, y)};
            if (modelMask[index] != 0U
                || externalEmptyScratch_[index] != 0U)
            {
                return;
            }
            externalEmptyScratch_[index] = 1U;
            floodStack_.push_back(static_cast<int>(index));
        };

        for (int x{0}; x < request_.widthPx; ++x)
        {
            pushExternal(x, 0);
            pushExternal(x, request_.heightPx - 1);
        }
        for (int y{0}; y < request_.heightPx; ++y)
        {
            pushExternal(0, y);
            pushExternal(request_.widthPx - 1, y);
        }

        constexpr std::array<std::array<int, 2>, 8> neighbors{{
            {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}},
            {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
        }};
        while (!floodStack_.empty())
        {
            const int current{floodStack_.back()};
            floodStack_.pop_back();
            const int x{current % request_.widthPx};
            const int y{current / request_.widthPx};
            for (const auto& neighbor : neighbors)
            {
                pushExternal(x + neighbor[0], y + neighbor[1]);
            }
        }

        const int radiusX{request_.outerVarnish.radius_x_px};
        const int radiusY{request_.outerVarnish.radius_y_px};
        for (int y{0}; y < request_.heightPx; ++y)
        {
            for (int x{0}; x < request_.widthPx; ++x)
            {
                const std::size_t index{
                    MaskIndex(request_.widthPx, x, y)};
                if (modelMask[index] == 0U)
                {
                    continue;
                }
                const int minX{radiusX > x ? 0 : x - radiusX};
                const int maxX{radiusX >= request_.widthPx - 1 - x
                        ? request_.widthPx - 1
                        : x + radiusX};
                const int minY{radiusY > y ? 0 : y - radiusY};
                const int maxY{radiusY >= request_.heightPx - 1 - y
                        ? request_.heightPx - 1
                        : y + radiusY};
                for (int neighborY{minY}; neighborY <= maxY; ++neighborY)
                {
                    for (int neighborX{minX}; neighborX <= maxX; ++neighborX)
                    {
                        if (IsOuterVarnishOffsetWithinThickness(
                                request_.outerVarnish,
                                neighborX - x,
                                neighborY - y))
                        {
                            dilationScratch_[MaskIndex(
                                request_.widthPx,
                                neighborX,
                                neighborY)] = 1U;
                        }
                    }
                }
            }
        }

        for (std::size_t index{0U}; index < pixelCount_; ++index)
        {
            if (modelMask[index] == 0U
                && externalEmptyScratch_[index] != 0U
                && dilationScratch_[index] != 0U)
            {
                outputOuterVarnishMask[index] = 1U;
            }
        }
    }

    for (std::size_t index{0U}; index < pixelCount_; ++index)
    {
        const bool boundaryOccupied{modelMask[index] != 0U
            || (request_.includeOuterVarnishInUpperBoundary
                && outputOuterVarnishMask[index] != 0U)};
        if (boundaryOccupied)
        {
            outputUpperBoundaryMask[index] = 1U;
            upperBoundaryLastLayers_[index] = layerIndex;
        }
    }
    ++expectedLayerIndex_;
}

BoundedOuterBoundaryScanResult BoundedOuterBoundaryScanner::Finish() &&
{
    if (expectedLayerIndex_ != request_.layerCount)
    {
        throw std::logic_error(
            "bounded outer-boundary scan is incomplete");
    }
    BoundedOuterBoundaryScanResult result;
    result.upperBoundaryLastLayers_ =
        std::move(upperBoundaryLastLayers_);
    return result;
}

BoundedUnsupportedDiscoveryScanner::BoundedUnsupportedDiscoveryScanner(
    const BoundedUnsupportedDiscoveryRequest& request,
    const BoundedSupportDemandPlan& preliminaryPlan)
    : request_{request},
      preliminaryPlan_{&preliminaryPlan},
      pixelCount_{ValidateRasterDimensions(
          request.widthPx,
          request.heightPx,
          request.layerCount)}
{
    ValidateHeightfieldInput(request.inputKind);
    if (request.connectivity != 4 && request.connectivity != 8)
    {
        throw std::invalid_argument(
            "bounded unsupported connectivity must be 4 or 8");
    }
    if (request.xyDilationPx < 0
        || request.minIslandAreaPx < 0
        || !std::isfinite(request.minOverlapRatio)
        || request.minOverlapRatio < 0.0
        || request.minOverlapRatio > 1.0)
    {
        throw std::invalid_argument(
            "bounded unsupported discovery policy is invalid");
    }
    if (preliminaryPlan.LayerCount() != request.layerCount
        || preliminaryPlan.ColumnCount() != pixelCount_
        || preliminaryPlan.InputKind() != request.inputKind)
    {
        throw std::invalid_argument(
            "bounded unsupported preliminary plan is incompatible");
    }
    if (preliminaryPlan.UnsupportedEnabled())
    {
        throw std::invalid_argument(
            "bounded unsupported preliminary plan must exclude unsupported demand");
    }

    unsupportedTopExclusiveLayers_.assign(pixelCount_, 0);
    layerEvents_.resize(static_cast<std::size_t>(request.layerCount));
    for (int layerIndex{0}; layerIndex < request.layerCount; ++layerIndex)
    {
        layerEvents_[static_cast<std::size_t>(layerIndex)].layerIndex =
            layerIndex;
    }
    previousModelMask_.resize(pixelCount_);
    previousUpperBoundaryMask_.resize(pixelCount_);
    preliminarySupportMask_.resize(pixelCount_);
    preliminarySupportTypes_.resize(pixelCount_);
    baseScratchA_.resize(pixelCount_);
    baseScratchB_.resize(pixelCount_);
    visitedScratch_.resize(pixelCount_);
    traversalStack_.reserve(pixelCount_);
    componentPixels_.reserve(pixelCount_);
}

void BoundedUnsupportedDiscoveryScanner::ConsumeLayer(
    const int layerIndex,
    const std::span<const std::uint8_t> modelMask,
    const std::span<const std::uint8_t> upperBoundaryMask)
{
    if (layerIndex != expectedLayerIndex_
        || layerIndex < 0
        || layerIndex >= request_.layerCount)
    {
        throw std::invalid_argument(
            "bounded unsupported layers must be consumed in order");
    }
    if (modelMask.size() != pixelCount_
        || upperBoundaryMask.size() != pixelCount_)
    {
        throw std::invalid_argument(
            "bounded unsupported input buffer size is invalid");
    }
    ValidateBinaryMask(
        modelMask,
        "bounded unsupported model mask must be binary");
    ValidateBinaryMask(
        upperBoundaryMask,
        "bounded unsupported upper-boundary mask must be binary");
    for (std::size_t index{0U}; index < pixelCount_; ++index)
    {
        if (modelMask[index] != 0U && upperBoundaryMask[index] == 0U)
        {
            throw std::invalid_argument(
                "bounded unsupported upper boundary must contain the model");
        }
    }

    if (layerIndex > 0 && request_.enabled)
    {
        MaterializePreShapeSupportLayer(
            *preliminaryPlan_,
            layerIndex - 1,
            previousModelMask_,
            previousUpperBoundaryMask_,
            preliminarySupportMask_,
            preliminarySupportTypes_);
        for (std::size_t index{0U}; index < pixelCount_; ++index)
        {
            baseScratchA_[index] = previousModelMask_[index] != 0U
                    || preliminarySupportMask_[index] != 0U
                ? 1U
                : 0U;
        }

        std::vector<std::uint8_t>* currentBase{&baseScratchA_};
        std::vector<std::uint8_t>* nextBase{&baseScratchB_};
        for (int iteration{0};
             iteration < request_.xyDilationPx;
             ++iteration)
        {
            std::copy(
                currentBase->begin(),
                currentBase->end(),
                nextBase->begin());
            for (int y{0}; y < request_.heightPx; ++y)
            {
                for (int x{0}; x < request_.widthPx; ++x)
                {
                    const std::size_t index{
                        MaskIndex(request_.widthPx, x, y)};
                    if ((*currentBase)[index] == 0U)
                    {
                        continue;
                    }
                    for (int deltaY{-1}; deltaY <= 1; ++deltaY)
                    {
                        for (int deltaX{-1}; deltaX <= 1; ++deltaX)
                        {
                            const int neighborX{x + deltaX};
                            const int neighborY{y + deltaY};
                            if (neighborX >= 0
                                && neighborX < request_.widthPx
                                && neighborY >= 0
                                && neighborY < request_.heightPx)
                            {
                                (*nextBase)[MaskIndex(
                                    request_.widthPx,
                                    neighborX,
                                    neighborY)] = 1U;
                            }
                        }
                    }
                }
            }
            std::swap(currentBase, nextBase);
        }

        std::fill(
            visitedScratch_.begin(),
            visitedScratch_.end(),
            0U);
        BoundedUnsupportedLayerEvent event;
        event.layerIndex = layerIndex;
        constexpr std::array<std::array<int, 2>, 8> neighbors8{{
            {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}},
            {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
        }};
        constexpr std::array<std::array<int, 2>, 4> neighbors4{{
            {{0, -1}}, {{-1, 0}}, {{1, 0}}, {{0, 1}},
        }};

        for (std::size_t start{0U}; start < pixelCount_; ++start)
        {
            if (modelMask[start] == 0U
                || visitedScratch_[start] != 0U)
            {
                continue;
            }
            traversalStack_.clear();
            componentPixels_.clear();
            traversalStack_.push_back(static_cast<int>(start));
            visitedScratch_[start] = 1U;
            int overlapPixels{0};
            while (!traversalStack_.empty())
            {
                const int current{traversalStack_.back()};
                traversalStack_.pop_back();
                componentPixels_.push_back(current);
                if ((*currentBase)[static_cast<std::size_t>(current)] != 0U)
                {
                    ++overlapPixels;
                }
                const int x{current % request_.widthPx};
                const int y{current / request_.widthPx};
                const auto visitNeighbor = [this, modelMask](
                    const int neighborX,
                    const int neighborY)
                {
                    if (neighborX < 0 || neighborX >= request_.widthPx
                        || neighborY < 0 || neighborY >= request_.heightPx)
                    {
                        return;
                    }
                    const std::size_t next{MaskIndex(
                        request_.widthPx,
                        neighborX,
                        neighborY)};
                    if (modelMask[next] != 0U
                        && visitedScratch_[next] == 0U)
                    {
                        visitedScratch_[next] = 1U;
                        traversalStack_.push_back(static_cast<int>(next));
                    }
                };
                if (request_.connectivity == 8)
                {
                    for (const auto& neighbor : neighbors8)
                    {
                        visitNeighbor(x + neighbor[0], y + neighbor[1]);
                    }
                }
                else
                {
                    for (const auto& neighbor : neighbors4)
                    {
                        visitNeighbor(x + neighbor[0], y + neighbor[1]);
                    }
                }
            }

            const int areaPixels{
                static_cast<int>(componentPixels_.size())};
            const double overlapRatio{areaPixels > 0
                ? static_cast<double>(overlapPixels)
                    / static_cast<double>(areaPixels)
                : 0.0};
            if (overlapRatio >= request_.minOverlapRatio)
            {
                continue;
            }
            if (areaPixels < request_.minIslandAreaPx)
            {
                ++event.filteredIslandCount;
                event.filteredIslandPixels += areaPixels;
                continue;
            }
            ++event.islandCount;
            event.islandPixels += areaPixels;
            event.unsupportedPixels += areaPixels;
            for (const int pixel : componentPixels_)
            {
                const std::size_t index{static_cast<std::size_t>(pixel)};
                unsupportedTopExclusiveLayers_[index] = std::max(
                    unsupportedTopExclusiveLayers_[index],
                    layerIndex);
            }
        }

        layerEvents_[static_cast<std::size_t>(layerIndex)] = event;
        if (event.islandCount > 0 || event.filteredIslandCount > 0)
        {
            ++totals_.layersWithIslands;
        }
        totals_.islandCount += event.islandCount;
        totals_.islandPixels += event.islandPixels;
        totals_.unsupportedPixels += event.unsupportedPixels;
        totals_.filteredIslandCount += event.filteredIslandCount;
        totals_.filteredIslandPixels += event.filteredIslandPixels;
    }

    std::copy(
        modelMask.begin(),
        modelMask.end(),
        previousModelMask_.begin());
    std::copy(
        upperBoundaryMask.begin(),
        upperBoundaryMask.end(),
        previousUpperBoundaryMask_.begin());
    ++expectedLayerIndex_;
}

BoundedUnsupportedDiscoveryResult
BoundedUnsupportedDiscoveryScanner::Finish() &&
{
    if (expectedLayerIndex_ != request_.layerCount)
    {
        throw std::logic_error(
            "bounded unsupported discovery is incomplete");
    }
    BoundedUnsupportedDiscoveryResult result;
    result.unsupportedTopExclusiveLayers_ =
        std::move(unsupportedTopExclusiveLayers_);
    result.layerEvents_ = std::move(layerEvents_);
    result.totals_ = totals_;
    return result;
}

}  // namespace slicer_core
