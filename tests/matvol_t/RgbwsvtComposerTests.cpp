#include "slicer_core/materials/transfer/TransferChannelError.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtProtocol.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

slicer_core::RgbwsvProductionLayer CraftFixture()
{
    slicer_core::RgbwsvProductionLayer layer;
    layer.layerIndex = 7;
    layer.zMm = 0.266;
    layer.widthPx = 4;
    layer.heightPx = 1;
    layer.channels = {
        10U, 20U, 30U, 255U, 255U, 255U,
        255U, 255U, 255U, 0U, 255U, 255U,
        70U, 80U, 90U, 255U, 255U, 0U,
        255U, 255U, 255U, 255U, 0U, 255U};
    return layer;
}

bool NoTransferPreservesAllSixChannels()
{
    const slicer_core::RgbwsvProductionLayer source = CraftFixture();
    const std::vector<std::uint8_t> modelMask(4U, 1U);
    const std::vector<std::uint8_t> transferMask(4U, 0U);
    const slicer_core::RgbwsvtProductionLayer result =
        slicer_core::ComposeRgbwsvtLayer(source, modelMask, transferMask, 0U);
    bool passed{true};
    for (std::size_t pixel{0U}; pixel < 4U; ++pixel)
    {
        for (std::size_t channel{0U}; channel < 6U; ++channel)
        {
            passed = ExpectTrue(
                         result.channels[pixel * 7U + channel]
                             == source.channels[pixel * 6U + channel],
                         "no-T conversion preserves RGBWSV bytes")
                && passed;
        }
        passed = ExpectTrue(
                     result.channels[pixel * 7U + 6U] == 255U,
                     "no-T conversion writes empty T")
            && passed;
    }
    return passed;
}

bool TransferPixelIsExclusive()
{
    const slicer_core::RgbwsvProductionLayer source = CraftFixture();
    const std::vector<std::uint8_t> modelMask(4U, 1U);
    const std::vector<std::uint8_t> transferMask{0U, 1U, 0U, 0U};
    const slicer_core::RgbwsvtProductionLayer result =
        slicer_core::ComposeRgbwsvtLayer(source, modelMask, transferMask, 0U);
    const std::size_t offset{7U};
    return ExpectTrue(
               std::all_of(
                   result.channels.begin() + static_cast<std::ptrdiff_t>(offset),
                   result.channels.begin() + static_cast<std::ptrdiff_t>(offset + 6U),
                   [](const std::uint8_t value) { return value == 255U; }),
               "transfer pixel clears RGBWSV")
        && ExpectTrue(result.channels[offset + 6U] == 0U, "transfer pixel writes black-is-print T");
}

bool PartialTransferValueFailsClosed()
{
    const slicer_core::RgbwsvProductionLayer source = CraftFixture();
    const std::vector<std::uint8_t> modelMask(4U, 1U);
    const std::vector<std::uint8_t> transferMask{0U, 1U, 0U, 0U};
    try
    {
        (void)slicer_core::ComposeRgbwsvtLayer(source, modelMask, transferMask, 17U);
    }
    catch (const slicer_core::TransferChannelError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::TransferChannelErrorCode::ConfigInvalid,
            "partial T value has a stable config error");
    }
    return ExpectTrue(false, "p0.rgbwsvt.1 must reject partial T values");
}

bool TransferOutsideModelFailsClosed()
{
    const slicer_core::RgbwsvProductionLayer source = CraftFixture();
    const std::vector<std::uint8_t> modelMask{1U, 0U, 1U, 1U};
    const std::vector<std::uint8_t> transferMask{0U, 1U, 0U, 0U};
    try
    {
        (void)slicer_core::ComposeRgbwsvtLayer(source, modelMask, transferMask, 0U);
    }
    catch (const slicer_core::TransferChannelError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::TransferChannelErrorCode::MaskOutsideModel,
            "out-of-model transfer has stable error code");
    }
    return ExpectTrue(false, "out-of-model transfer must fail closed");
}

bool ProtocolIsFrozen()
{
    const slicer_core::RgbwsvtProtocol protocol = slicer_core::CurrentRgbwsvtProtocol();
    return ExpectTrue(protocol.schema == "p0.rgbwsvt.1", "new schema is versioned")
        && ExpectTrue(
            protocol.channelOrder
                == std::array<std::string, 7>{"R", "G", "B", "W", "S", "V", "T"},
            "new channel order is RGBWSVT")
        && ExpectTrue(protocol.bitDepth == 8, "new protocol remains uint8")
        && ExpectTrue(protocol.polarity == "black_is_print", "polarity remains black_is_print");
}

}  // namespace

int main()
{
    int failures{0};
    const auto run = [&failures](const bool passed, const char* name)
    {
        if (!passed)
        {
            std::cerr << "CASE FAILED " << name << '\n';
            ++failures;
        }
    };
    run(NoTransferPreservesAllSixChannels(), "no_transfer_zero_drift");
    run(TransferPixelIsExclusive(), "transfer_exclusive");
    run(PartialTransferValueFailsClosed(), "partial_transfer_value_fail_closed");
    run(TransferOutsideModelFailsClosed(), "outside_model_fail_closed");
    run(ProtocolIsFrozen(), "protocol_frozen");
    if (failures != 0)
    {
        std::cerr << "FAIL RgbwsvtComposerTests " << failures << " case(s)\n";
        return 1;
    }
    std::cout << "PASS RgbwsvtComposerTests 5/5\n";
    return 0;
}
