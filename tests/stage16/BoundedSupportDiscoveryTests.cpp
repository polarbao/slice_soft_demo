#include "slicer_core/support/BoundedSupportDiscovery.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

using slicer_core::BoundedOuterBoundaryScanRequest;
using slicer_core::BoundedOuterBoundaryScanResult;
using slicer_core::BoundedOuterBoundaryScanner;
using slicer_core::BoundedSupportDemandPlan;
using slicer_core::BoundedSupportDemandRequest;
using slicer_core::BoundedUnsupportedDiscoveryRequest;
using slicer_core::BoundedUnsupportedDiscoveryResult;
using slicer_core::BoundedUnsupportedDiscoveryScanner;
using slicer_core::BoundedUnsupportedLayerEvent;
using slicer_core::BoundedUnsupportedTotals;
using slicer_core::BuildBoundedSupportDemandPlan;
using slicer_core::FinalizeBoundedUnsupportedDemand;
using slicer_core::GeometryOccupancyInputKind;
using slicer_core::MaterializePreShapeSupportLayer;
using slicer_core::OuterVarnishDiscretization;
using slicer_core::SupportType;
using slicer_core::SupportTypePriority;

static_assert(!std::is_copy_constructible_v<BoundedOuterBoundaryScanResult>);
static_assert(!std::is_copy_constructible_v<BoundedUnsupportedDiscoveryResult>);
static_assert(!std::is_copy_constructible_v<BoundedOuterBoundaryScanner>);
static_assert(!std::is_copy_constructible_v<BoundedUnsupportedDiscoveryScanner>);

std::atomic<bool> gTrackAllocations{false};
std::atomic<std::size_t> gAllocationCount{0U};

void RecordAllocation() noexcept
{
    if (gTrackAllocations.load(std::memory_order_relaxed))
    {
        gAllocationCount.fetch_add(1U, std::memory_order_relaxed);
    }
}

std::size_t Index(const int width, const int x, const int y) noexcept
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x);
}

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

template <typename Exception>
bool ExpectThrows(
    const std::function<void()>& operation,
    const std::string& message)
{
    try
    {
        operation();
    }
    catch (const Exception&)
    {
        return true;
    }
    catch (...)
    {
    }
    std::cerr << "FAIL " << message << '\n';
    return false;
}

struct OuterLayerReference
{
    std::vector<std::uint8_t> outer;
    std::vector<std::uint8_t> upper;
};

OuterLayerReference BuildOuterReference(
    const int width,
    const int height,
    const std::span<const std::uint8_t> model,
    const OuterVarnishDiscretization& discretization,
    const bool includeOuter)
{
    const std::size_t count{model.size()};
    std::vector<std::uint8_t> external(count, 0U);
    std::vector<std::uint8_t> dilated(count, 0U);
    std::vector<int> stack;
    stack.reserve(count);
    const auto push = [&](const int x, const int y)
    {
        if (x < 0 || x >= width || y < 0 || y >= height)
        {
            return;
        }
        const std::size_t index{Index(width, x, y)};
        if (model[index] == 0U && external[index] == 0U)
        {
            external[index] = 1U;
            stack.push_back(static_cast<int>(index));
        }
    };
    for (int x{0}; x < width; ++x)
    {
        push(x, 0);
        push(x, height - 1);
    }
    for (int y{0}; y < height; ++y)
    {
        push(0, y);
        push(width - 1, y);
    }
    constexpr std::array<std::array<int, 2>, 8> neighbors{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}},
        {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};
    while (!stack.empty())
    {
        const int current{stack.back()};
        stack.pop_back();
        const int x{current % width};
        const int y{current / width};
        for (const auto& neighbor : neighbors)
        {
            push(x + neighbor[0], y + neighbor[1]);
        }
    }

    if (discretization.enabled)
    {
        for (int y{0}; y < height; ++y)
        {
            for (int x{0}; x < width; ++x)
            {
                if (model[Index(width, x, y)] == 0U)
                {
                    continue;
                }
                for (int deltaY{-discretization.radius_y_px};
                     deltaY <= discretization.radius_y_px;
                     ++deltaY)
                {
                    for (int deltaX{-discretization.radius_x_px};
                         deltaX <= discretization.radius_x_px;
                         ++deltaX)
                    {
                        const int targetX{x + deltaX};
                        const int targetY{y + deltaY};
                        if (targetX < 0 || targetX >= width
                            || targetY < 0 || targetY >= height)
                        {
                            continue;
                        }
                        const double normalizedX{
                            static_cast<double>(deltaX)
                            / static_cast<double>(
                                discretization.radius_x_px)};
                        const double normalizedY{
                            static_cast<double>(deltaY)
                            / static_cast<double>(
                                discretization.radius_y_px)};
                        if (normalizedX * normalizedX
                                + normalizedY * normalizedY
                            <= 1.0 + 1.0e-12)
                        {
                            dilated[Index(width, targetX, targetY)] = 1U;
                        }
                    }
                }
            }
        }
    }

    OuterLayerReference result;
    result.outer.assign(count, 0U);
    result.upper.assign(count, 0U);
    for (std::size_t index{0U}; index < count; ++index)
    {
        if (model[index] == 0U && external[index] != 0U
            && dilated[index] != 0U)
        {
            result.outer[index] = 1U;
        }
        result.upper[index] = model[index] != 0U
                || (includeOuter && result.outer[index] != 0U)
            ? 1U
            : 0U;
    }
    return result;
}

