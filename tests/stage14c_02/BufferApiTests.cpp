#include "slicer_module/BufferApi.h"

#include "contracts/print_module_spi.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{

void Require(const bool condition, const char* const message)
{
    if (!condition)
    {
        std::cerr << "Stage 14C-02: " << message << '\n';
        std::exit(1);
    }
}

void ProbeReportsRequiredSize()
{
    int required{-1};
    const int result = slicesoft::module::WriteOut(
        "probe",
        nullptr,
        0,
        &required);

    Require(result == PM_ERR_BUFFER_SMALL, "C-SPI-05a must report buffer small");
    Require(required == 5, "C-SPI-05a must report bytes excluding NUL");
}

void DifferenceOfOneDoesNotWrite()
{
    constexpr std::array<char, 5> sentinel{'x', 'x', 'x', 'x', 'x'};
    std::array<char, sentinel.size()> output = sentinel;
    int required{-1};
    const int result = slicesoft::module::WriteOut(
        "probe",
        output.data(),
        static_cast<int>(output.size()),
        &required);

    Require(result == PM_ERR_BUFFER_SMALL, "C-SPI-05b must report buffer small");
    Require(required == 5, "C-SPI-05b must preserve the required size");
    Require(output == sentinel, "C-SPI-05b must not partially write");
}

void SufficientBufferWritesContentAndNul()
{
    std::array<char, 7> output{'x', 'x', 'x', 'x', 'x', 'x', 'x'};
    int required{-1};
    const int result = slicesoft::module::WriteOut(
        "probe",
        output.data(),
        6,
        &required);

    Require(result == 5, "C-SPI-05c must return bytes excluding NUL");
    Require(required == 5, "C-SPI-05c must report bytes excluding NUL");
    Require(std::string_view{output.data()} == "probe", "C-SPI-05c content mismatch");
    Require(output.at(5U) == '\0', "C-SPI-05c must append NUL");
    Require(output.at(6U) == 'x', "C-SPI-05c must not write past required+1");
}

void ProbeFormsAndOptionalRequiredAreSupported()
{
    std::array<char, 4> output{'q', 'q', 'q', 'q'};
    const auto sentinel = output;
    int required{-1};

    Require(
        slicesoft::module::WriteOut("abc", nullptr, 4, &required)
            == PM_ERR_BUFFER_SMALL,
        "null output with positive capacity must remain a probe");
    Require(required == 3, "null-output probe must report required size");
    Require(
        slicesoft::module::WriteOut("abc", output.data(), 0, &required)
            == PM_ERR_BUFFER_SMALL,
        "zero capacity with non-null output must remain a probe");
    Require(output == sentinel, "zero-capacity probe must not write");
    Require(
        slicesoft::module::WriteOut("abc", nullptr, 0, nullptr)
            == PM_ERR_BUFFER_SMALL,
        "outRequired must be optional during probe");
    Require(
        slicesoft::module::WriteOut("abc", output.data(), 4, nullptr) == 3,
        "outRequired must be optional during write");
}

void InvalidCapacityDoesNotMutateOutputs()
{
    std::array<char, 4> output{'z', 'z', 'z', 'z'};
    const auto sentinel = output;
    int required{73};

    Require(
        slicesoft::module::WriteOut("abc", output.data(), -1, &required)
            == PM_ERR_INVALID_ARG,
        "negative capacity with output must be invalid");
    Require(output == sentinel, "invalid capacity must not write output");
    Require(required == 73, "invalid capacity must not report a misleading size");
    Require(
        slicesoft::module::WriteOut("abc", nullptr, -1, nullptr)
            == PM_ERR_INVALID_ARG,
        "negative capacity probe must be invalid");
}

void SmallerBuffersNeverPartiallyWrite()
{
    for (int capacity = 1; capacity <= 5; ++capacity)
    {
        std::array<char, 6> output{'s', 's', 's', 's', 's', 's'};
        const auto sentinel = output;
        const int result = slicesoft::module::WriteOut(
            "probe",
            output.data(),
            capacity,
            nullptr);
        Require(result == PM_ERR_BUFFER_SMALL, "small capacity must fail");
        Require(output == sentinel, "small capacity must not partially write");
    }
}

void EmptyContentStillRequiresNulCapacity()
{
    int required{-1};
    char output{'x'};

    Require(
        slicesoft::module::WriteOut("", nullptr, 0, &required)
            == PM_ERR_BUFFER_SMALL,
        "empty content probe must follow the probe state");
    Require(required == 0, "empty content requires zero content bytes");
    Require(
        slicesoft::module::WriteOut("", &output, 1, &required) == 0,
        "empty content must succeed with one NUL byte of capacity");
    Require(output == '\0', "empty content must write the trailing NUL");
}

}  // namespace

int main()
{
    ProbeReportsRequiredSize();
    DifferenceOfOneDoesNotWrite();
    SufficientBufferWritesContentAndNul();
    ProbeFormsAndOptionalRequiredAreSupported();
    InvalidCapacityDoesNotMutateOutputs();
    SmallerBuffersNeverPartiallyWrite();
    EmptyContentStillRequiresNulCapacity();
    std::cout << "Stage 14C-02 BufferApi tests: PASS\n";
    return 0;
}
