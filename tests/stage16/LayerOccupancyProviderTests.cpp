#include "slicer_core/geometry/LayerOccupancyProvider.h"

#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using slicer_core::BuildLayerOccupancy;
using slicer_core::GeometryOccupancyColumn;
using slicer_core::LayerOccupancyMode;
using slicer_core::LayerOccupancyRequest;
using slicer_core::LayerOccupancyResult;
using slicer_core::MakeLegacyGeometryOccupancyPolicy;
using slicer_core::XyCoverageMode;

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool ExpectThrowsInvalidArgument(const std::function<void()>& operation, const std::string& message)
{
    try
    {
        operation();
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    catch (...)
    {
    }
    std::cerr << "FAIL " << message << '\n';
    return false;
}

bool DefaultPolicyIsLegacyOnly()
{
    const auto policy = MakeLegacyGeometryOccupancyPolicy();
    return ExpectTrue(policy.layerMode == LayerOccupancyMode::LegacyCenterSample, "legacy layer mode")
        && ExpectTrue(policy.xyMode == XyCoverageMode::PixelCenter, "legacy XY mode")
        && ExpectTrue(policy.minimumCoveredSubsamples == 1U, "legacy sample threshold");
}

bool LegacyProviderMatchesCenterSampleRanges()
{
    const std::vector<GeometryOccupancyColumn> columns{
        {true, 0.00, 0.20},
        {true, 0.051, 0.149},
        {true, 0.10, 0.16},
        {false, 0.00, 0.00},
        {true, -0.25, 0.55},
    };
    LayerOccupancyRequest request;
    request.columns = columns;
    request.layerCount = 5;
    request.layerThicknessMm = 0.1;
    request.policy = MakeLegacyGeometryOccupancyPolicy();

    const LayerOccupancyResult result = BuildLayerOccupancy(request);
    const std::vector<int> expectedFirst{0, -1, 1, -1, 0};
    const std::vector<int> expectedLast{1, -1, 1, -1, 4};
    std::vector<std::vector<std::uint8_t>> expected(
        static_cast<std::size_t>(request.layerCount),
        std::vector<std::uint8_t>(columns.size(), 0));
    for (std::size_t columnIndex{0}; columnIndex < columns.size(); ++columnIndex)
    {
        for (int layerIndex{expectedFirst[columnIndex]}; layerIndex >= 0 && layerIndex <= expectedLast[columnIndex]; ++layerIndex)
        {
            expected[static_cast<std::size_t>(layerIndex)][columnIndex] = 1;
        }
    }
    bool passed = ExpectTrue(result.masks == expected, "legacy masks preserve center-sample interval semantics");
    if (!passed)
    {
        for (std::size_t layerIndex{0}; layerIndex < result.masks.size(); ++layerIndex)
        {
            std::cerr << "actual layer " << layerIndex << ':';
            for (const std::uint8_t value : result.masks[layerIndex])
            {
                std::cerr << ' ' << static_cast<int>(value);
            }
            std::cerr << '\n';
        }
    }
    return passed
        && ExpectTrue(
            result.firstOccupiedLayers == expectedFirst,
            "first occupied layer ranges")
        && ExpectTrue(
            result.lastOccupiedLayers == expectedLast,
            "last occupied layer ranges");
}

bool UnsupportedCandidatesFailClosed()
{
    const std::vector<GeometryOccupancyColumn> columns{{true, 0.0, 0.2}};
    LayerOccupancyRequest request;
    request.columns = columns;
    request.layerCount = 2;
    request.layerThicknessMm = 0.1;

    request.policy = MakeLegacyGeometryOccupancyPolicy();
    request.policy.layerMode = LayerOccupancyMode::LayerSlabCoverage;
    bool passed = ExpectThrowsInvalidArgument(
        [&request]() { (void)BuildLayerOccupancy(request); },
        "Layer Slab must remain unavailable before 16A-03");

    request.policy = MakeLegacyGeometryOccupancyPolicy();
    request.policy.xyMode = XyCoverageMode::Supersample2x2;
    passed = ExpectThrowsInvalidArgument(
                 [&request]() { (void)BuildLayerOccupancy(request); },
                 "2x2 must remain unavailable before 16A-04")
        && passed;
    return passed;
}

bool InvalidInputsFailClosed()
{
    LayerOccupancyRequest request;
    request.layerCount = 0;
    request.layerThicknessMm = 0.1;
    bool passed = ExpectThrowsInvalidArgument(
        [&request]() { (void)BuildLayerOccupancy(request); },
        "non-positive layer count");

    const std::vector<GeometryOccupancyColumn> invalidColumns{{true, 0.2, 0.1}};
    request.columns = invalidColumns;
    request.layerCount = 2;
    passed = ExpectThrowsInvalidArgument(
                 [&request]() { (void)BuildLayerOccupancy(request); },
                 "reversed column interval")
        && passed;
    return passed;
}

}  // namespace

int main()
{
    try
    {
        const bool passed = DefaultPolicyIsLegacyOnly()
            && LegacyProviderMatchesCenterSampleRanges()
            && UnsupportedCandidatesFailClosed()
            && InvalidInputsFailClosed();
        if (!passed)
        {
            return 1;
        }
        std::cout << "Stage 16 layer occupancy provider tests complete.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL exception=" << error.what() << '\n';
        return 1;
    }
}