BoundedSupportDemandPlan BuildPlan(
    const int layerCount,
    const std::vector<int>& lower,
    const std::vector<int>& last,
    const std::vector<int>& upper,
    const bool lowerEnabled,
    const bool fullEnabled,
    const bool upperEnabled)
{
    BoundedSupportDemandRequest request;
    request.layerCount = layerCount;
    request.lowerSourceLayers = lower;
    request.modelLastLayers = last;
    request.upperBoundaryLastLayers = upper;
    const std::vector<int> unsupportedTop(lower.size(), 0);
    request.unsupportedTopExclusiveLayers = unsupportedTop;
    request.lowerEnabled = lowerEnabled;
    request.fullVerticalEnabled = fullEnabled;
    request.upperEnabled = upperEnabled;
    request.unsupportedEnabled = false;
    return BuildBoundedSupportDemandPlan(request);
}

void ReferenceSetSupport(
    const std::size_t index,
    const SupportType candidate,
    std::vector<std::uint8_t>& mask,
    std::vector<SupportType>& types)
{
    mask[index] = 1U;
    if (SupportTypePriority(candidate) >= SupportTypePriority(types[index]))
    {
        types[index] = candidate;
    }
}

struct DenseSupportReference
{
    std::vector<std::vector<std::uint8_t>> masks;
    std::vector<std::vector<SupportType>> types;
    std::vector<int> unsupportedTop;
    std::vector<BoundedUnsupportedLayerEvent> events;
    BoundedUnsupportedTotals totals;
};

std::vector<std::uint8_t> DilateBaseReference(
    const int width,
    const int height,
    const std::vector<std::uint8_t>& previousModel,
    const std::vector<std::uint8_t>& previousSupport,
    const int dilation)
{
    std::vector<std::uint8_t> base(previousModel.size(), 0U);
    for (std::size_t index{0U}; index < base.size(); ++index)
    {
        base[index] = previousModel[index] != 0U
                || previousSupport[index] != 0U
            ? 1U
            : 0U;
    }
    for (int iteration{0}; iteration < dilation; ++iteration)
    {
        std::vector<std::uint8_t> next{base};
        for (int y{0}; y < height; ++y)
        {
            for (int x{0}; x < width; ++x)
            {
                if (base[Index(width, x, y)] == 0U)
                {
                    continue;
                }
                for (int dy{-1}; dy <= 1; ++dy)
                {
                    for (int dx{-1}; dx <= 1; ++dx)
                    {
                        const int nx{x + dx};
                        const int ny{y + dy};
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height)
                        {
                            next[Index(width, nx, ny)] = 1U;
                        }
                    }
                }
            }
        }
        base = std::move(next);
    }
    return base;
}

