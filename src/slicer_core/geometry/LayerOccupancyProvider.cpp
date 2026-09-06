#include "slicer_core/geometry/LayerOccupancyProvider.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr double kLayerBoundaryToleranceMm{1.0e-9};
constexpr std::size_t kSupersample2x2Count{4U};

void ValidateColumn(const GeometryOccupancyColumn& column)
{
    if (!column.occupied)
    {
        return;
    }
    if (!std::isfinite(column.minimumZMm) || !std::isfinite(column.maximumZMm)
        || column.minimumZMm > column.maximumZMm)
    {
        throw std::invalid_argument("GeometryOccupancyColumn must contain a finite ordered interval");
    }
}

void ValidateRequest(const LayerOccupancyRequest& request)
{
    if (request.inputKind != GeometryOccupancyInputKind::SingleIntervalHeightfield
        && request.inputKind != GeometryOccupancyInputKind::GeneralMesh)
    {
        throw std::invalid_argument("Geometry occupancy input kind is unsupported");
    }
    if (request.layerCount <= 0)
    {
        throw std::invalid_argument("LayerOccupancyRequest layerCount must be positive");
    }
    if (!std::isfinite(request.layerThicknessMm) || request.layerThicknessMm <= 0.0)
    {
        throw std::invalid_argument("LayerOccupancyRequest layerThicknessMm must be positive and finite");
    }
    ValidateLayerOccupancyPolicy(request.policy);
    if (request.policy.layerMode == LayerOccupancyMode::LayerSlabCoverage
        && request.inputKind != GeometryOccupancyInputKind::SingleIntervalHeightfield)
    {
        throw std::invalid_argument(
            "LayerSlabCoverage requires a single-interval heightfield input");
    }
    if (request.policy.xyMode == XyCoverageMode::PixelCenter
        && !request.coverageSubsampleColumns.empty())
    {
        throw std::invalid_argument("PixelCenter does not accept coverage subsample columns");
    }
    if (request.policy.xyMode == XyCoverageMode::Supersample2x2
        && (request.columns.size()
                > std::numeric_limits<std::size_t>::max()
                    / kSupersample2x2Count
            || request.coverageSubsampleColumns.size()
                != request.columns.size() * kSupersample2x2Count))
    {
        throw std::invalid_argument("Supersample2x2 requires exactly four samples per output column");
    }
    for (const GeometryOccupancyColumn& column : request.columns)
    {
        ValidateColumn(column);
    }
    for (const GeometryOccupancyColumn& column : request.coverageSubsampleColumns)
    {
        ValidateColumn(column);
    }
}

int FirstLayerAtOrAboveZ(const double zMm, const double layerThicknessMm)
{
    return static_cast<int>(std::ceil(zMm / layerThicknessMm - 0.5));
}

int LastLayerAtOrBelowZ(const double zMm, const double layerThicknessMm)
{
    return static_cast<int>(std::floor(zMm / layerThicknessMm - 0.5));
}

double SnapToLayerBoundary(const double zMm, const double layerThicknessMm)
{
    const double boundaryIndex{std::round(zMm / layerThicknessMm)};
    const double boundaryMm{boundaryIndex * layerThicknessMm};
    return std::abs(zMm - boundaryMm) <= kLayerBoundaryToleranceMm
        ? boundaryMm
        : zMm;
}

int FirstLayerIntersectingSlab(const double zMm, const double layerThicknessMm)
{
    const double snappedZ{SnapToLayerBoundary(zMm, layerThicknessMm)};
    return static_cast<int>(std::floor(snappedZ / layerThicknessMm));
}

int LastLayerIntersectingSlab(const double zMm, const double layerThicknessMm)
{
    const double snappedZ{SnapToLayerBoundary(zMm, layerThicknessMm)};
    return static_cast<int>(std::ceil(snappedZ / layerThicknessMm)) - 1;
}

std::pair<int, int> FindOccupiedLayerRange(
    const GeometryOccupancyColumn& column,
    const LayerOccupancyMode layerMode,
    const int layerCount,
    const double layerThicknessMm)
{
    if (!column.occupied)
    {
        return {-1, -1};
    }
    if (layerMode == LayerOccupancyMode::LayerSlabCoverage
        && column.maximumZMm - column.minimumZMm <= kLayerBoundaryToleranceMm)
    {
        return {-1, -1};
    }

    int firstLayer{0};
    int lastLayer{-1};
    if (layerMode == LayerOccupancyMode::LayerSlabCoverage)
    {
        firstLayer = FirstLayerIntersectingSlab(column.minimumZMm, layerThicknessMm);
        lastLayer = LastLayerIntersectingSlab(column.maximumZMm, layerThicknessMm);
    }
    else
    {
        firstLayer = FirstLayerAtOrAboveZ(column.minimumZMm, layerThicknessMm);
        lastLayer = LastLayerAtOrBelowZ(column.maximumZMm, layerThicknessMm);
    }
    firstLayer = std::max(0, firstLayer);
    lastLayer = std::min(layerCount - 1, lastLayer);
    return firstLayer <= lastLayer
        ? std::pair<int, int>{firstLayer, lastLayer}
        : std::pair<int, int>{-1, -1};
}

