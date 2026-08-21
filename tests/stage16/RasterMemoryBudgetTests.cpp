#include "slicer_core/pipeline/RasterMemoryBudget.h"

#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using slicer_core::PlanRasterMemoryBudget;
using slicer_core::RasterMemoryBudgetRequest;
using slicer_core::RasterMemoryRoute;
using slicer_core::RasterMemoryRouteName;

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
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

RasterMemoryBudgetRequest MakeRequest()
{
    RasterMemoryBudgetRequest request;
    request.widthPx = 100;
    request.heightPx = 100;
    request.layerCount = 10;
    request.visibleInstanceCount = 1;
    request.maskBytesPerPixel = 4U;
    request.boundedWindowLayerCount = 3;
    request.inFlightInstanceCount = 1;
    return request;
}

bool UnsetBudgetPreservesRetainedDense()
{
    const auto plan = PlanRasterMemoryBudget(MakeRequest());
    return ExpectTrue(
               plan.route == RasterMemoryRoute::RetainedDense,
               "unset budget keeps the current retained route")
        && ExpectTrue(
            !plan.memoryBudgetConfigured,
            "unset budget remains explicit")
        && ExpectTrue(
            std::string_view{plan.reason}
                == "memory_budget_unset_retained_dense",
            "unset budget reason")
        && ExpectTrue(
            plan.retainedRasterBytes == 1'000'000U,
            "retained RGBWSV plus four mask bytes")
        && ExpectTrue(
            plan.boundedWindowBytes == 300'000U,
            "three-layer bounded estimate");
}

bool SmallGridWithinBudgetStaysRetained()
{
    RasterMemoryBudgetRequest request = MakeRequest();
    request.memoryBudgetBytes = 2'000'000U;
    request.boundedDenseStreamingAvailable = true;
    const auto plan = PlanRasterMemoryBudget(request);
    return ExpectTrue(
               plan.route == RasterMemoryRoute::RetainedDense,
               "small grid remains retained")
        && ExpectTrue(
            !plan.retainedBudgetExceeded,
            "small retained grid is within budget")
        && ExpectTrue(
            std::string_view{plan.reason}
                == "retained_dense_within_budget",
            "within-budget reason");
}

bool SceneRgbwsvAndPerInstanceMasksAreCountedSeparately()
{
    RasterMemoryBudgetRequest request = MakeRequest();
    request.visibleInstanceCount = 2;
    const auto plan = PlanRasterMemoryBudget(request);
    return ExpectTrue(
               plan.retainedRgbwsvBytes == 600'000U,
               "scene RGBWSV storage is not multiplied by instance count")
        && ExpectTrue(
            plan.retainedMaskBytes == 800'000U,
            "retained mask storage is counted per visible instance")
        && ExpectTrue(
            plan.boundedWindowBytes == 300'000U,
            "bounded scene RGBWSV plus one in-flight instance mask");
}

bool LargeSparseModelSelectsAvailableBoundedRoute()
{
    RasterMemoryBudgetRequest request;
    request.widthPx = 5197;
    request.heightPx = 1418;
    request.layerCount = 141;
    request.visibleInstanceCount = 1;
    request.maskBytesPerPixel = 4U;
    request.boundedWindowLayerCount = 3;
    request.inFlightInstanceCount = 1;
    request.memoryBudgetBytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
    request.boundedDenseStreamingAvailable = true;

    const auto plan = PlanRasterMemoryBudget(request);
    return ExpectTrue(
               plan.pixelsPerLayer == 7'369'346U,
               "123.stl observed pixels per layer")
        && ExpectTrue(
            plan.retainedRgbwsvBytes == 6'234'466'716U,
            "123.stl retained RGBWSV bytes")
        && ExpectTrue(
            plan.retainedRasterBytes == 10'390'777'860U,
            "123.stl retained lower-bound bytes")
        && ExpectTrue(
            plan.boundedWindowBytes == 221'080'380U,
            "123.stl three-layer bounded lower-bound bytes")
        && ExpectTrue(
            plan.route == RasterMemoryRoute::BoundedDenseStream,
            "large model selects the implemented bounded route")
        && ExpectTrue(
            std::string_view{plan.reason}
                == "bounded_dense_stream_required",
            "bounded-route reason");
}

