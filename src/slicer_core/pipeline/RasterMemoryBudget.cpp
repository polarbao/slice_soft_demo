#include "slicer_core/pipeline/RasterMemoryBudget.h"

#include <limits>
#include <stdexcept>

namespace slicer_core
{
namespace
{

constexpr std::uint64_t kRgbwsvChannelCount{6U};

std::uint64_t CheckedMultiply(
    const std::uint64_t first,
    const std::uint64_t second,
    const char* field)
{
    if (first != 0U
        && second > std::numeric_limits<std::uint64_t>::max() / first)
    {
        throw std::overflow_error(field);
    }
    return first * second;
}

std::uint64_t CheckedAdd(
    const std::uint64_t first,
    const std::uint64_t second,
    const char* field)
{
    if (second > std::numeric_limits<std::uint64_t>::max() - first)
    {
        throw std::overflow_error(field);
    }
    return first + second;
}

void ValidateRequest(const RasterMemoryBudgetRequest& request)
{
    if (request.widthPx <= 0
        || request.heightPx <= 0
        || request.layerCount <= 0
        || request.visibleInstanceCount <= 0)
    {
        throw std::invalid_argument(
            "raster memory budget dimensions and visible instance count must be positive");
    }
    if (request.boundedWindowLayerCount <= 0
        || request.inFlightInstanceCount <= 0
        || request.inFlightInstanceCount > request.visibleInstanceCount)
    {
        throw std::invalid_argument(
            "raster memory budget stream window and in-flight instance count are invalid");
    }
}

}  // namespace

RasterMemoryBudgetPlan PlanRasterMemoryBudget(
    const RasterMemoryBudgetRequest& request)
{
    ValidateRequest(request);

    RasterMemoryBudgetPlan plan;
    plan.pixelsPerLayer = CheckedMultiply(
        static_cast<std::uint64_t>(request.widthPx),
        static_cast<std::uint64_t>(request.heightPx),
        "raster memory pixels per layer overflow");

    const std::uint64_t retainedScenePixelLayers = CheckedMultiply(
        plan.pixelsPerLayer,
        static_cast<std::uint64_t>(request.layerCount),
        "raster memory retained layer count overflow");
    const std::uint64_t retainedMaskPixelLayers = CheckedMultiply(
        retainedScenePixelLayers,
        static_cast<std::uint64_t>(request.visibleInstanceCount),
        "raster memory retained instance count overflow");
    plan.retainedRgbwsvBytes = CheckedMultiply(
        retainedScenePixelLayers,
        kRgbwsvChannelCount,
        "raster memory retained RGBWSV bytes overflow");
    plan.retainedMaskBytes = CheckedMultiply(
        retainedMaskPixelLayers,
        static_cast<std::uint64_t>(request.maskBytesPerPixel),
        "raster memory retained mask bytes overflow");
    plan.retainedRasterBytes = CheckedAdd(
        plan.retainedRgbwsvBytes,
        plan.retainedMaskBytes,
        "raster memory retained total bytes overflow");

    const std::uint64_t boundedScenePixelLayers = CheckedMultiply(
        plan.pixelsPerLayer,
        static_cast<std::uint64_t>(request.boundedWindowLayerCount),
        "raster memory bounded layer window overflow");
    const std::uint64_t boundedMaskPixelLayers = CheckedMultiply(
        boundedScenePixelLayers,
        static_cast<std::uint64_t>(request.inFlightInstanceCount),
        "raster memory bounded instance window overflow");
    plan.boundedWindowRgbwsvBytes = CheckedMultiply(
        boundedScenePixelLayers,
        kRgbwsvChannelCount,
        "raster memory bounded RGBWSV bytes overflow");
    plan.boundedWindowMaskBytes = CheckedMultiply(
        boundedMaskPixelLayers,
        static_cast<std::uint64_t>(request.maskBytesPerPixel),
        "raster memory bounded mask bytes overflow");
    plan.boundedWindowBytes = CheckedAdd(
        plan.boundedWindowRgbwsvBytes,
        plan.boundedWindowMaskBytes,
        "raster memory bounded total bytes overflow");

    plan.memoryBudgetConfigured = request.memoryBudgetBytes != 0U;
    if (!plan.memoryBudgetConfigured)
    {
        return plan;
    }

    plan.retainedBudgetExceeded =
        plan.retainedRasterBytes > request.memoryBudgetBytes;
    plan.boundedBudgetExceeded =
        plan.boundedWindowBytes > request.memoryBudgetBytes;
    if (!plan.retainedBudgetExceeded)
    {
        plan.reason = "retained_dense_within_budget";
        return plan;
    }
    if (!request.boundedDenseStreamingAvailable)
    {
        plan.route = RasterMemoryRoute::Blocked;
        plan.reason = "bounded_dense_stream_unavailable";
        return plan;
    }
    if (plan.boundedBudgetExceeded)
    {
        plan.route = RasterMemoryRoute::Blocked;
        plan.reason = "bounded_dense_stream_exceeds_budget";
        return plan;
    }

    plan.route = RasterMemoryRoute::BoundedDenseStream;
    plan.reason = "bounded_dense_stream_required";
    return plan;
}

std::string_view RasterMemoryRouteName(
    const RasterMemoryRoute route) noexcept
{
    switch (route)
    {
    case RasterMemoryRoute::RetainedDense:
        return "retained_dense";
    case RasterMemoryRoute::BoundedDenseStream:
        return "bounded_dense_stream";
    case RasterMemoryRoute::Blocked:
        return "blocked";
    }
    return "blocked";
}

}  // namespace slicer_core
