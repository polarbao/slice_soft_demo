#include "slicer_module/ModuleInfo.h"
#include "contracts/print_module_spi.h"
#include "SliceSoftBuildVersion.h"

#include <iostream>
#include <string_view>
#include <vector>

namespace
{

bool Expect(const bool condition, const std::string_view message)
{
    if (!condition)
    {
        std::cerr << "Stage 14C-05 module info: FAIL: " << message << '\n';
    }
    return condition;
}

bool Contains(
    const std::string_view value,
    const std::string_view expected) noexcept
{
    return value.find(expected) != std::string_view::npos;
}

bool TestSpiModuleInfo(const std::string_view expected)
{
    int required{-1};
    bool passed = Expect(
        pm_module_info(nullptr, 0, &required) == PM_ERR_BUFFER_SMALL,
        "pm_module_info probe must use the shared buffer protocol");
    passed = Expect(
                 required == static_cast<int>(expected.size()),
                 "pm_module_info probe returned the wrong required size")
        && passed;

    std::vector<char> shortBuffer(static_cast<std::size_t>(required), 'x');
    passed = Expect(
                 pm_module_info(shortBuffer.data(), required, nullptr)
                     == PM_ERR_BUFFER_SMALL,
                 "pm_module_info must reject a buffer that is one byte short")
        && passed;

    std::vector<char> output(static_cast<std::size_t>(required + 1), '\0');
    passed = Expect(
                 pm_module_info(output.data(), required + 1, nullptr)
                     == static_cast<int>(expected.size()),
                 "pm_module_info write returned the wrong byte count")
        && passed;
    passed = Expect(
                 std::string_view{output.data()} == expected,
                 "pm_module_info bytes differ from the frozen module information")
        && passed;
    return passed;
}

}  // namespace

int main(const int argc, char* argv[])
{
    const std::string_view first = slicesoft::module::GetModuleInfoJson();
    if (argc == 2 && std::string_view{argv[1]} == "--print-json")
    {
        std::cout << first;
        return 0;
    }

    const std::string_view second = slicesoft::module::GetModuleInfoJson();
    bool passed = true;
    passed = Expect(
                 first.data() == second.data() && first == second,
                 "repeated calls must return identical process-lifetime bytes")
        && passed;
    passed = Expect(
                 slicesoft::module::GetModuleId() == "slicer",
                 "module id drifted")
        && passed;
    passed = Expect(
                 slicesoft::module::GetModuleVersion()
                     == SLICESOFT_SLICER_IMPLEMENTATION_VERSION,
                 "module version drifted")
        && passed;

#if defined(_DEBUG)
    passed = Expect(
                 slicesoft::module::GetModuleRuntime() == "MSVC-x64-MDd",
                 "Debug runtime identity drifted")
        && passed;
    passed = Expect(
                 slicesoft::module::GetModuleBuildConfig() == "Debug",
                 "Debug build configuration drifted")
        && passed;
#else
    passed = Expect(
                 slicesoft::module::GetModuleRuntime() == "MSVC-x64-MD",
                 "Release runtime identity drifted")
        && passed;
    passed = Expect(
                 slicesoft::module::GetModuleBuildConfig() == "Release",
                 "Release build configuration drifted")
        && passed;
#endif

    passed = Expect(
                 Contains(first, R"json("schema":"slicesoft.module_info.1")json"),
                 "module info schema is missing")
        && passed;
    passed = Expect(
                 Contains(first, R"json("spi":1)json"),
                 "SPI version is missing")
        && passed;
    passed = Expect(
                 Contains(first, R"json("contract":"p0.rgbwsv.2")json"),
                 "production package contract is missing")
        && passed;
    passed = Expect(
                 Contains(first, R"json("contract":"p0.rgbwsvt.1")json"),
                 "transfer package contract is missing")
        && passed;
    passed = Expect(
                 Contains(first, R"json("slice.rgbwsv")json")
                     && Contains(first, R"json("slice.rgbwsvt")json"),
                 "legacy and transfer slice capabilities must both be advertised")
        && passed;
    passed = Expect(
                 Contains(first, R"json("maxConcurrentJobs":1)json")
                     && Contains(first, R"json("cancelLatencyMs":2000)json"),
                 "job limits are missing")
        && passed;
    passed = TestSpiModuleInfo(first) && passed;

    if (!passed)
    {
        return 1;
    }
    std::cout << "Stage 14C-05 module info: PASS\n";
    return 0;
}