bool OverBudgetWithoutCapabilityFailsClosed()
{
    RasterMemoryBudgetRequest request = MakeRequest();
    request.memoryBudgetBytes = 500'000U;
    const auto plan = PlanRasterMemoryBudget(request);
    return ExpectTrue(
               plan.route == RasterMemoryRoute::Blocked,
               "over-budget route blocks when streaming is unavailable")
        && ExpectTrue(
            std::string_view{plan.reason}
                == "bounded_dense_stream_unavailable",
            "unavailable-route reason");
}

bool BoundedWindowMustAlsoFitBudget()
{
    RasterMemoryBudgetRequest request = MakeRequest();
    request.memoryBudgetBytes = 200'000U;
    request.boundedDenseStreamingAvailable = true;
    const auto plan = PlanRasterMemoryBudget(request);
    return ExpectTrue(
               plan.route == RasterMemoryRoute::Blocked,
               "bounded route blocks when its window exceeds budget")
        && ExpectTrue(
            plan.boundedBudgetExceeded,
            "bounded over-budget state is explicit")
        && ExpectTrue(
            std::string_view{plan.reason}
                == "bounded_dense_stream_exceeds_budget",
            "bounded-over-budget reason");
}

bool InvalidAndOverflowInputsFailClosed()
{
    RasterMemoryBudgetRequest request = MakeRequest();
    request.widthPx = 0;
    bool passed = ExpectThrows<std::invalid_argument>(
        [&request]() { (void)PlanRasterMemoryBudget(request); },
        "zero width must fail");

    request = MakeRequest();
    request.inFlightInstanceCount = 2;
    passed = ExpectThrows<std::invalid_argument>(
                 [&request]() { (void)PlanRasterMemoryBudget(request); },
                 "in-flight instances cannot exceed visible instances")
        && passed;

    request.widthPx = std::numeric_limits<int>::max();
    request.heightPx = std::numeric_limits<int>::max();
    request.layerCount = std::numeric_limits<int>::max();
    request.visibleInstanceCount = std::numeric_limits<int>::max();
    request.inFlightInstanceCount = 1;
    passed = ExpectThrows<std::overflow_error>(
                 [&request]() { (void)PlanRasterMemoryBudget(request); },
                 "unrepresentable retained bytes must fail")
        && passed;
    return passed;
}

bool RouteNamesAreStable()
{
    return ExpectTrue(
               RasterMemoryRouteName(RasterMemoryRoute::RetainedDense)
                   == "retained_dense",
               "retained route name")
        && ExpectTrue(
            RasterMemoryRouteName(RasterMemoryRoute::BoundedDenseStream)
                == "bounded_dense_stream",
            "bounded route name")
        && ExpectTrue(
            RasterMemoryRouteName(RasterMemoryRoute::Blocked) == "blocked",
            "blocked route name");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, std::function<bool()>>> tests{
        {"unset_budget_preserves_retained_dense", UnsetBudgetPreservesRetainedDense},
        {"small_grid_within_budget_stays_retained", SmallGridWithinBudgetStaysRetained},
        {"scene_rgbwsv_and_per_instance_masks_are_counted_separately", SceneRgbwsvAndPerInstanceMasksAreCountedSeparately},
        {"large_sparse_model_selects_available_bounded_route", LargeSparseModelSelectsAvailableBoundedRoute},
        {"over_budget_without_capability_fails_closed", OverBudgetWithoutCapabilityFailsClosed},
        {"bounded_window_must_also_fit_budget", BoundedWindowMustAlsoFitBudget},
        {"invalid_and_overflow_inputs_fail_closed", InvalidAndOverflowInputsFailClosed},
        {"route_names_are_stable", RouteNamesAreStable},
    };

    int failed{0};
    for (const auto& [name, test] : tests)
    {
        try
        {
            if (test())
            {
                std::cout << "PASS " << name << '\n';
            }
            else
            {
                ++failed;
            }
        }
        catch (const std::exception& exception)
        {
            std::cerr << "FAIL " << name << ": " << exception.what() << '\n';
            ++failed;
        }
    }
    return failed == 0 ? 0 : 1;
}