bool ContainsLayer(
    const LayerOccupancyRange& range,
    const int layerIndex) noexcept
{
    return range.firstLayer >= 0
        && layerIndex >= range.firstLayer
        && layerIndex <= range.lastLayer;
}

bool IsOutputColumnOccupied(
    const LayerOccupancyRanges& ranges,
    const std::size_t columnIndex,
    const int layerIndex)
{
    if (ranges.Policy().xyMode == XyCoverageMode::PixelCenter)
    {
        return ContainsLayer(
            ranges.PrimaryRanges()[columnIndex],
            layerIndex);
    }

    unsigned coveredSamples{0U};
    const std::size_t sampleOffset =
        columnIndex * kSupersample2x2Count;
    for (std::size_t sampleIndex{0U};
         sampleIndex < kSupersample2x2Count;
         ++sampleIndex)
    {
        coveredSamples += ContainsLayer(
            ranges.CoverageSubsampleRanges()[sampleOffset + sampleIndex],
            layerIndex)
            ? 1U
            : 0U;
    }
    return coveredSamples
        >= ranges.Policy().minimumCoveredSubsamples;
}

std::pair<int, int> FindThresholdedRange(
    const std::span<const LayerOccupancyRange> sampleRanges,
    const unsigned minimumCoveredSubsamples)
{
    int firstLayer{-1};
    int lastLayer{-1};
    for (const LayerOccupancyRange& candidate : sampleRanges)
    {
        if (candidate.firstLayer < 0)
        {
            continue;
        }
        unsigned firstCoverage{0U};
        unsigned lastCoverage{0U};
        for (const LayerOccupancyRange& sample : sampleRanges)
        {
            firstCoverage += ContainsLayer(sample, candidate.firstLayer) ? 1U : 0U;
            lastCoverage += ContainsLayer(sample, candidate.lastLayer) ? 1U : 0U;
        }
        if (firstCoverage >= minimumCoveredSubsamples
            && (firstLayer < 0 || candidate.firstLayer < firstLayer))
        {
            firstLayer = candidate.firstLayer;
        }
        if (lastCoverage >= minimumCoveredSubsamples
            && candidate.lastLayer > lastLayer)
        {
            lastLayer = candidate.lastLayer;
        }
    }
    return {firstLayer, lastLayer};
}

void ValidateRange(
    const LayerOccupancyRange& range,
    const int layerCount)
{
    if (range.firstLayer == -1 && range.lastLayer == -1)
    {
        return;
    }
    if (range.firstLayer < 0
        || range.lastLayer < range.firstLayer
        || range.lastLayer >= layerCount)
    {
        throw std::invalid_argument(
            "LayerOccupancyRanges contains an invalid interval");
    }
}

void ValidateRanges(const LayerOccupancyRanges& ranges)
{
    if (ranges.LayerCount() <= 0
        || ranges.PrimaryRanges().size() != ranges.ColumnCount()
        || ranges.FirstOccupiedLayers().size() != ranges.ColumnCount()
        || ranges.LastOccupiedLayers().size() != ranges.ColumnCount())
    {
        throw std::invalid_argument(
            "LayerOccupancyRanges dimensions are inconsistent");
    }
    if (ranges.InputKind() != GeometryOccupancyInputKind::SingleIntervalHeightfield
        && ranges.InputKind() != GeometryOccupancyInputKind::GeneralMesh)
    {
        throw std::invalid_argument(
            "LayerOccupancyRanges input kind is unsupported");
    }
    ValidateLayerOccupancyPolicy(ranges.Policy());
    if (ranges.Policy().layerMode == LayerOccupancyMode::LayerSlabCoverage
        && ranges.InputKind() != GeometryOccupancyInputKind::SingleIntervalHeightfield)
    {
        throw std::invalid_argument(
            "Layer Slab compact occupancy requires SingleIntervalHeightfield input");
    }
    if (ranges.ColumnCount()
        > std::numeric_limits<std::size_t>::max()
            / kSupersample2x2Count)
    {
        throw std::invalid_argument(
            "LayerOccupancyRanges column count is too large");
    }
    const std::size_t expectedSubsampleCount =
        ranges.Policy().xyMode == XyCoverageMode::Supersample2x2
        ? ranges.ColumnCount() * kSupersample2x2Count
        : 0U;
    if (ranges.CoverageSubsampleRanges().size()
        != expectedSubsampleCount)
    {
        throw std::invalid_argument(
            "LayerOccupancyRanges subsample count is inconsistent");
    }
    for (const LayerOccupancyRange& range : ranges.PrimaryRanges())
    {
        ValidateRange(range, ranges.LayerCount());
    }
    for (const LayerOccupancyRange& range :
         ranges.CoverageSubsampleRanges())
    {
        ValidateRange(range, ranges.LayerCount());
    }

    for (std::size_t columnIndex{0U};
         columnIndex < ranges.ColumnCount();
         ++columnIndex)
    {
        std::pair<int, int> expected;
        if (ranges.Policy().xyMode == XyCoverageMode::PixelCenter)
        {
            const LayerOccupancyRange& range =
                ranges.PrimaryRanges()[columnIndex];
            expected = {range.firstLayer, range.lastLayer};
        }
        else
        {
            const std::size_t sampleOffset =
                columnIndex * kSupersample2x2Count;
            expected = FindThresholdedRange(
                ranges.CoverageSubsampleRanges().subspan(
                    sampleOffset,
                    kSupersample2x2Count),
                ranges.Policy().minimumCoveredSubsamples);
        }
        if (ranges.FirstOccupiedLayers()[columnIndex] != expected.first
            || ranges.LastOccupiedLayers()[columnIndex] != expected.second)
        {
            throw std::invalid_argument(
                "LayerOccupancyRanges summary is inconsistent");
        }
    }
}

}  // namespace

