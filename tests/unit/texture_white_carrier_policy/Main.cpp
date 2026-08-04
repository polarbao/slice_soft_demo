#include "slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

bool ExpectTrue(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

bool FailClosedPolicyNeverSelectsAWhiteCarrier()
{
    return ExpectTrue(
        !slicer_core::IsUnprintableWhiteTexel(
            "fail_closed",
            255U,
            std::array<std::uint8_t, 3>{255U, 255U, 255U}),
        "fail_closed must not classify any texel for white-carrier output");
}

bool ExactWhiteIsSelectedAtZeroThreshold()
{
    return ExpectTrue(
        slicer_core::IsUnprintableWhiteTexel(
            "white_underbase",
            0U,
            std::array<std::uint8_t, 3>{255U, 255U, 255U}),
        "pure white must be selected at threshold zero");
}

bool OffWhiteIsRejectedAtZeroThreshold()
{
    return ExpectTrue(
        !slicer_core::IsUnprintableWhiteTexel(
            "white_underbase",
            0U,
            std::array<std::uint8_t, 3>{254U, 255U, 255U}),
        "off-white must be rejected at threshold zero");
}

bool Gray254IsSelectedAtThresholdOne()
{
    return ExpectTrue(
        slicer_core::IsUnprintableWhiteTexel(
            "white_underbase",
            1U,
            std::array<std::uint8_t, 3>{254U, 254U, 254U}),
        "254 gray must be selected at threshold one");
}

bool ApplicationCanOnlyMutateTheWhiteChannel()
{
    std::array<std::uint8_t, 6> rgbwsv{
        255U,
        255U,
        255U,
        255U,
        255U,
        255U};
    const std::array<std::uint8_t, 6> before = rgbwsv;
    const bool applied = slicer_core::ApplyUnprintableWhiteCarrier(
        "white_underbase",
        0U,
        0U,
        std::array<std::uint8_t, 3>{rgbwsv[0], rgbwsv[1], rgbwsv[2]},
        rgbwsv[3]);
    return ExpectTrue(applied, "exact white must receive the white carrier")
        && ExpectTrue(rgbwsv[0] == before[0], "R must remain unchanged")
        && ExpectTrue(rgbwsv[1] == before[1], "G must remain unchanged")
        && ExpectTrue(rgbwsv[2] == before[2], "B must remain unchanged")
        && ExpectTrue(rgbwsv[3] == 0U, "W must receive the configured print value")
        && ExpectTrue(rgbwsv[4] == before[4], "S must remain unchanged")
        && ExpectTrue(rgbwsv[5] == before[5], "V must remain unchanged");
}

bool WriteF04DiffEvidence(const std::filesystem::path& outputPath)
{
    std::filesystem::create_directories(outputPath.parent_path());
    std::ofstream output{outputPath};
    if (!output)
    {
        return ExpectTrue(false, "failed to open F-04 pixel diff evidence");
    }
    output << "layerIndex,pixelIndex,channel,before,after,r,g,b,reason\n";
    for (std::size_t pixelIndex{0U}; pixelIndex < 4U; ++pixelIndex)
    {
        std::array<std::uint8_t, 6> rgbwsv{
            255U,
            255U,
            255U,
            255U,
            255U,
            255U};
        const std::uint8_t beforeWhite = rgbwsv[3];
        const bool applied = slicer_core::ApplyUnprintableWhiteCarrier(
            "white_underbase",
            0U,
            0U,
            std::array<std::uint8_t, 3>{rgbwsv[0], rgbwsv[1], rgbwsv[2]},
            rgbwsv[3]);
        if (!applied)
        {
            return ExpectTrue(false, "F-04 exact white evidence must apply");
        }
        output << "0," << pixelIndex << ",W,"
               << static_cast<int>(beforeWhite) << ','
               << static_cast<int>(rgbwsv[3])
               << ",255,255,255,unprintable_exact_white\n";
    }
    return true;
}

}  // namespace

int main(const int argc, const char* const argv[])
{
    if (!FailClosedPolicyNeverSelectsAWhiteCarrier()
        || !ExactWhiteIsSelectedAtZeroThreshold()
        || !OffWhiteIsRejectedAtZeroThreshold()
        || !Gray254IsSelectedAtThresholdOne()
        || !ApplicationCanOnlyMutateTheWhiteChannel())
    {
        return 1;
    }

    if (argc == 3 && std::string{argv[1]} == "--evidence-csv")
    {
        if (!WriteF04DiffEvidence(argv[2]))
        {
            return 1;
        }
    }
    else if (argc != 1)
    {
        std::cerr << "Usage: texture_white_carrier_policy_unit_tests "
                     "[--evidence-csv <path>]\n";
        return 2;
    }

    std::cout << "Texture white carrier policy unit tests complete.\n";
    return 0;
}
