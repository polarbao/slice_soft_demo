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

}  // namespace slicer_core