void ValidateLayerOccupancyPolicy(const GeometryOccupancyPolicy& policy)
{
    if (policy.layerMode != LayerOccupancyMode::LegacyCenterSample
        && policy.layerMode != LayerOccupancyMode::LayerSlabCoverage)
    {
        throw std::invalid_argument("Layer occupancy mode is unsupported");
    }
    if (policy.xyMode == XyCoverageMode::PixelCenter)
    {
        if (policy.minimumCoveredSubsamples != 1U)
        {
            throw std::invalid_argument("PixelCenter requires minimumCoveredSubsamples=1");
        }
        return;
    }
    if (policy.xyMode != XyCoverageMode::Supersample2x2)
    {
        throw std::invalid_argument("XY coverage mode is unsupported");
    }
    if (policy.layerMode != LayerOccupancyMode::LayerSlabCoverage)
    {
        throw std::invalid_argument("Supersample2x2 requires LayerSlabCoverage");
    }
    if (policy.minimumCoveredSubsamples != 1U
        && policy.minimumCoveredSubsamples != 2U)
    {
        throw std::invalid_argument("Supersample2x2 supports only 1/4 or 2/4 coverage candidates");
    }
}

LayerOccupancyRanges BuildLayerOccupancyRanges(
    const LayerOccupancyRequest& request)
{
    ValidateRequest(request);

    const std::size_t columnCount{request.columns.size()};
    LayerOccupancyRanges ranges;
    ranges.layerCount_ = request.layerCount;
    ranges.columnCount_ = columnCount;
    ranges.inputKind_ = request.inputKind;
    ranges.policy_ = request.policy;
    ranges.primaryRanges_.reserve(columnCount);
    for (const GeometryOccupancyColumn& column : request.columns)
    {
        const auto [firstLayer, lastLayer] = FindOccupiedLayerRange(
            column,
            request.policy.layerMode,
            request.layerCount,
            request.layerThicknessMm);
        ranges.primaryRanges_.push_back(
            LayerOccupancyRange{firstLayer, lastLayer});
    }
    if (request.policy.xyMode == XyCoverageMode::Supersample2x2)
    {
        ranges.coverageSubsampleRanges_.reserve(
            request.coverageSubsampleColumns.size());
        for (const GeometryOccupancyColumn& sample :
             request.coverageSubsampleColumns)
        {
            const auto [firstLayer, lastLayer] = FindOccupiedLayerRange(
                sample,
                request.policy.layerMode,
                request.layerCount,
                request.layerThicknessMm);
            ranges.coverageSubsampleRanges_.push_back(
                LayerOccupancyRange{firstLayer, lastLayer});
        }
    }

    ranges.firstOccupiedLayers_.assign(columnCount, -1);
    ranges.lastOccupiedLayers_.assign(columnCount, -1);
    if (request.policy.xyMode == XyCoverageMode::PixelCenter)
    {
        for (std::size_t columnIndex{0U};
             columnIndex < columnCount;
             ++columnIndex)
        {
            ranges.firstOccupiedLayers_[columnIndex] =
                ranges.primaryRanges_[columnIndex].firstLayer;
            ranges.lastOccupiedLayers_[columnIndex] =
                ranges.primaryRanges_[columnIndex].lastLayer;
        }
    }
    else
    {
        for (std::size_t columnIndex{0U};
             columnIndex < columnCount;
             ++columnIndex)
        {
            const std::size_t sampleOffset =
                columnIndex * kSupersample2x2Count;
            const auto [firstLayer, lastLayer] = FindThresholdedRange(
                std::span<const LayerOccupancyRange>{
                    ranges.coverageSubsampleRanges_}.subspan(
                        sampleOffset,
                        kSupersample2x2Count),
                request.policy.minimumCoveredSubsamples);
            ranges.firstOccupiedLayers_[columnIndex] = firstLayer;
            ranges.lastOccupiedLayers_[columnIndex] = lastLayer;
        }
    }
    ValidateRanges(ranges);
    return ranges;
}

