#pragma once

namespace slicer_core
{

/** @brief 选择垂直几何区间如何映射到输出层。 */
enum class LayerOccupancyMode
{
    LegacyCenterSample,
    LayerSlabCoverage
};

/** @brief 选择几何占用的 XY 覆盖采样模式。 */
enum class XyCoverageMode
{
    PixelCenter,
    Supersample2x2
};

/** @brief 采样提供者共享的纯几何占用策略。 */
struct GeometryOccupancyPolicy
{
    LayerOccupancyMode layerMode{LayerOccupancyMode::LegacyCenterSample};
    XyCoverageMode xyMode{XyCoverageMode::PixelCenter};
    unsigned minimumCoveredSubsamples{1U};
};

/**
 * @brief 构造兼容生产的旧版占用策略。
 * @return 层和 XY 覆盖均使用中心采样的策略。
 */
constexpr GeometryOccupancyPolicy MakeLegacyGeometryOccupancyPolicy() noexcept
{
    return GeometryOccupancyPolicy{};
}

/**
 * @brief 构造 Stage 16A-03 层薄片、像素中心候选策略。
 * @return 使用半开层薄片和现有 XY 像素中心的策略。
 */
constexpr GeometryOccupancyPolicy MakeLayerSlabGeometryOccupancyPolicy() noexcept
{
    GeometryOccupancyPolicy policy;
    policy.layerMode = LayerOccupancyMode::LayerSlabCoverage;
    return policy;
}

/**
 * @brief 构造 Stage 16A-04 层薄片、固定 2x2 覆盖候选策略。
 * @param minimumCoveredSubsamples 每个输出像素和层要求覆盖的子采样数。
 * @return 使用半开层薄片和固定 2x2 XY 覆盖的策略。
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
