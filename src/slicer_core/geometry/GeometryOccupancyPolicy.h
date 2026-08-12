#pragma once

namespace slicer_core
{

/** @brief Selects how a vertical geometry interval is mapped to output layers. */
enum class LayerOccupancyMode
{
    LegacyCenterSample,
    LayerSlabCoverage
};

/** @brief Selects the XY coverage sampling mode for geometry occupancy. */
enum class XyCoverageMode
{
    PixelCenter,
    Supersample2x2
};

/** @brief Geometry-only occupancy strategy shared by sampling providers. */
struct GeometryOccupancyPolicy
{
    LayerOccupancyMode layerMode{LayerOccupancyMode::LegacyCenterSample};
    XyCoverageMode xyMode{XyCoverageMode::PixelCenter};
    unsigned minimumCoveredSubsamples{1U};
};

/**
 * @brief Build the production-compatible legacy occupancy policy.
 * @return Policy using center-sampled layers and center-sampled XY coverage.
 */
constexpr GeometryOccupancyPolicy MakeLegacyGeometryOccupancyPolicy() noexcept
{
    return GeometryOccupancyPolicy{};
}

/**
 * @brief Build the Stage 16A-03 layer-slab, pixel-center candidate policy.
 * @return Policy using half-open layer slabs and existing XY pixel centers.
 */
constexpr GeometryOccupancyPolicy MakeLayerSlabGeometryOccupancyPolicy() noexcept
{
    GeometryOccupancyPolicy policy;
    policy.layerMode = LayerOccupancyMode::LayerSlabCoverage;
    return policy;
}

/**
 * @brief Build a Stage 16A-04 layer-slab, fixed 2x2 coverage candidate policy.
 * @param minimumCoveredSubsamples Required covered samples per output pixel and layer.
 * @return Policy using half-open layer slabs and fixed 2x2 XY coverage.
 */
constexpr GeometryOccupancyPolicy MakeLayerSlabSupersample2x2GeometryOccupancyPolicy(
    const unsigned minimumCoveredSubsamples) noexcept
{
    GeometryOccupancyPolicy policy;
    policy.layerMode = LayerOccupancyMode::LayerSlabCoverage;
    policy.xyMode = XyCoverageMode::Supersample2x2;
    policy.minimumCoveredSubsamples = minimumCoveredSubsamples;
    return policy;
}

}  // namespace slicer_core
