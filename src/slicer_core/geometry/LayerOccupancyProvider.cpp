#include "slicer_core/geometry/LayerOccupancyProvider.h"

#include <algorithm>
#include <cmath>
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
        && request.coverageSubsampleColumns.size()
            != request.columns.size() * kSupersample2x2Count)
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
