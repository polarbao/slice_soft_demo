#include "slicer_core/geometry/LayerOccupancyProvider.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "slicer_core/config.h"

namespace
{

using slicer_core::BuildLayerOccupancy;
using slicer_core::GeometryOccupancyColumn;
using slicer_core::GeometryOccupancyInputKind;
using slicer_core::LayerOccupancyMode;
using slicer_core::LayerOccupancyRequest;
using slicer_core::LayerOccupancyResult;
using slicer_core::MakeLegacyGeometryOccupancyPolicy;
using slicer_core::MakeLayerSlabGeometryOccupancyPolicy;
using slicer_core::MakeLayerSlabSupersample2x2GeometryOccupancyPolicy;
using slicer_core::SliceConfig;
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

bool ExpectThrowsRuntimeError(const std::function<void()>& operation, const std::string& message)
{
    try
    {
        operation();
    }
    catch (const std::runtime_error&)
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

bool LayerSlabProviderMatchesHalfOpenRanges()
{
    const std::vector<GeometryOccupancyColumn> columns{
        {true, 0.0, 0.125},
        {true, 0.0, 0.375},
        {true, 0.0, 0.625},
        {true, 0.0, 0.875},
        {true, 0.25, 0.50},
        {true, 0.50, 0.50},
    };
    LayerOccupancyRequest request;
    request.columns = columns;
    request.layerCount = 4;
    request.layerThicknessMm = 0.25;
    request.policy = MakeLayerSlabGeometryOccupancyPolicy();

    const LayerOccupancyResult result = BuildLayerOccupancy(request);
    const std::vector<std::vector<std::uint8_t>> expected{
        {1, 1, 1, 1, 0, 0},
        {0, 1, 1, 1, 1, 0},
        {0, 0, 1, 1, 0, 0},
        {0, 0, 0, 1, 0, 0},
    };
    const std::vector<int> expectedFirst{0, 0, 0, 0, 1, -1};
    const std::vector<int> expectedLast{0, 1, 2, 3, 1, -1};
    return ExpectTrue(result.masks == expected, "half-open slab masks")
        && ExpectTrue(
            result.firstOccupiedLayers == expectedFirst,
            "half-open first occupied layers")
        && ExpectTrue(
            result.lastOccupiedLayers == expectedLast,
            "half-open last occupied layers");
}

bool DescendingLayerSlabWedgeIsSymmetric()
{
    const std::vector<GeometryOccupancyColumn> columns{
        {true, 0.0, 0.875},
        {true, 0.0, 0.625},
        {true, 0.0, 0.375},
        {true, 0.0, 0.125},
    };
    LayerOccupancyRequest request;
    request.columns = columns;
    request.layerCount = 4;
    request.layerThicknessMm = 0.25;
    request.policy = MakeLayerSlabGeometryOccupancyPolicy();

    const LayerOccupancyResult result = BuildLayerOccupancy(request);
    std::vector<int> occupiedCounts;
    for (const auto& mask : result.masks)
    {
        int count{0};
        for (const std::uint8_t value : mask)
        {
            count += value != 0 ? 1 : 0;
        }
        occupiedCounts.push_back(count);
    }
    return ExpectTrue(
        occupiedCounts == std::vector<int>({4, 3, 2, 1}),
        "descending wedge remains layer-symmetric");
}

bool SupersampleCandidatesApplyPerLayerThresholds()
{
    const std::vector<GeometryOccupancyColumn> columns{
        {true, 0.0, 0.4},
        {true, 0.0, 0.4},
    };
    const std::vector<GeometryOccupancyColumn> samples{
        {true, 0.0, 0.1},
        {true, 0.0, 0.2},
        {true, 0.1, 0.3},
        {false, 0.0, 0.0},
        {true, 0.2, 0.3},
        {false, 0.0, 0.0},
        {false, 0.0, 0.0},
        {false, 0.0, 0.0},
    };
    LayerOccupancyRequest request;
    request.columns = columns;
    request.coverageSubsampleColumns = samples;
    request.layerCount = 4;
    request.layerThicknessMm = 0.1;
    request.policy = MakeLayerSlabSupersample2x2GeometryOccupancyPolicy(2U);

    const LayerOccupancyResult atLeastTwoResult = BuildLayerOccupancy(request);
    const std::vector<std::vector<std::uint8_t>> expectedAtLeastTwo{
        {1, 0},
        {1, 0},
        {0, 0},
        {0, 0},
    };
    bool passed = ExpectTrue(
        atLeastTwoResult.masks == expectedAtLeastTwo,
        "2/4 coverage threshold is applied independently per layer");

    request.policy = MakeLayerSlabSupersample2x2GeometryOccupancyPolicy(1U);
    const LayerOccupancyResult anyHitResult = BuildLayerOccupancy(request);
    const std::vector<std::vector<std::uint8_t>> expectedAnyHit{
        {1, 0},
        {1, 0},
        {1, 1},
        {1, 1},
    };
    return ExpectTrue(
               anyHitResult.masks == expectedAnyHit,
               "1/4 coverage preserves single-sample thin features")
        && passed;
}

bool UnsupportedCandidatesFailClosed()
{
    const std::vector<GeometryOccupancyColumn> columns{{true, 0.0, 0.2}};
    LayerOccupancyRequest request;
    request.columns = columns;
    request.layerCount = 2;
    request.layerThicknessMm = 0.1;

    request.policy = MakeLayerSlabGeometryOccupancyPolicy();
    request.inputKind = GeometryOccupancyInputKind::GeneralMesh;
    bool passed = ExpectThrowsInvalidArgument(
        [&request]() { (void)BuildLayerOccupancy(request); },
        "Layer Slab must reject non-heightfield geometry");

    request.policy = MakeLegacyGeometryOccupancyPolicy();
    request.inputKind = GeometryOccupancyInputKind::SingleIntervalHeightfield;
    request.policy.xyMode = XyCoverageMode::Supersample2x2;
    passed = ExpectThrowsInvalidArgument(
                 [&request]() { (void)BuildLayerOccupancy(request); },
                 "2x2 must require Layer Slab coverage")
        && passed;

    request.policy = MakeLayerSlabSupersample2x2GeometryOccupancyPolicy(3U);
    passed = ExpectThrowsInvalidArgument(
                 [&request]() { (void)BuildLayerOccupancy(request); },
                 "unapproved 3/4 threshold must fail closed")
        && passed;

    request.policy = MakeLayerSlabSupersample2x2GeometryOccupancyPolicy(1U);
    passed = ExpectThrowsInvalidArgument(
                 [&request]() { (void)BuildLayerOccupancy(request); },
                 "2x2 must require four coverage samples per output column")
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

std::filesystem::path WriteConfig(
    const std::string& name,
    const std::string& slicingMode,
    const std::string& geometrySamplingObject)
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "slicesoft_stage16_layer_occupancy";
    std::filesystem::create_directories(directory);
    const std::filesystem::path path = directory / name;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output
        << "{\n"
        << "  \"slicingMode\": \"" << slicingMode << "\",\n"
        << "  \"input\": {\"modelPath\": \"fixture.obj\"}";
    if (!geometrySamplingObject.empty())
    {
        output << ",\n  \"geometrySampling\": " << geometrySamplingObject;
    }
    output << "\n}\n";
    return path;
}

bool CandidateConfigurationIsExplicitAndHeightfieldOnly()
{
    const SliceConfig defaultConfig = slicer_core::load_slice_config(
        WriteConfig("default.json", "relief_heightfield", ""));
    bool passed = ExpectTrue(
        defaultConfig.geometry_sampling.strategy == "legacy_center_sample",
        "missing geometrySampling keeps Legacy default");

    const SliceConfig candidateConfig = slicer_core::load_slice_config(
        WriteConfig(
            "candidate.json",
            "relief_heightfield",
            "{\"strategy\": \"layer_slab_pixel_center_candidate\"}"));
    passed = ExpectTrue(
                 candidateConfig.geometry_sampling.strategy
                     == "layer_slab_pixel_center_candidate",
                 "heightfield candidate is explicit")
        && passed;

    passed = ExpectThrowsRuntimeError(
                 []()
                 {
                     (void)slicer_core::load_slice_config(
                         WriteConfig(
                             "non_heightfield.json",
                             "closed_mesh_scanline",
                             "{\"strategy\": \"layer_slab_pixel_center_candidate\"}"));
                 },
                 "non-heightfield candidate config fails closed")
        && passed;

    for (const std::string strategy : {
             std::string("layer_slab_supersample_2x2_at_least_two_candidate"),
             std::string("layer_slab_supersample_2x2_any_hit_candidate")})
    {
        const SliceConfig supersampleConfig = slicer_core::load_slice_config(
            WriteConfig(
                strategy + ".json",
                "relief_heightfield",
                "{\"strategy\": \"" + strategy + "\"}"));
        passed = ExpectTrue(
                     supersampleConfig.geometry_sampling.strategy == strategy,
                     "heightfield 2x2 candidate is explicit")
            && passed;
        passed = ExpectThrowsRuntimeError(
                     [strategy]()
                     {
                         (void)slicer_core::load_slice_config(
                             WriteConfig(
                                 "non_heightfield_" + strategy + ".json",
                                 "closed_mesh_scanline",
                                 "{\"strategy\": \"" + strategy + "\"}"));
                     },
                     "non-heightfield 2x2 candidate config fails closed")
            && passed;
    }
    return passed;
}

}  // namespace

int main()
{
    try
    {
        const bool passed = DefaultPolicyIsLegacyOnly()
            && LegacyProviderMatchesCenterSampleRanges()
            && LayerSlabProviderMatchesHalfOpenRanges()
            && DescendingLayerSlabWedgeIsSymmetric()
            && SupersampleCandidatesApplyPerLayerThresholds()
            && UnsupportedCandidatesFailClosed()
            && InvalidInputsFailClosed()
            && CandidateConfigurationIsExplicitAndHeightfieldOnly();
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
