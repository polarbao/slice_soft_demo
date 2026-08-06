#include "contracts/print_module_spi.h"

#include <Windows.h>
#include <Psapi.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{

void Require(const bool condition, const char* const message)
{
    if (!condition)
    {
        std::cerr << "Stage 14C-03 ABI: " << message << '\n';
        std::exit(1);
    }
}

std::string ReadLastError()
{
    int required{-1};
    Require(
        pm_last_error(nullptr, 0, &required) == PM_ERR_BUFFER_SMALL,
        "pm_last_error probe must use the shared buffer protocol");
    Require(required > 0, "pm_last_error must expose failure JSON");

    std::vector<char> output(static_cast<std::size_t>(required) + 1U, '\0');
    Require(
        pm_last_error(output.data(), static_cast<int>(output.size()), nullptr)
            == required,
        "pm_last_error write must return the JSON byte count");
    return {output.data()};
}

void TestModuleLifecycle()
{
    for (int iteration = 0; iteration < 10; ++iteration)
    {
        pm_module_t* const module = pm_create(nullptr);
        Require(module != nullptr, "pm_create warmup must return a live handle");
        pm_destroy(module);
    }

    PROCESS_MEMORY_COUNTERS_EX before{};
    before.cb = sizeof(before);
    Require(
        GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&before),
            sizeof(before)) != FALSE,
        "failed to read memory before the lifecycle loop");

    for (int iteration = 0; iteration < 100; ++iteration)
    {
        pm_module_t* const module = pm_create(nullptr);
        Require(module != nullptr, "pm_create must return a live handle");
        pm_destroy(module);
    }

    PROCESS_MEMORY_COUNTERS_EX after{};
    after.cb = sizeof(after);
    Require(
        GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&after),
            sizeof(after)) != FALSE,
        "failed to read memory after the lifecycle loop");
    const std::uint64_t growth = after.PrivateUsage > before.PrivateUsage
        ? static_cast<std::uint64_t>(after.PrivateUsage - before.PrivateUsage)
        : 0U;
    Require(
        growth < 1024U * 1024U,
        "C-SPI-04 private memory growth must stay below 1 MiB");

    pm_destroy(nullptr);
    pm_release(nullptr);
}

void TestInvalidHandlesAndThreadError()
{
    Require(
        pm_poll(nullptr, nullptr, 0, nullptr) == PM_ERR_INVALID_ARG,
        "pm_poll must reject a null job");
    const std::string firstError = ReadLastError();
    Require(
        firstError.find("PM-SLICER-INPUT-0002") != std::string::npos,
        "pm_poll failure must publish stable TLS JSON");

    Require(pm_cancel(nullptr) == PM_ERR_INVALID_ARG, "pm_cancel must reject a null job");
    Require(
        pm_result(nullptr, nullptr, 0, nullptr) == PM_ERR_INVALID_ARG,
        "pm_result must reject a null job");
    const std::string replacement = ReadLastError();
    Require(
        replacement.find("pm_result") != std::string::npos,
        "the next failure must replace the current thread error");

    Require(pm_spi_version() == PM_SPI_VERSION, "successful calls must remain available");
    Require(
        ReadLastError() == replacement,
        "a successful call must not clear the last failure");
}

void TestSubmitDoesNotFakeCapabilitySuccess()
{
    pm_module_t* const module = pm_create(nullptr);
    Require(module != nullptr, "module setup failed");
    Require(
        pm_submit(module, "{}") == nullptr,
        "unwired capability routing must fail closed");
    const std::string error = ReadLastError();
    Require(
        error.find("PM-SLICER-INPUT-0002") != std::string::npos
            && error.find("invalid capability carrier request")
                != std::string::npos,
        "invalid routing request must publish the stable input contract error");
    pm_destroy(module);
}

}  // namespace

int main()
{
    TestModuleLifecycle();
    TestInvalidHandlesAndThreadError();
    TestSubmitDoesNotFakeCapabilitySuccess();
    std::cout << "Stage 14C-03 module ABI tests: PASS\n";
    return 0;
}
