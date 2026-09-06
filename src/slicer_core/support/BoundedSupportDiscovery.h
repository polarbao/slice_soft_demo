#pragma once

#include "slicer_core/geometry/LayerOccupancyProvider.h"
#include "slicer_core/materials/varnish_geometry/OuterVarnishDiscretization.h"
#include "slicer_core/support/BoundedSupportDemand.h"
#include "slicer_core/support/SupportType.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace slicer_core
{

/** @brief Configuration for the bounded P1 outer-boundary scan. */
struct BoundedOuterBoundaryScanRequest
{
    int widthPx{0};
    int heightPx{0};
    int layerCount{0};
    GeometryOccupancyInputKind inputKind{
        GeometryOccupancyInputKind::SingleIntervalHeightfield};
    OuterVarnishDiscretization outerVarnish;
    bool includeOuterVarnishInUpperBoundary{false};
};

/** @brief Compact completed result of the bounded P1 scan. */
class BoundedOuterBoundaryScanResult
{
public:
    BoundedOuterBoundaryScanResult(
        const BoundedOuterBoundaryScanResult&) = delete;
    BoundedOuterBoundaryScanResult(
        BoundedOuterBoundaryScanResult&&) noexcept = default;
    BoundedOuterBoundaryScanResult& operator=(
        const BoundedOuterBoundaryScanResult&) = delete;
    BoundedOuterBoundaryScanResult& operator=(
        BoundedOuterBoundaryScanResult&&) noexcept = default;

    [[nodiscard]] std::span<const int> UpperBoundaryLastLayers() const noexcept
    {
        return upperBoundaryLastLayers_;
    }

private:
    BoundedOuterBoundaryScanResult() = default;
    friend class BoundedOuterBoundaryScanner;

    std::vector<int> upperBoundaryLastLayers_;
};

/**
 * @brief Materialize outer varnish and upper boundary one layer at a time.
 *
 * Layers must be consumed exactly once in ascending order. Finish is only
 * valid after all layers have been consumed. Output and scratch storage are
 * bounded by one raster layer.
 */
class BoundedOuterBoundaryScanner
{
public:
    explicit BoundedOuterBoundaryScanner(
        const BoundedOuterBoundaryScanRequest& request);

    BoundedOuterBoundaryScanner(const BoundedOuterBoundaryScanner&) = delete;
    BoundedOuterBoundaryScanner(
        BoundedOuterBoundaryScanner&&) noexcept = default;
    BoundedOuterBoundaryScanner& operator=(
        const BoundedOuterBoundaryScanner&) = delete;
    BoundedOuterBoundaryScanner& operator=(
        BoundedOuterBoundaryScanner&&) noexcept = default;

    void ConsumeLayer(
        int layerIndex,
        std::span<const std::uint8_t> modelMask,
        std::span<std::uint8_t> outputOuterVarnishMask,
        std::span<std::uint8_t> outputUpperBoundaryMask);

    [[nodiscard]] int ExpectedLayerIndex() const noexcept
    {
        return expectedLayerIndex_;
    }

    [[nodiscard]] BoundedOuterBoundaryScanResult Finish() &&;

private:
    BoundedOuterBoundaryScanRequest request_;
    std::size_t pixelCount_{0U};
    int expectedLayerIndex_{0};
    std::vector<int> upperBoundaryLastLayers_;
    std::vector<std::uint8_t> externalEmptyScratch_;
    std::vector<std::uint8_t> dilationScratch_;
    std::vector<int> floodStack_;
};

/** @brief Unsupported-island detection policy for bounded P2 replay. */
struct BoundedUnsupportedDiscoveryRequest
{
    int widthPx{0};
    int heightPx{0};
    int layerCount{0};
    GeometryOccupancyInputKind inputKind{
        GeometryOccupancyInputKind::SingleIntervalHeightfield};
    bool enabled{false};
    int connectivity{8};
    int xyDilationPx{0};
    double minOverlapRatio{0.2};
    int minIslandAreaPx{16};
};

/** @brief Existing retained diagnostic counters owned by one source layer. */
struct BoundedUnsupportedLayerEvent
{
    int layerIndex{0};
    int islandCount{0};
    int islandPixels{0};
    int unsupportedPixels{0};
    int filteredIslandCount{0};
    int filteredIslandPixels{0};
};

/** @brief Existing retained aggregate counters for unsupported discovery. */
struct BoundedUnsupportedTotals
{
    int layersWithIslands{0};
    int islandCount{0};
    int islandPixels{0};
    int unsupportedPixels{0};
    int filteredIslandCount{0};
    int filteredIslandPixels{0};
};

/** @brief Compact completed result of the bounded P2 scan. */
class BoundedUnsupportedDiscoveryResult
{
public:
    BoundedUnsupportedDiscoveryResult(
        const BoundedUnsupportedDiscoveryResult&) = delete;
    BoundedUnsupportedDiscoveryResult(
        BoundedUnsupportedDiscoveryResult&&) noexcept = default;
    BoundedUnsupportedDiscoveryResult& operator=(
        const BoundedUnsupportedDiscoveryResult&) = delete;
    BoundedUnsupportedDiscoveryResult& operator=(
        BoundedUnsupportedDiscoveryResult&&) noexcept = default;

    [[nodiscard]] std::span<const int> UnsupportedTopExclusiveLayers() const noexcept
    {
        return unsupportedTopExclusiveLayers_;
    }
    [[nodiscard]] std::span<const BoundedUnsupportedLayerEvent> LayerEvents() const noexcept
    {
        return layerEvents_;
    }
    [[nodiscard]] const BoundedUnsupportedTotals& Totals() const noexcept
    {
        return totals_;
    }

private:
    BoundedUnsupportedDiscoveryResult() = default;
    friend class BoundedUnsupportedDiscoveryScanner;

    std::vector<int> unsupportedTopExclusiveLayers_;
    std::vector<BoundedUnsupportedLayerEvent> layerEvents_;
    BoundedUnsupportedTotals totals_;
};

/**
 * @brief Discover unsupported demand from a sequential bounded layer stream.
 *
 * The preliminary plan must outlive this scanner and must have unsupported
 * generation disabled. Previous-layer support is materialized only from the
 * Bottom/Full/Upper facts already frozen by MF-03B1.
 */
class BoundedUnsupportedDiscoveryScanner
{
public:
    BoundedUnsupportedDiscoveryScanner(
        const BoundedUnsupportedDiscoveryRequest& request,
        const BoundedSupportDemandPlan& preliminaryPlan);

    BoundedUnsupportedDiscoveryScanner(
        const BoundedUnsupportedDiscoveryScanner&) = delete;
    BoundedUnsupportedDiscoveryScanner(
        BoundedUnsupportedDiscoveryScanner&&) noexcept = default;
    BoundedUnsupportedDiscoveryScanner& operator=(
        const BoundedUnsupportedDiscoveryScanner&) = delete;
    BoundedUnsupportedDiscoveryScanner& operator=(
        BoundedUnsupportedDiscoveryScanner&&) noexcept = default;

    void ConsumeLayer(
        int layerIndex,
        std::span<const std::uint8_t> modelMask,
        std::span<const std::uint8_t> upperBoundaryMask);

    [[nodiscard]] int ExpectedLayerIndex() const noexcept
    {
        return expectedLayerIndex_;
    }

    [[nodiscard]] BoundedUnsupportedDiscoveryResult Finish() &&;

private:
    BoundedUnsupportedDiscoveryRequest request_;
    const BoundedSupportDemandPlan* preliminaryPlan_{nullptr};
    std::size_t pixelCount_{0U};
    int expectedLayerIndex_{0};
    std::vector<int> unsupportedTopExclusiveLayers_;
    std::vector<BoundedUnsupportedLayerEvent> layerEvents_;
    BoundedUnsupportedTotals totals_;
    std::vector<std::uint8_t> previousModelMask_;
    std::vector<std::uint8_t> previousUpperBoundaryMask_;
    std::vector<std::uint8_t> preliminarySupportMask_;
    std::vector<SupportType> preliminarySupportTypes_;
    std::vector<std::uint8_t> baseScratchA_;
    std::vector<std::uint8_t> baseScratchB_;
    std::vector<std::uint8_t> visitedScratch_;
    std::vector<int> traversalStack_;
    std::vector<int> componentPixels_;
};

}  // namespace slicer_core