DenseSupportReference BuildDenseReference(
    const int width,
    const int height,
    const std::vector<std::vector<std::uint8_t>>& model,
    const std::vector<std::vector<std::uint8_t>>& upperBoundary,
    const std::vector<int>& lower,
    const std::vector<int>& last,
    const std::vector<int>& upper,
    const bool lowerEnabled,
    const bool fullEnabled,
    const bool upperEnabled,
    const BoundedUnsupportedDiscoveryRequest& policy)
{
    const int layerCount{static_cast<int>(model.size())};
    const std::size_t count{model.front().size()};
    DenseSupportReference result;
    result.masks.assign(
        static_cast<std::size_t>(layerCount),
        std::vector<std::uint8_t>(count, 0U));
    result.types.assign(
        static_cast<std::size_t>(layerCount),
        std::vector<SupportType>(count, SupportType::None));
    result.unsupportedTop.assign(count, 0);
    result.events.resize(static_cast<std::size_t>(layerCount));
    for (int layer{0}; layer < layerCount; ++layer)
    {
        result.events[static_cast<std::size_t>(layer)].layerIndex = layer;
    }

    for (std::size_t index{0U}; index < count; ++index)
    {
        if (lowerEnabled)
        {
            for (int layer{0}; layer < lower[index]; ++layer)
            {
                if (model[static_cast<std::size_t>(layer)][index] == 0U)
                {
                    ReferenceSetSupport(
                        index,
                        SupportType::BottomProjection,
                        result.masks[static_cast<std::size_t>(layer)],
                        result.types[static_cast<std::size_t>(layer)]);
                }
            }
        }
        if (fullEnabled)
        {
            for (int layer{0}; layer < last[index]; ++layer)
            {
                if (model[static_cast<std::size_t>(layer)][index] == 0U)
                {
                    ReferenceSetSupport(
                        index,
                        SupportType::FullVerticalProjection,
                        result.masks[static_cast<std::size_t>(layer)],
                        result.types[static_cast<std::size_t>(layer)]);
                }
            }
        }
        if (upperEnabled && upper[index] >= 0)
        {
            for (int layer{upper[index] + 1}; layer < layerCount; ++layer)
            {
                if (model[static_cast<std::size_t>(layer)][index] == 0U
                    && upperBoundary[static_cast<std::size_t>(layer)][index]
                        == 0U)
                {
                    ReferenceSetSupport(
                        index,
                        SupportType::UpperProjection,
                        result.masks[static_cast<std::size_t>(layer)],
                        result.types[static_cast<std::size_t>(layer)]);
                }
            }
        }
    }

    if (!policy.enabled)
    {
        return result;
    }
    constexpr std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}},
        {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};
    constexpr std::array<std::array<int, 2>, 4> neighbors4{{
        {{0, -1}}, {{-1, 0}}, {{1, 0}}, {{0, 1}},
    }};
    for (int layer{1}; layer < layerCount; ++layer)
    {
        const auto base = DilateBaseReference(
            width,
            height,
            model[static_cast<std::size_t>(layer - 1)],
            result.masks[static_cast<std::size_t>(layer - 1)],
            policy.xyDilationPx);
        std::vector<std::uint8_t> visited(count, 0U);
        auto& event{result.events[static_cast<std::size_t>(layer)]};
        for (std::size_t start{0U}; start < count; ++start)
        {
            if (model[static_cast<std::size_t>(layer)][start] == 0U
                || visited[start] != 0U)
            {
                continue;
            }
            std::vector<int> stack{static_cast<int>(start)};
            std::vector<int> component;
            visited[start] = 1U;
            int overlap{0};
            while (!stack.empty())
            {
                const int current{stack.back()};
                stack.pop_back();
                component.push_back(current);
                overlap += base[static_cast<std::size_t>(current)] != 0U
                    ? 1
                    : 0;
                const int x{current % width};
                const int y{current / width};
                const auto visit = [&](const int nx, const int ny)
                {
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                    {
                        return;
                    }
                    const std::size_t next{Index(width, nx, ny)};
                    if (model[static_cast<std::size_t>(layer)][next] != 0U
                        && visited[next] == 0U)
                    {
                        visited[next] = 1U;
                        stack.push_back(static_cast<int>(next));
                    }
                };
                if (policy.connectivity == 8)
                {
                    for (const auto& neighbor : neighbors8)
                    {
                        visit(x + neighbor[0], y + neighbor[1]);
                    }
                }
                else
                {
                    for (const auto& neighbor : neighbors4)
                    {
                        visit(x + neighbor[0], y + neighbor[1]);
                    }
                }
            }
            const int area{static_cast<int>(component.size())};
            const double ratio{static_cast<double>(overlap)
                / static_cast<double>(area)};
            if (ratio >= policy.minOverlapRatio)
            {
                continue;
            }
            if (area < policy.minIslandAreaPx)
            {
                ++event.filteredIslandCount;
                event.filteredIslandPixels += area;
                continue;
            }
            ++event.islandCount;
            event.islandPixels += area;
            event.unsupportedPixels += area;
            for (int targetLayer{0}; targetLayer < layer; ++targetLayer)
            {
                for (const int pixel : component)
                {
                    const std::size_t index{static_cast<std::size_t>(pixel)};
                    if (model[static_cast<std::size_t>(targetLayer)][index]
                        == 0U)
                    {
                        ReferenceSetSupport(
                            index,
                            SupportType::UnsupportedIsland,
                            result.masks[static_cast<std::size_t>(targetLayer)],
                            result.types[static_cast<std::size_t>(targetLayer)]);
                    }
                    result.unsupportedTop[index] = std::max(
                        result.unsupportedTop[index],
                        layer);
                }
            }
        }
        if (event.islandCount > 0 || event.filteredIslandCount > 0)
        {
            ++result.totals.layersWithIslands;
        }
        result.totals.islandCount += event.islandCount;
        result.totals.islandPixels += event.islandPixels;
        result.totals.unsupportedPixels += event.unsupportedPixels;
        result.totals.filteredIslandCount += event.filteredIslandCount;
        result.totals.filteredIslandPixels += event.filteredIslandPixels;
    }
    return result;
}

