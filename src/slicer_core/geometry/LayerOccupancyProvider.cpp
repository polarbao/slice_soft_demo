#include "slicer_core/geometry/LayerOccupancyProvider.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace slicer_core
{
namespace
{

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
}

int FirstLayerAtOrAboveZ(const double zMm, const double layerThicknessMm)
{
    return static_cast<int>(std::ceil(zMm / layerThicknessMm - 0.5));
}

int LastLayerAtOrBelowZ(const double zMm, const double layerThicknessMm)
{
    return static_cast<int>(std::floor(zMm / layerThicknessMm - 0.5));
}

}  // namespace

void ValidateLayerOccupancyPolicy(const GeometryOccupancyPolicy& policy)
{
    if (policy.layerMode != LayerOccupancyMode::LegacyCenterSample)
    {
        throw std::invalid_argument("LayerSlabCoverage is not implemented by the 16A-02 provider");
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

        int firstLayer{FirstLayerAtOrAboveZ(column.minimumZMm, request.layerThicknessMm)};
        int lastLayer{LastLayerAtOrBelowZ(column.maximumZMm, request.layerThicknessMm)};
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
