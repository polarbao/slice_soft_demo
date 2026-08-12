#include "slicer_core/json_value.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace
{

using slicer_core::Json;

Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("cannot open " + path.string());
    }
    return Json::parse(input);
}

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

const Json& FindFixture(const Json& document, const std::string& id)
{
    for (const Json& fixture : document.at("fixtures").as_array())
    {
        if (fixture.at("id").as_string() == id)
        {
            return fixture;
        }
    }
    throw std::runtime_error("fixture not found: " + id);
}

std::vector<int> CountOccupiedPixelsByLayer(const Json& fixture)
{
    const double layerThickness = fixture.at("layerThicknessMm").as_double();
    double maximumZ = 0.0;
    for (const Json& column : fixture.at("columns").as_array())
    {
        for (const Json& interval : column.at("intervalsMm").as_array())
        {
            maximumZ = std::max(maximumZ, interval.at(1).as_double());
        }
    }

    const int layerCount = static_cast<int>(std::ceil(maximumZ / layerThickness));
    std::vector<int> counts(static_cast<std::size_t>(layerCount), 0);
    for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex)
    {
        const double slabLow = static_cast<double>(layerIndex) * layerThickness;
        const double slabHigh = slabLow + layerThickness;
        for (const Json& column : fixture.at("columns").as_array())
        {
            bool occupied = false;
            for (const Json& interval : column.at("intervalsMm").as_array())
            {
                const double intervalLow = interval.at(0).as_double();
                const double intervalHigh = interval.at(1).as_double();
                occupied = occupied || (intervalHigh > slabLow && intervalLow < slabHigh);
            }
            counts[static_cast<std::size_t>(layerIndex)] += occupied ? 1 : 0;
        }
    }
    return counts;
}

std::vector<int> ReadIntegerArray(const Json& value)
{
    std::vector<int> result;
    for (const Json& item : value.as_array())
    {
        result.push_back(item.as_int());
    }
    return result;
}

bool FixtureExpectationsAreHandCheckable(const Json& fixtures)
{
    bool passed = true;
    const std::vector<std::string> heightfieldIds{
        "flat_bottom_block",
        "ascending_wedge",
        "descending_wedge",
    };
    for (const std::string& id : heightfieldIds)
    {
        const Json& fixture = FindFixture(fixtures, id);
        const std::vector<int> actual = CountOccupiedPixelsByLayer(fixture);
        if (fixture.contains("expectedOccupiedPixelCountByLayer"))
        {
            passed = ExpectTrue(
                         actual == ReadIntegerArray(fixture.at("expectedOccupiedPixelCountByLayer")),
                         id + " occupied pixel counts")
                && passed;
        }
        else
        {
            std::vector<int> actualLayers;
            for (std::size_t layerIndex = 0; layerIndex < actual.size(); ++layerIndex)
            {
                if (actual[layerIndex] > 0)
                {
                    actualLayers.push_back(static_cast<int>(layerIndex));
                }
            }
            passed = ExpectTrue(
                         actualLayers == ReadIntegerArray(fixture.at("expectedOccupiedLayers")),
                         id + " occupied layers")
                && passed;
        }
    }

    for (const std::string& id : {"circular_contact_edge", "subpixel_thin_sheet"})
    {
        const Json& fixture = FindFixture(fixtures, id);
        const std::vector<int> coverage = ReadIntegerArray(fixture.at("coveredSubsamplesByPixel"));
        std::vector<int> atLeastOne;
        std::vector<int> atLeastTwo;
        for (const int covered : coverage)
        {
            atLeastOne.push_back(covered >= 1 ? 1 : 0);
            atLeastTwo.push_back(covered >= 2 ? 1 : 0);
        }
        passed = ExpectTrue(
                     atLeastOne == ReadIntegerArray(fixture.at("expectedMaskAtLeastOneOfFour")),
                     id + " >=1/4 mask")
            && passed;
        passed = ExpectTrue(
                     atLeastTwo == ReadIntegerArray(fixture.at("expectedMaskAtLeastTwoOfFour")),
                     id + " >=2/4 mask")
            && passed;
    }

    const Json& negative = FindFixture(fixtures, "multi_interval_column_negative");
    passed = ExpectTrue(negative.at("expectedAdmission").as_string() == "rejected", "negative admission") && passed;
    passed = ExpectTrue(
                 negative.at("expectedErrorCode").as_string()
                     == "E_GEOMETRY_SAMPLING_MULTI_INTERVAL_UNSUPPORTED",
                 "negative error code")
        && passed;
    return passed;
}

bool DiffContractIsStable(const Json& contract, const Json& example)
{
    bool passed = true;
    passed = ExpectTrue(
                 contract.at("schema").as_string()
                     == "slicesoft.stage16.layer_channel_diff_contract.1",
                 "contract schema")
        && passed;
    passed = ExpectTrue(
                 example.at("schema").as_string() == contract.at("reportSchema").as_string(),
                 "example report schema")
        && passed;

    for (const Json& field : contract.at("requiredTopLevelFields").as_array())
    {
        passed = ExpectTrue(example.contains(field.as_string()), "top-level field " + field.as_string()) && passed;
    }

    const Json& layer = example.at("layers").at(0);
    for (const Json& field : contract.at("requiredLayerFields").as_array())
    {
        passed = ExpectTrue(layer.contains(field.as_string()), "layer field " + field.as_string()) && passed;
    }

    std::vector<std::string> channels;
    for (const Json& channel : layer.at("channels").as_array())
    {
        for (const Json& field : contract.at("requiredChannelFields").as_array())
        {
            passed = ExpectTrue(channel.contains(field.as_string()), "channel field " + field.as_string()) && passed;
        }
        channels.push_back(channel.at("channel").as_string());
    }
    std::vector<std::string> expectedChannels;
    for (const Json& channel : contract.at("channelOrder").as_array())
    {
        expectedChannels.push_back(channel.as_string());
    }
    passed = ExpectTrue(channels == expectedChannels, "channel order remains R G B W S V") && passed;

    const Json& summary = example.at("summary");
    for (const Json& field : contract.at("requiredSummaryFields").as_array())
    {
        passed = ExpectTrue(summary.contains(field.as_string()), "summary field " + field.as_string()) && passed;
    }
    return passed;
}

}  // namespace

int main()
{
    try
    {
        const std::filesystem::path sourceDir = SLICESOFT_SOURCE_DIR;
        const Json fixtures = ReadJson(sourceDir / "tests/stage16/fixtures/geometry_sampling_fixtures.json");
        const Json contract = ReadJson(sourceDir / "tests/stage16/contracts/layer_channel_diff_schema.json");
        const Json example = ReadJson(sourceDir / "tests/stage16/fixtures/layer_channel_diff_example.json");

        bool passed = true;
        passed = ExpectTrue(
                     fixtures.at("schema").as_string()
                         == "slicesoft.stage16.geometry_sampling_fixture.1",
                     "fixture schema")
            && passed;
        passed = ExpectTrue(fixtures.at("intervalConvention").as_string() == "half_open", "half-open interval")
            && passed;
        passed = ExpectTrue(fixtures.at("fixtures").size() == 6U, "six synthetic fixtures") && passed;
        passed = FixtureExpectationsAreHandCheckable(fixtures) && passed;
        passed = DiffContractIsStable(contract, example) && passed;

        if (!passed)
        {
            return 1;
        }
        std::cout << "Stage 16 geometry sampling fixture tests complete.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL exception=" << error.what() << '\n';
        return 1;
    }
}