void ComputeFacts(
    const std::vector<std::vector<std::uint8_t>>& masks,
    std::vector<int>* const first,
    std::vector<int>* const last)
{
    const std::size_t count{masks.front().size()};
    first->assign(count, -1);
    last->assign(count, -1);
    for (int layer{0}; layer < static_cast<int>(masks.size()); ++layer)
    {
        for (std::size_t index{0U}; index < count; ++index)
        {
            if (masks[static_cast<std::size_t>(layer)][index] != 0U)
            {
                if ((*first)[index] < 0)
                {
                    (*first)[index] = layer;
                }
                (*last)[index] = layer;
            }
        }
    }
}

bool OuterBoundaryMatchesIndependentReference()
{
    constexpr int width{9};
    constexpr int height{7};
    constexpr int layers{3};
    const std::size_t count{static_cast<std::size_t>(width * height)};
    std::vector<std::vector<std::uint8_t>> model(
        layers,
        std::vector<std::uint8_t>(count, 0U));
    for (int x{2}; x <= 6; ++x)
    {
        model[1][Index(width, x, 1)] = 1U;
        model[1][Index(width, x, 5)] = 1U;
    }
    for (int y{1}; y <= 5; ++y)
    {
        model[1][Index(width, 2, y)] = 1U;
        model[1][Index(width, 6, y)] = 1U;
    }
    model[2][Index(width, 0, 3)] = 1U;
    model[2][Index(width, 8, 3)] = 1U;

    OuterVarnishDiscretization outer;
    outer.enabled = true;
    outer.requested_thickness_mm = 0.20;
    outer.pixel_size_x_mm = 0.10;
    outer.pixel_size_y_mm = 0.20;
    outer.radius_x_px = 2;
    outer.radius_y_px = 1;
    outer.effective_thickness_x_mm = 0.20;
    outer.effective_thickness_y_mm = 0.20;
    BoundedOuterBoundaryScanRequest request;
    request.widthPx = width;
    request.heightPx = height;
    request.layerCount = layers;
    request.outerVarnish = outer;
    request.includeOuterVarnishInUpperBoundary = true;
    BoundedOuterBoundaryScanner scanner{request};
    std::vector<std::uint8_t> actualOuter(count, 7U);
    std::vector<std::uint8_t> actualUpper(count, 7U);
    std::vector<int> expectedLast(count, -1);
    const auto* const outerAddress{actualOuter.data()};
    const auto* const upperAddress{actualUpper.data()};
    bool passed{true};
    std::size_t hotPathAllocations{0U};
    for (int layer{0}; layer < layers; ++layer)
    {
        const auto expected = BuildOuterReference(
            width,
            height,
            model[static_cast<std::size_t>(layer)],
            outer,
            true);
        gAllocationCount.store(0U, std::memory_order_relaxed);
        gTrackAllocations.store(true, std::memory_order_relaxed);
        scanner.ConsumeLayer(layer, model[static_cast<std::size_t>(layer)], actualOuter, actualUpper);
        gTrackAllocations.store(false, std::memory_order_relaxed);
        hotPathAllocations += gAllocationCount.load(std::memory_order_relaxed);
        passed = ExpectTrue(actualOuter == expected.outer, "outer mask matches retained formula") && passed;
        passed = ExpectTrue(actualUpper == expected.upper, "upper boundary matches retained formula") && passed;
        passed = ExpectTrue(actualOuter.data() == outerAddress && actualUpper.data() == upperAddress,
                            "caller output buffers are reused") && passed;
        for (std::size_t index{0U}; index < count; ++index)
        {
            if (expected.upper[index] != 0U)
            {
                expectedLast[index] = layer;
            }
        }
    }
    auto result = std::move(scanner).Finish();
    passed = ExpectTrue(
                 std::vector<int>(result.UpperBoundaryLastLayers().begin(), result.UpperBoundaryLastLayers().end())
                     == expectedLast,
                 "upper boundary last-layer facts match") && passed;
    passed = ExpectTrue(actualOuter[Index(width, 4, 3)] == 0U,
                        "closed internal hole is not outer varnish") && passed;
    passed = ExpectTrue(hotPathAllocations == 0U,
                        "outer ConsumeLayer hot path performs no heap allocation") && passed;
    return passed;
}