void MaterializeLayerOccupancy(
    const LayerOccupancyRanges& ranges,
    const int layerIndex,
    const std::span<std::uint8_t> output)
{
    if (layerIndex < 0 || layerIndex >= ranges.layerCount_)
    {
        throw std::invalid_argument(
            "Layer occupancy materialization index is out of range");
    }
    if (output.size() != ranges.columnCount_)
    {
        throw std::invalid_argument(
            "Layer occupancy materialization output size is invalid");
    }

    std::fill(output.begin(), output.end(), 0U);
    for (std::size_t columnIndex{0U};
         columnIndex < ranges.columnCount_;
         ++columnIndex)
    {
        output[columnIndex] = IsOutputColumnOccupied(
            ranges,
            columnIndex,
            layerIndex)
            ? 1U
            : 0U;
    }
}

LayerOccupancyResult BuildLayerOccupancy(const LayerOccupancyRequest& request)
{
    ValidateRequest(request);

    const std::size_t columnCount{request.columns.size()};
    LayerOccupancyResult result;
    result.masks.resize(
        static_cast<std::size_t>(request.layerCount),
        std::vector<std::uint8_t>(columnCount, 0));
    result.firstOccupiedLayers.assign(columnCount, -1);
    result.lastOccupiedLayers.assign(columnCount, -1);

    if (request.policy.xyMode == XyCoverageMode::Supersample2x2)
    {
        std::vector<std::uint8_t> layerCoverageCounts(
            static_cast<std::size_t>(request.layerCount),
            0);
        for (std::size_t columnIndex{0}; columnIndex < columnCount; ++columnIndex)
        {
            std::fill(layerCoverageCounts.begin(), layerCoverageCounts.end(), 0);
            for (std::size_t sampleIndex{0}; sampleIndex < kSupersample2x2Count; ++sampleIndex)
            {
                const GeometryOccupancyColumn& sample{
                    request.coverageSubsampleColumns[columnIndex * kSupersample2x2Count + sampleIndex]};
                const auto [firstLayer, lastLayer] = FindOccupiedLayerRange(
                    sample,
                    request.policy.layerMode,
                    request.layerCount,
                    request.layerThicknessMm);
                for (int layerIndex{firstLayer}; layerIndex >= 0 && layerIndex <= lastLayer; ++layerIndex)
                {
                    ++layerCoverageCounts[static_cast<std::size_t>(layerIndex)];
                }
            }

            for (int layerIndex{0}; layerIndex < request.layerCount; ++layerIndex)
            {
                if (layerCoverageCounts[static_cast<std::size_t>(layerIndex)]
                    < request.policy.minimumCoveredSubsamples)
                {
                    continue;
                }
                result.masks[static_cast<std::size_t>(layerIndex)][columnIndex] = 1;
                if (result.firstOccupiedLayers[columnIndex] < 0)
                {
                    result.firstOccupiedLayers[columnIndex] = layerIndex;
                }
                result.lastOccupiedLayers[columnIndex] = layerIndex;
            }
        }
        return result;
    }

    for (std::size_t columnIndex{0}; columnIndex < columnCount; ++columnIndex)
    {
        const GeometryOccupancyColumn& column{request.columns[columnIndex]};
        if (!column.occupied)
        {
            continue;
        }
        const auto [firstLayer, lastLayer] = FindOccupiedLayerRange(
            column,
            request.policy.layerMode,
            request.layerCount,
            request.layerThicknessMm);
        if (firstLayer > lastLayer)
        {
            continue;
        }

        result.firstOccupiedLayers[columnIndex] = firstLayer;
        result.lastOccupiedLayers[columnIndex] = lastLayer;
        for (int layerIndex{firstLayer}; layerIndex >= 0 && layerIndex <= lastLayer; ++layerIndex)
        {
            result.masks[static_cast<std::size_t>(layerIndex)][columnIndex] = 1;
        }
    }

    return result;
}

}  // namespace slicer_core
