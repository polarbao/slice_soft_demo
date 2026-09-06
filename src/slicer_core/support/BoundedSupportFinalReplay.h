#pragma once

#include "slicer_core/support/BoundedSupportShapeScan.h"
#include "slicer_core/support/SupportType.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace slicer_core
{

inline constexpr std::size_t kBoundedSupportTypeCount{7U};

/** @brief Compact retained-equivalent connectivity totals for one final layer. */
struct BoundedSupportConnectivityStats
{
    bool enabled{true};
    int componentCount{0};
    int largestComponentPixels{0};
    int smallComponentCount{0};
    int tinyComponentCount{0};
    std::span<const BoundedSupportComponentSummary> components;
};

/**
 * @brief Final support/type evidence handed off before caller buffers commit.
 *
 * The component span is valid only for the duration of sink Consume.
 */
struct BoundedSupportFinalLayerStats
{
    int layerIndex{0};
    std::uint64_t supportPixels{0U};
    std::array<std::uint64_t, kBoundedSupportTypeCount> typePixels{};
    BoundedSupportConnectivityStats connectivity;
};

class BoundedSupportFinalLayerSink
{
public:
    virtual ~BoundedSupportFinalLayerSink() = default;
    virtual void Consume(const BoundedSupportFinalLayerStats& stats) = 0;
};

/** @brief Frozen configuration for non-production MF-03B4A replay. */
struct BoundedSupportFinalReplayRequest
{
    BoundedSupportShapeScanRequest shapeReplay;
    SupportBaseProjectionConfig baseProjection;
};

/** @brief Compact BaseProjection evidence derived from the B3 footprint. */
struct BoundedSupportBaseProjectionSummary
{
    bool enabled{false};
    int configuredLayerCount{0};
    int effectiveLayerCount{0};
    std::string layerPlacement{"overlay_existing"};
    std::size_t footprintPixels{0U};
    std::uint64_t addedSupportPixels{0U};
};

/** @brief Aggregate final support evidence without retained raster layers. */
struct BoundedSupportFinalTotals
{
    int completedLayerCount{0};
    int layersWithSupport{0};
    std::uint64_t supportPixels{0U};
    std::array<std::uint64_t, kBoundedSupportTypeCount> typePixels{};
    std::uint64_t clearedOuterVarnishSupportPixels{0U};
    std::uint64_t connectivityComponentCount{0U};
    int largestConnectivityComponentPixels{0};
    std::uint64_t smallConnectivityComponentCount{0U};
    std::uint64_t tinyConnectivityComponentCount{0U};
};

class BoundedSupportFinalReplayResult
{
public:
    BoundedSupportFinalReplayResult(
        const BoundedSupportFinalReplayResult&) = delete;
    BoundedSupportFinalReplayResult(
        BoundedSupportFinalReplayResult&&) noexcept = default;
    BoundedSupportFinalReplayResult& operator=(
        const BoundedSupportFinalReplayResult&) = delete;
    BoundedSupportFinalReplayResult& operator=(
        BoundedSupportFinalReplayResult&&) noexcept = default;

    [[nodiscard]] const BoundedSupportBaseProjectionSummary& BaseProjection()
        const noexcept
    {
        return baseProjection_;
    }
    [[nodiscard]] const BoundedSupportFinalTotals& Totals() const noexcept
    {
        return totals_;
    }

private:
    BoundedSupportFinalReplayResult() = default;
    friend class BoundedSupportFinalReplayScanner;

    BoundedSupportBaseProjectionSummary baseProjection_;
    BoundedSupportFinalTotals totals_;
};

/**
 * @brief Sequential verified support finalizer for non-production MF-03B4A.
 *
 * The final plan and completed B3 result must outlive this scanner. Each layer
 * is replayed into private scratch and its digest is verified before Base,
 * outer-varnish priority, sink handoff, or caller-output commit.
 */
class BoundedSupportFinalReplayScanner
{
public:
    BoundedSupportFinalReplayScanner(
        const BoundedSupportFinalReplayRequest& request,
        const BoundedSupportDemandPlan& finalPlan,
        const BoundedSupportShapeScanResult& verifiedShapeResult,
        BoundedSupportFinalLayerSink* layerSink = nullptr);

    BoundedSupportFinalReplayScanner(
        const BoundedSupportFinalReplayScanner&) = delete;
    BoundedSupportFinalReplayScanner(
        BoundedSupportFinalReplayScanner&&) noexcept = default;
    BoundedSupportFinalReplayScanner& operator=(
        const BoundedSupportFinalReplayScanner&) = delete;
    BoundedSupportFinalReplayScanner& operator=(
        BoundedSupportFinalReplayScanner&&) noexcept = default;

    void ConsumeLayer(
        int layerIndex,
        std::span<const std::uint8_t> modelMask,
        std::span<const std::uint8_t> upperBoundaryMask,
        std::span<const std::uint8_t> outerVarnishMask,
        std::span<std::uint8_t> outputSupportMask,
        std::span<SupportType> outputTypeMap,
        std::span<std::uint8_t> outputClearedOuterOverlapMask);

    [[nodiscard]] int ExpectedLayerIndex() const noexcept
    {
        return expectedLayerIndex_;
    }

    [[nodiscard]] BoundedSupportFinalReplayResult Finish() &&;

private:
    BoundedSupportFinalReplayRequest request_;
    const BoundedSupportShapeScanResult* verifiedShapeResult_{nullptr};
    BoundedSupportFinalLayerSink* layerSink_{nullptr};
    std::size_t pixelCount_{0U};
    int expectedLayerIndex_{0};
    bool failed_{false};
    bool finished_{false};
    BoundedSupportShapeScanner shapeScanner_;
    BoundedSupportBaseProjectionSummary baseProjection_;
    BoundedSupportFinalTotals totals_;
    std::vector<std::uint8_t> supportScratch_;
    std::vector<SupportType> typeScratch_;
    std::vector<std::uint8_t> clearedScratch_;
    std::vector<std::uint8_t> visitedScratch_;
    std::vector<int> traversalStack_;
    std::vector<BoundedSupportComponentSummary> componentScratch_;
};

}  // namespace slicer_core
