#pragma once

#include "slicer_core/config.h"
#include "slicer_core/support/BoundedSupportDemand.h"
#include "slicer_core/support/SupportShapePolicy.h"
#include "slicer_core/support/SupportType.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace slicer_core
{

/** @brief Pixel-free connected-component evidence retained by MF-03B3. */
struct BoundedSupportComponentSummary
{
    int areaPx{0};
    int minX{0};
    int minY{0};
    int maxX{0};
    int maxY{0};
};

/** @brief Compact equivalent of one retained component-analysis result. */
struct BoundedSupportComponentAnalysis
{
    bool enabled{false};
    int componentCount{0};
    int largestComponentArea{0};
    int smallComponentCount{0};
    int tinyComponentCount{0};
    int tinyComponentAreaPx{8};
    int smallComponentAreaPx{512};
    std::vector<BoundedSupportComponentSummary> components;
};

/** @brief Pixel-free evidence for one filtered support component. */
struct BoundedFilteredSupportComponent
{
    int layerIndex{0};
    int areaPx{0};
    int minX{0};
    int minY{0};
    int maxX{0};
    int maxY{0};
};

/** @brief Compact evidence for one retained support bridge. */
struct BoundedBridgedSupportGap
{
    int layerIndex{0};
    int x0{0};
    int y0{0};
    int x1{0};
    int y1{0};
    int gapPx{0};
    std::string direction;
};

/**
 * @brief Pixel-free retained-equivalent shape report for one reported layer.
 *
 * The synchronous sink may serialize or spool this value, but must not retain
 * references to it after Consume returns.
 */
struct BoundedSupportShapeLayerReport
{
    int layerIndex{0};
    BoundedSupportComponentAnalysis pre;
    BoundedSupportComponentAnalysis post;
    int addedSupportPixels{0};
    int removedSupportPixels{0};
    std::vector<BoundedFilteredSupportComponent> filteredComponents;
    std::vector<BoundedBridgedSupportGap> bridgedGaps;
    std::vector<std::string> warnings;
    std::vector<std::string> globalWarnings;
};

/** @brief Synchronous handoff for compact per-layer shape report evidence. */
class BoundedSupportShapeReportSink
{
public:
    virtual ~BoundedSupportShapeReportSink() = default;
    virtual void Consume(const BoundedSupportShapeLayerReport& report) = 0;
};

/** @brief Configuration frozen for the non-production MF-03B3 scan. */
struct BoundedSupportShapeScanRequest
{
    int widthPx{0};
    int heightPx{0};
    int layerCount{0};
    GeometryOccupancyInputKind inputKind{
        GeometryOccupancyInputKind::SingleIntervalHeightfield};
    int connectivity{8};
    InternalVoidSupportConfig internalVoid;
    SupportShapePolicy shape;
};

/** @brief Canonical SHA-256 for one post-shape support/type layer. */
using BoundedSupportReplayDigest = std::array<std::uint8_t, 32>;

/** @brief Aggregate P3 evidence that does not retain raster-layer stacks. */
struct BoundedSupportShapeTotals
{
    int reportedLayerCount{0};
    std::uint64_t addedSupportPixels{0U};
    std::uint64_t removedSupportPixels{0U};
    std::uint64_t filteredComponentCount{0U};
    std::uint64_t bridgedGapCount{0U};
    std::uint64_t warningCount{0U};
};

/** @brief Completed compact result of the non-production P3 scan. */
class BoundedSupportShapeScanResult
{
public:
    BoundedSupportShapeScanResult(
        const BoundedSupportShapeScanResult&) = delete;
    BoundedSupportShapeScanResult(
        BoundedSupportShapeScanResult&&) noexcept = default;
    BoundedSupportShapeScanResult& operator=(
        const BoundedSupportShapeScanResult&) = delete;
    BoundedSupportShapeScanResult& operator=(
        BoundedSupportShapeScanResult&&) noexcept = default;

    [[nodiscard]] std::span<const std::uint8_t> SupportFootprint() const noexcept
    {
        return supportFootprint_;
    }
    [[nodiscard]] std::size_t FootprintPixels() const noexcept
    {
        return footprintPixels_;
    }
    [[nodiscard]] std::span<const BoundedSupportReplayDigest>
    ReplayDigests() const noexcept
    {
        return replayDigests_;
    }
    [[nodiscard]] const BoundedSupportShapeTotals& Totals() const noexcept
    {
        return totals_;
    }

private:
    BoundedSupportShapeScanResult() = default;
    friend class BoundedSupportShapeScanner;

    std::vector<std::uint8_t> supportFootprint_;
    std::size_t footprintPixels_{0U};
    std::vector<BoundedSupportReplayDigest> replayDigests_;
    BoundedSupportShapeTotals totals_;
};

/**
 * @brief Sequential bounded P3 InternalVoid/Shape/footprint/digest scanner.
 *
 * The final MF-03B1 plan must outlive this scanner. Layers are accepted once,
 * in ascending order from zero. Caller-owned outputs are changed only after a
 * layer has been fully processed and its optional report sink has returned.
 * Base projection, varnish priority, material composition and production
 * routing deliberately remain outside this class.
 */
class BoundedSupportShapeScanner
{
public:
    BoundedSupportShapeScanner(
        const BoundedSupportShapeScanRequest& request,
        const BoundedSupportDemandPlan& finalPlan,
        BoundedSupportShapeReportSink* reportSink = nullptr);

    BoundedSupportShapeScanner(const BoundedSupportShapeScanner&) = delete;
    BoundedSupportShapeScanner(BoundedSupportShapeScanner&&) noexcept = default;
    BoundedSupportShapeScanner& operator=(
        const BoundedSupportShapeScanner&) = delete;
    BoundedSupportShapeScanner& operator=(
        BoundedSupportShapeScanner&&) noexcept = default;

    void ConsumeLayer(
        int layerIndex,
        std::span<const std::uint8_t> modelMask,
        std::span<const std::uint8_t> upperBoundaryMask,
        std::span<std::uint8_t> outputSupportMask,
        std::span<SupportType> outputTypeMap);

    [[nodiscard]] int ExpectedLayerIndex() const noexcept
    {
        return expectedLayerIndex_;
    }

    [[nodiscard]] BoundedSupportShapeScanResult Finish() &&;

private:
    BoundedSupportShapeScanRequest request_;
    const BoundedSupportDemandPlan* finalPlan_{nullptr};
    BoundedSupportShapeReportSink* reportSink_{nullptr};
    std::size_t pixelCount_{0U};
    int expectedLayerIndex_{0};
    bool failed_{false};
    bool finished_{false};
    std::vector<std::uint8_t> supportFootprint_;
    std::size_t footprintPixels_{0U};
    std::vector<BoundedSupportReplayDigest> replayDigests_;
    BoundedSupportShapeTotals totals_;

    std::vector<std::uint8_t> modelScratch_;
    std::vector<std::uint8_t> upperBoundaryScratch_;
    std::vector<std::uint8_t> supportScratch_;
    std::vector<std::uint8_t> originalSupportScratch_;
    std::vector<std::uint8_t> sourceScratch_;
    std::vector<std::uint8_t> addedScratch_;
    std::vector<SupportType> typeScratch_;
    std::vector<std::uint8_t> typeDigestScratch_;
    std::vector<std::uint8_t> externalEmptyScratch_;
    std::vector<std::uint8_t> visitedScratch_;
    std::vector<int> traversalStack_;
    std::vector<int> componentPixels_;
};

}  // namespace slicer_core