bool OuterBoundaryValidationIsRetryable()
{
    BoundedOuterBoundaryScanRequest request;
    request.widthPx = 3;
    request.heightPx = 3;
    request.layerCount = 1;
    BoundedOuterBoundaryScanner scanner{request};
    std::vector<std::uint8_t> model(9U, 0U);
    std::vector<std::uint8_t> outer(9U, 5U);
    std::vector<std::uint8_t> upper(9U, 6U);
    model[0] = 2U;
    bool passed = ExpectThrows<std::invalid_argument>(
        [&] { scanner.ConsumeLayer(0, model, outer, upper); },
        "non-binary outer input fails");
    passed = ExpectTrue(scanner.ExpectedLayerIndex() == 0,
                        "invalid outer input does not advance state") && passed;
    passed = ExpectTrue(outer.front() == 5U && upper.front() == 6U,
                        "invalid outer input keeps outputs unchanged") && passed;
    model[0] = 1U;
    passed = ExpectThrows<std::invalid_argument>(
        [&] { scanner.ConsumeLayer(0, model, outer, outer); },
        "aliased outer outputs fail") && passed;
    scanner.ConsumeLayer(0, model, outer, upper);
    passed = ExpectTrue(scanner.ExpectedLayerIndex() == 1,
                        "valid retry advances outer scanner") && passed;

    BoundedOuterBoundaryScanRequest incompleteRequest{request};
    incompleteRequest.layerCount = 2;
    BoundedOuterBoundaryScanner incomplete{incompleteRequest};
    passed = ExpectThrows<std::logic_error>(
        [&] { static_cast<void>(std::move(incomplete).Finish()); },
        "early outer Finish fails") && passed;

    request.inputKind = GeometryOccupancyInputKind::GeneralMesh;
    passed = ExpectThrows<std::invalid_argument>(
        [&] { BoundedOuterBoundaryScanner rejected{request}; },
        "GeneralMesh outer scan fails closed") && passed;
    request.inputKind = GeometryOccupancyInputKind::SingleIntervalHeightfield;
    request.includeOuterVarnishInUpperBoundary = true;
    passed = ExpectThrows<std::invalid_argument>(
        [&] { BoundedOuterBoundaryScanner rejected{request}; },
        "disabled outer cannot extend upper boundary") && passed;
    return passed;
}

