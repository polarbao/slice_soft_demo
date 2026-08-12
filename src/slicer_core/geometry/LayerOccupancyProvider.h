#pragma once

#include "slicer_core/geometry/GeometryOccupancyPolicy.h"

#include <cstdint>
#include <span>
#include <vector>

namespace slicer_core
{

/** @brief Geometry field admission used by a layer occupancy request. */
enum class GeometryOccupancyInputKind
{
    SingleIntervalHeightfield,
    GeneralMesh
};

/** @brief One XY column represented by its occupied vertical extent in millimeters. */
struct GeometryOccupancyColumn
{
    bool occupied{false};
    double minimumZMm{0.0};
    double maximumZMm{0.0};
};

/** @brief Geometry-only input used to materialize layer occupancy masks. */
struct LayerOccupancyRequest
{
    std::span<const GeometryOccupancyColumn> columns;
    int layerCount{0};
    double layerThicknessMm{0.0};
    GeometryOccupancyInputKind inputKind{GeometryOccupancyInputKind::SingleIntervalHeightfield};
    GeometryOccupancyPolicy policy{MakeLegacyGeometryOccupancyPolicy()};
};

/**
 * @brief Materialized binary occupancy masks indexed by layer and then XY column.
 *
 * Member names retain conventional camelCase because this DTO is a public core
 * contract rather than a project-defined lowercase fixture struct.
 */
struct LayerOccupancyResult
{
    std::vector<std::vector<std::uint8_t>> masks;
    std::vector<int> firstOccupiedLayers;
    std::vector<int> lastOccupiedLayers;
};

/**
 * @brief Materializes geometry occupancy using an explicit STL-only strategy.
 * @param request Column field, output-layer geometry, and occupancy strategy.
 * @return Layer masks and per-column occupied ranges.
 * @throws std::invalid_argument when the request is invalid or selects an unsupported candidate.
 */
LayerOccupancyResult BuildLayerOccupancy(const LayerOccupancyRequest& request);

/**
 * @brief Validate a geometry occupancy policy without invoking a candidate algorithm.
 * @param policy Policy to validate.
 * @throws std::invalid_argument when the policy is unsupported by the current provider.
 */
void ValidateLayerOccupancyPolicy(const GeometryOccupancyPolicy& policy);

}  // namespace slicer_core
