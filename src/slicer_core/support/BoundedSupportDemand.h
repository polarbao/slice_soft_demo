#pragma once

#include "slicer_core/geometry/LayerOccupancyProvider.h"
#include "slicer_core/support/SupportType.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace slicer_core
{

/**
 * @brief Compact per-column facts for pre-shape support materialization.
 *
 * All four spans must have the same non-zero size. A value of -1 means that
 * lower/model-last/upper-boundary has no fact for that column. Unsupported
 * demand uses its retained source layer as a half-open top: 0 means none and
 * valid sources are 1..layerCount-1 inside the corresponding model range.
 */
struct BoundedSupportDemandRequest
{
    int layerCount{0};
    GeometryOccupancyInputKind inputKind{
        GeometryOccupancyInputKind::SingleIntervalHeightfield};
    std::span<const int> lowerSourceLayers;
    std::span<const int> modelLastLayers;
    std::span<const int> upperBoundaryLastLayers;
    std::span<const int> unsupportedTopExclusiveLayers;
    bool lowerEnabled{false};
    bool fullVerticalEnabled{false};
    bool upperEnabled{false};
    bool unsupportedEnabled{false};
};

/**
 * @brief Validated immutable support-demand facts for caller-owned replay.
 *
 * This MF-03B1 contract deliberately excludes island detection, InternalVoid,
 * shape, base projection, varnish and material composition.
 */
class BoundedSupportDemandPlan
{
public:
    BoundedSupportDemandPlan(const BoundedSupportDemandPlan&) = delete;
    BoundedSupportDemandPlan(BoundedSupportDemandPlan&&) noexcept = default;
    BoundedSupportDemandPlan& operator=(const BoundedSupportDemandPlan&) = delete;
    BoundedSupportDemandPlan& operator=(BoundedSupportDemandPlan&&) noexcept = default;

    [[nodiscard]] int LayerCount() const noexcept { return layerCount_; }
    [[nodiscard]] std::size_t ColumnCount() const noexcept { return columnCount_; }
    [[nodiscard]] GeometryOccupancyInputKind InputKind() const noexcept
    {
        return inputKind_;
    }
    [[nodiscard]] bool LowerEnabled() const noexcept { return lowerEnabled_; }
    [[nodiscard]] bool FullVerticalEnabled() const noexcept
    {
        return fullVerticalEnabled_;
    }
    [[nodiscard]] bool UpperEnabled() const noexcept { return upperEnabled_; }
    [[nodiscard]] bool UnsupportedEnabled() const noexcept
    {
        return unsupportedEnabled_;
    }
    [[nodiscard]] std::span<const int> LowerSourceLayers() const noexcept
    {
        return lowerSourceLayers_;
    }
    [[nodiscard]] std::span<const int> ModelLastLayers() const noexcept
    {
        return modelLastLayers_;
    }
    [[nodiscard]] std::span<const int> UpperBoundaryLastLayers() const noexcept
    {
        return upperBoundaryLastLayers_;
    }
    [[nodiscard]] std::span<const int> UnsupportedTopExclusiveLayers() const noexcept
    {
        return unsupportedTopExclusiveLayers_;
    }

private:
    BoundedSupportDemandPlan() = default;

    friend BoundedSupportDemandPlan BuildBoundedSupportDemandPlan(
        const BoundedSupportDemandRequest& request);
    friend void MaterializePreShapeSupportLayer(
        const BoundedSupportDemandPlan& plan,
        int layerIndex,
        std::span<const std::uint8_t> modelOccupancyMask,
        std::span<const std::uint8_t> upperBoundaryOccupancyMask,
        std::span<std::uint8_t> outputSupportMask,
        std::span<SupportType> outputTypeMap);
    friend BoundedSupportDemandPlan FinalizeBoundedUnsupportedDemand(
        BoundedSupportDemandPlan&& preliminaryPlan,
        std::vector<int>&& unsupportedTopExclusiveLayers);

    int layerCount_{0};
    std::size_t columnCount_{0U};
    GeometryOccupancyInputKind inputKind_{
        GeometryOccupancyInputKind::SingleIntervalHeightfield};
    std::vector<int> lowerSourceLayers_;
    std::vector<int> modelLastLayers_;
    std::vector<int> upperBoundaryLastLayers_;
    std::vector<int> unsupportedTopExclusiveLayers_;
    bool lowerEnabled_{false};
    bool fullVerticalEnabled_{false};
    bool upperEnabled_{false};
    bool unsupportedEnabled_{false};
};

/** @brief Validate and copy compact pre-shape support demand facts. */
[[nodiscard]] BoundedSupportDemandPlan BuildBoundedSupportDemandPlan(
    const BoundedSupportDemandRequest& request);

/**
 * @brief Move unsupported discovery facts into an existing preliminary plan.
 *
 * This avoids deep-copying the other three large per-column fact vectors.
 * The preliminary plan must have unsupported generation disabled.
 */
[[nodiscard]] BoundedSupportDemandPlan FinalizeBoundedUnsupportedDemand(
    BoundedSupportDemandPlan&& preliminaryPlan,
    std::vector<int>&& unsupportedTopExclusiveLayers);

/**
 * @brief Materialize one pre-shape support layer into reusable caller storage.
 *
 * Output buffers are fully overwritten. Model pixels retain priority for all
 * support types. Upper projection also excludes the current upper-boundary
 * occupancy, which may be model OR outer-varnish. This operation performs no
 * cross-layer scan.
 */
void MaterializePreShapeSupportLayer(
    const BoundedSupportDemandPlan& plan,
    int layerIndex,
    std::span<const std::uint8_t> modelOccupancyMask,
    std::span<const std::uint8_t> upperBoundaryOccupancyMask,
    std::span<std::uint8_t> outputSupportMask,
    std::span<SupportType> outputTypeMap);

}  // namespace slicer_core