bool UnsupportedDiscoveryMatchesDenseRetainedOracle()
{
    constexpr int width{11};
    constexpr int height{8};
    constexpr int layers{6};
    const std::size_t count{static_cast<std::size_t>(width * height)};
    std::vector<std::vector<std::uint8_t>> model(
        layers,
        std::vector<std::uint8_t>(count, 0U));
    const auto fillRect = [&](const int layer, const int minX, const int minY,
                              const int maxX, const int maxY)
    {
        for (int y{minY}; y <= maxY; ++y)
        {
            for (int x{minX}; x <= maxX; ++x)
            {
                model[static_cast<std::size_t>(layer)][Index(width, x, y)] = 1U;
            }
        }
    };
    for (int layer{0}; layer < layers; ++layer)
    {
        fillRect(layer, 1, 1, 2, 2);
    }
    fillRect(1, 7, 1, 8, 2);
    fillRect(2, 7, 1, 8, 2);
    fillRect(2, 8, 5, 9, 6);
    fillRect(3, 8, 5, 9, 6);
    model[4][Index(width, 5, 5)] = 1U;

    std::vector<std::vector<std::uint8_t>> upperBoundary{model};
    upperBoundary[2][Index(width, 3, 1)] = 1U;
    upperBoundary[3][Index(width, 3, 1)] = 1U;
    std::vector<int> lower;
    std::vector<int> last;
    std::vector<int> ignored;
    std::vector<int> upper;
    ComputeFacts(model, &lower, &last);
    ComputeFacts(upperBoundary, &ignored, &upper);

    auto plan = BuildPlan(layers, lower, last, upper, true, true, true);
    BoundedUnsupportedDiscoveryRequest request;
    request.widthPx = width;
    request.heightPx = height;
    request.layerCount = layers;
    request.enabled = true;
    request.connectivity = 8;
    request.xyDilationPx = 1;
    request.minOverlapRatio = 0.25;
    request.minIslandAreaPx = 2;
    const auto expected = BuildDenseReference(
        width, height, model, upperBoundary, lower, last, upper,
        true, true, true, request);

    BoundedUnsupportedDiscoveryScanner scanner{request, plan};
    gAllocationCount.store(0U, std::memory_order_relaxed);
    gTrackAllocations.store(true, std::memory_order_relaxed);
    for (int layer{0}; layer < layers; ++layer)
    {
        scanner.ConsumeLayer(
            layer,
            model[static_cast<std::size_t>(layer)],
            upperBoundary[static_cast<std::size_t>(layer)]);
    }
    gTrackAllocations.store(false, std::memory_order_relaxed);
    auto result = std::move(scanner).Finish();
    bool passed = ExpectTrue(
        gAllocationCount.load(std::memory_order_relaxed) == 0U,
        "unsupported ConsumeLayer hot path performs no heap allocation");
    passed = ExpectTrue(
        std::vector<int>(result.UnsupportedTopExclusiveLayers().begin(), result.UnsupportedTopExclusiveLayers().end())
            == expected.unsupportedTop,
        "unsupported compact top facts match retained oracle") && passed;
    const auto actualEvents = result.LayerEvents();
    for (std::size_t layer{0U}; layer < actualEvents.size(); ++layer)
    {
        const auto& actual{actualEvents[layer]};
        const auto& reference{expected.events[layer]};
        passed = ExpectTrue(
            actual.layerIndex == reference.layerIndex
                && actual.islandCount == reference.islandCount
                && actual.islandPixels == reference.islandPixels
                && actual.unsupportedPixels == reference.unsupportedPixels
                && actual.filteredIslandCount == reference.filteredIslandCount
                && actual.filteredIslandPixels == reference.filteredIslandPixels,
            "per-source-layer unsupported diagnostics match retained oracle") && passed;
    }
    const auto& totals{result.Totals()};
    passed = ExpectTrue(
        totals.layersWithIslands == expected.totals.layersWithIslands
            && totals.islandCount == expected.totals.islandCount
            && totals.islandPixels == expected.totals.islandPixels
            && totals.unsupportedPixels == expected.totals.unsupportedPixels
            && totals.filteredIslandCount == expected.totals.filteredIslandCount
            && totals.filteredIslandPixels == expected.totals.filteredIslandPixels,
        "unsupported aggregate diagnostics match retained oracle") && passed;

    auto finalPlan = FinalizeBoundedUnsupportedDemand(
        std::move(plan),
        std::vector<int>(result.UnsupportedTopExclusiveLayers().begin(), result.UnsupportedTopExclusiveLayers().end()));
    std::vector<std::uint8_t> actualMask(count, 0U);
    std::vector<SupportType> actualTypes(count, SupportType::None);
    for (int layer{0}; layer < layers; ++layer)
    {
        MaterializePreShapeSupportLayer(
            finalPlan,
            layer,
            model[static_cast<std::size_t>(layer)],
            upperBoundary[static_cast<std::size_t>(layer)],
            actualMask,
            actualTypes);
        passed = ExpectTrue(actualMask == expected.masks[static_cast<std::size_t>(layer)],
                            "final bounded support mask matches retained oracle") && passed;
        passed = ExpectTrue(actualTypes == expected.types[static_cast<std::size_t>(layer)],
                            "final bounded support type map matches retained oracle") && passed;
    }
    return passed;
}

BoundedUnsupportedDiscoveryResult RunConnectivityCase(
    const int connectivity,
    const int minArea,
    const double overlap)
{
    constexpr int width{3};
    constexpr int height{3};
    constexpr int layers{2};
    const std::size_t count{9U};
    std::vector<int> lower(count, -1);
    std::vector<int> last(count, -1);
    std::vector<int> upper(count, -1);
    lower[Index(width, 0, 0)] = 1;
    last[Index(width, 0, 0)] = 1;
    lower[Index(width, 1, 1)] = 1;
    last[Index(width, 1, 1)] = 1;
    auto plan = BuildPlan(layers, lower, last, upper, false, false, false);
    BoundedUnsupportedDiscoveryRequest request;
    request.widthPx = width;
    request.heightPx = height;
    request.layerCount = layers;
    request.enabled = true;
    request.connectivity = connectivity;
    request.minIslandAreaPx = minArea;
    request.minOverlapRatio = overlap;
    BoundedUnsupportedDiscoveryScanner scanner{request, plan};
    std::vector<std::uint8_t> empty(count, 0U);
    std::vector<std::uint8_t> diagonal(count, 0U);
    diagonal[Index(width, 0, 0)] = 1U;
    diagonal[Index(width, 1, 1)] = 1U;
    scanner.ConsumeLayer(0, empty, empty);
    scanner.ConsumeLayer(1, diagonal, diagonal);
    return std::move(scanner).Finish();
}

