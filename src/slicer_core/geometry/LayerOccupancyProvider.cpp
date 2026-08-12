#include "slicer_core/geometry/LayerOccupancyProvider.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace slicer_core
{
namespace
{

constexpr double kLayerBoundaryToleranceMm{1.0e-9};

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

}  // namespace

void ValidateLayerOccupancyPolicy(const GeometryOccupancyPolicy& policy)
{
    if (policy.layerMode != LayerOccupancyMode::LegacyCenterSample
        && policy.layerMode != LayerOccupancyMode::LayerSlabCoverage)
    {
        throw std::invalid_argument("Layer occupancy mode is unsupported");
    }
    if (policy.xyMode != XyCoverageMode::PixelCenter)
    {
        throw std::invalid_argument("Supersample2x2 is not implemented by the 16A-02 provider");
    }
    if (policy.minimumCoveredSubsamples != 1U)
    {
        throw std::invalid_argument("PixelCenter requires minimumCoveredSubsamples=1");
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

    for (std::size_t columnIndex{0}; columnIndex < columnCount; ++columnIndex)
    {
        const GeometryOccupancyColumn& column{request.columns[columnIndex]};
        if (!column.occupied)
        {
            continue;
        }
        if (!std::isfinite(column.minimumZMm) || !std::isfinite(column.maximumZMm)
            || column.minimumZMm > column.maximumZMm)
        {
            throw std::invalid_argument("GeometryOccupancyColumn must contain a finite ordered interval");
        }

        if (request.policy.layerMode == LayerOccupancyMode::LayerSlabCoverage
            && column.maximumZMm - column.minimumZMm <= kLayerBoundaryToleranceMm)
        {
            continue;
        }

        int firstLayer{0};
        int lastLayer{-1};
        if (request.policy.layerMode == LayerOccupancyMode::LayerSlabCoverage)
        {
            firstLayer = FirstLayerIntersectingSlab(
                column.minimumZMm,
                request.layerThicknessMm);
            lastLayer = LastLayerIntersectingSlab(
                column.maximumZMm,
                request.layerThicknessMm);
        }
        else
        {
            firstLayer = FirstLayerAtOrAboveZ(
                column.minimumZMm,
                request.layerThicknessMm);
            lastLayer = LastLayerAtOrBelowZ(
                column.maximumZMm,
                request.layerThicknessMm);
        }
        firstLayer = std::max(0, firstLayer);
        lastLayer = std::min(request.layerCount - 1, lastLayer);
        if (firstLayer > lastLayer)
        {
            continue;
        }

        result.firstOccupiedLayers[columnIndex] = firstLayer;
        result.lastOccupiedLayers[columnIndex] = lastLayer;
        for (int layerIndex{firstLayer}; layerIndex <= lastLayer; ++layerIndex)
        {
            result.masks[static_cast<std::size_t>(layerIndex)][columnIndex] = 1;
        }
    }

    return result;
}

}  // namespace slicer_core
