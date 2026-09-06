#pragma once

#include <cstdint>
#include <string_view>

namespace slicer_core
{

/** @brief Internal raster retention route selected before production output starts. */
enum class RasterMemoryRoute
{
    RetainedDense,
    BoundedDenseStream,
    Blocked,
};

/** @brief Inputs used to estimate the dominant RGBWSV and ownership-mask storage. */
struct RasterMemoryBudgetRequest
{
    int widthPx{0};
    int heightPx{0};
    int layerCount{0};
    int visibleInstanceCount{1};
    std::uint32_t maskBytesPerPixel{0U};
    int boundedWindowLayerCount{1};
    int inFlightInstanceCount{1};
    std::uint64_t memoryBudgetBytes{0U};
    bool boundedDenseStreamingAvailable{false};
};

/**
 * @brief Overflow-safe lower-bound estimate for dominant raster storage.
 *
 * Geometry, texture, allocator, report and TIFF-library allocations are not
 * included. Peak working set must still be measured in a separate process.
 */
struct RasterMemoryBudgetPlan
{
    RasterMemoryRoute route{RasterMemoryRoute::RetainedDense};
    std::uint64_t pixelsPerLayer{0U};
    std::uint64_t retainedRgbwsvBytes{0U};
    std::uint64_t retainedMaskBytes{0U};
    std::uint64_t retainedRasterBytes{0U};
    std::uint64_t boundedWindowRgbwsvBytes{0U};
    std::uint64_t boundedWindowMaskBytes{0U};
    std::uint64_t boundedWindowBytes{0U};
    bool memoryBudgetConfigured{false};
    bool retainedBudgetExceeded{false};
    bool boundedBudgetExceeded{false};
    const char* reason{"memory_budget_unset_retained_dense"};
};

/**
 * @brief Estimate dominant raster storage and select only an implemented route.
 * @throws std::invalid_argument for invalid dimensions or stream-window inputs.
 * @throws std::overflow_error when a byte estimate exceeds uint64_t.
 */
RasterMemoryBudgetPlan PlanRasterMemoryBudget(
    const RasterMemoryBudgetRequest& request);

/** @brief Return the stable diagnostic name of a raster memory route. */
std::string_view RasterMemoryRouteName(RasterMemoryRoute route) noexcept;

}  // namespace slicer_core