bool ConnectivityAndThresholdBoundaries()
{
    const auto four = RunConnectivityCase(4, 2, 0.5);
    const auto eight = RunConnectivityCase(8, 2, 0.5);
    bool passed = ExpectTrue(
        four.Totals().filteredIslandCount == 2
            && four.Totals().islandCount == 0,
        "4-connectivity keeps diagonal pixels separate and filtered");
    passed = ExpectTrue(
        eight.Totals().islandCount == 1
            && eight.Totals().filteredIslandCount == 0,
        "8-connectivity joins diagonal pixels at exact minimum area") && passed;
    return passed;
}

BoundedUnsupportedDiscoveryResult RunOverlapBoundaryCase(
    const double minOverlap,
    const int minArea)
{
    constexpr int width{3};
    constexpr int layers{2};
    std::vector<int> lower{0, 1, -1};
    std::vector<int> last{1, 1, -1};
    std::vector<int> upper{-1, -1, -1};
    auto plan = BuildPlan(layers, lower, last, upper, false, false, false);
    BoundedUnsupportedDiscoveryRequest request;
    request.widthPx = width;
    request.heightPx = 1;
    request.layerCount = layers;
    request.enabled = true;
    request.connectivity = 4;
    request.minIslandAreaPx = minArea;
    request.minOverlapRatio = minOverlap;
    BoundedUnsupportedDiscoveryScanner scanner{request, plan};
    const std::vector<std::uint8_t> base{1U, 0U, 0U};
    const std::vector<std::uint8_t> component{1U, 1U, 0U};
    scanner.ConsumeLayer(0, base, base);
    scanner.ConsumeLayer(1, component, component);
    return std::move(scanner).Finish();
}

bool StrictOverlapAndAreaBoundaries()
{
    const auto equalOverlap = RunOverlapBoundaryCase(0.5, 2);
    const auto belowOverlap = RunOverlapBoundaryCase(0.5001, 2);
    const auto belowArea = RunOverlapBoundaryCase(0.5001, 3);
    bool passed = ExpectTrue(
        equalOverlap.Totals().islandCount == 0,
        "overlap equal to threshold is supported");
    passed = ExpectTrue(
        belowOverlap.Totals().islandCount == 1
            && belowOverlap.Totals().filteredIslandCount == 0,
        "area equal to minimum is accepted when overlap is strictly below") && passed;
    passed = ExpectTrue(
        belowArea.Totals().islandCount == 0
            && belowArea.Totals().filteredIslandCount == 1,
        "area strictly below minimum is filtered") && passed;
    return passed;
}

bool PreviousUnsupportedDoesNotSupportNextSourceLayer()
{
    constexpr int width{2};
    constexpr int height{1};
    constexpr int layers{4};
    std::vector<int> lower{1, -1};
    std::vector<int> last{3, -1};
    std::vector<int> upper{-1, -1};
    auto plan = BuildPlan(layers, lower, last, upper, false, false, false);
    BoundedUnsupportedDiscoveryRequest request;
    request.widthPx = width;
    request.heightPx = height;
    request.layerCount = layers;
    request.enabled = true;
    request.connectivity = 4;
    request.minIslandAreaPx = 1;
    request.minOverlapRatio = 1.0;
    BoundedUnsupportedDiscoveryScanner scanner{request, plan};
    const std::vector<std::uint8_t> empty{0U, 0U};
    const std::vector<std::uint8_t> island{1U, 0U};
    scanner.ConsumeLayer(0, empty, empty);
    scanner.ConsumeLayer(1, island, island);
    scanner.ConsumeLayer(2, empty, empty);
    scanner.ConsumeLayer(3, island, island);
    const auto result = std::move(scanner).Finish();
    return ExpectTrue(
        result.LayerEvents()[1].islandCount == 1
            && result.LayerEvents()[3].islandCount == 1
            && result.UnsupportedTopExclusiveLayers()[0] == 3,
        "earlier unsupported demand is excluded from the next previous base");
}

bool UnsupportedValidationIsRetryable()
{
    constexpr int width{2};
    constexpr int layers{2};
    std::vector<int> lower{1, -1};
    std::vector<int> last{1, -1};
    std::vector<int> upper{-1, -1};
    auto plan = BuildPlan(layers, lower, last, upper, false, false, false);
    BoundedUnsupportedDiscoveryRequest request;
    request.widthPx = width;
    request.heightPx = 1;
    request.layerCount = layers;
    request.enabled = true;
    request.connectivity = 4;
    request.minIslandAreaPx = 1;
    BoundedUnsupportedDiscoveryScanner scanner{request, plan};
    std::vector<std::uint8_t> invalid{2U, 0U};
    std::vector<std::uint8_t> valid{0U, 0U};
    bool passed = ExpectThrows<std::invalid_argument>(
        [&] { scanner.ConsumeLayer(0, invalid, valid); },
        "non-binary unsupported input fails");
    passed = ExpectTrue(scanner.ExpectedLayerIndex() == 0,
                        "invalid unsupported input does not advance") && passed;
    passed = ExpectThrows<std::invalid_argument>(
        [&] { scanner.ConsumeLayer(1, valid, valid); },
        "out-of-order unsupported layer fails") && passed;
    const std::vector<std::uint8_t> modelWithoutBoundary{1U, 0U};
    passed = ExpectThrows<std::invalid_argument>(
        [&] { scanner.ConsumeLayer(0, modelWithoutBoundary, valid); },
        "upper boundary that omits model occupancy fails") && passed;
    passed = ExpectTrue(scanner.ExpectedLayerIndex() == 0,
                        "boundary containment failure remains retryable") && passed;
    scanner.ConsumeLayer(0, valid, valid);
    scanner.ConsumeLayer(1, valid, valid);
    static_cast<void>(std::move(scanner).Finish());

    auto incompletePlan = BuildPlan(layers, lower, last, upper, false, false, false);
    BoundedUnsupportedDiscoveryScanner incomplete{request, incompletePlan};
    incomplete.ConsumeLayer(0, valid, valid);
    passed = ExpectThrows<std::logic_error>(
        [&] { static_cast<void>(std::move(incomplete).Finish()); },
        "early unsupported Finish fails") && passed;

    request.connectivity = 6;
    passed = ExpectThrows<std::invalid_argument>(
        [&] { BoundedUnsupportedDiscoveryScanner rejected{request, incompletePlan}; },
        "invalid connectivity fails closed") && passed;
    request.connectivity = 4;
    request.inputKind = GeometryOccupancyInputKind::GeneralMesh;
    passed = ExpectThrows<std::invalid_argument>(
        [&] { BoundedUnsupportedDiscoveryScanner rejected{request, incompletePlan}; },
        "GeneralMesh unsupported discovery fails closed") && passed;
    return passed;
}

}  // namespace

void* operator new(const std::size_t size)
{
    RecordAllocation();
    if (void* const memory{std::malloc(size)})
    {
        return memory;
    }
    throw std::bad_alloc{};
}

void* operator new[](const std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* const memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* const memory) noexcept
{
    ::operator delete(memory);
}

void operator delete(void* const memory, const std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* const memory, const std::size_t) noexcept
{
    ::operator delete(memory);
}

int main()
{
    const std::array<std::pair<const char*, std::function<bool()>>, 7> tests{{
        {"outer_boundary_retained_oracle", OuterBoundaryMatchesIndependentReference},
        {"outer_boundary_validation_retry", OuterBoundaryValidationIsRetryable},
        {"unsupported_dense_retained_oracle", UnsupportedDiscoveryMatchesDenseRetainedOracle},
        {"unsupported_connectivity_thresholds", ConnectivityAndThresholdBoundaries},
        {"unsupported_strict_threshold_boundaries", StrictOverlapAndAreaBoundaries},
        {"unsupported_previous_base_invariant", PreviousUnsupportedDoesNotSupportNextSourceLayer},
        {"unsupported_validation_retry", UnsupportedValidationIsRetryable},
    }};
    int failed{0};
    for (const auto& [name, test] : tests)
    {
        try
        {
            if (!test())
            {
                ++failed;
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
            ++failed;
        }
    }
    if (failed == 0)
    {
        std::cout << "PASS BoundedSupportDiscoveryTests " << tests.size() << "/"
                  << tests.size() << '\n';
        return 0;
    }
    std::cerr << "FAIL BoundedSupportDiscoveryTests " << failed << " failed\n";
    return 1;
}
