#include "contracts/print_module_spi.h"
#include "slicer_module/ModuleInitialization.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

namespace
{

constexpr std::size_t ConcurrentCreateCount{32U};
std::atomic_size_t g_successActionCount{0U};
std::atomic_size_t g_failureActionCount{0U};

void Require(const bool condition, const std::string_view message)
{
    if (!condition)
    {
        std::cerr << "Stage 14C-07 DLL initialization: FAIL: " << message << '\n';
        std::exit(1);
    }
}

bool SuccessfulInitialization() noexcept
{
    g_successActionCount.fetch_add(1U, std::memory_order_relaxed);
    return true;
}

bool FailedInitialization() noexcept
{
    g_failureActionCount.fetch_add(1U, std::memory_order_relaxed);
    return false;
}

void TestReadOnlyAbiBeforeCreate()
{
    Require(pm_spi_version() == PM_SPI_VERSION, "pm_spi_version failed before pm_create");

    int moduleInfoSize{-1};
    Require(
        pm_module_info(nullptr, 0, &moduleInfoSize) == PM_ERR_BUFFER_SMALL,
        "pm_module_info must remain available before pm_create");
    Require(moduleInfoSize > 0, "pm_module_info returned an empty description");

    int errorSize{-1};
    Require(
        pm_last_error(nullptr, 0, &errorSize) == PM_ERR_BUFFER_SMALL,
        "pm_last_error must remain available before pm_create");
    Require(errorSize > 0, "pm_last_error returned an empty JSON value");
}

void TestCallOnceSuccess()
{
    g_successActionCount.store(0U, std::memory_order_relaxed);
    slicesoft::module::ModuleInitialization initialization{
        SuccessfulInitialization};
    std::vector<std::thread> threads;
    threads.reserve(ConcurrentCreateCount);
    std::atomic_size_t successfulCalls{0U};

    for (std::size_t index = 0U; index < ConcurrentCreateCount; ++index)
    {
        threads.emplace_back([&initialization, &successfulCalls]()
        {
            if (initialization.EnsureInitialized())
            {
                successfulCalls.fetch_add(1U, std::memory_order_relaxed);
            }
        });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }

    Require(
        successfulCalls.load(std::memory_order_relaxed) == ConcurrentCreateCount,
        "concurrent initialization callers did not all observe success");
    Require(
        g_successActionCount.load(std::memory_order_relaxed) == 1U,
        "the successful initialization action ran more than once");
    Require(
        initialization.GetInvocationCount() == 1U,
        "the successful initialization boundary recorded the wrong count");
    Require(
        initialization.GetState()
            == slicesoft::module::ModuleInitializationState::Initialized,
        "the successful initialization boundary did not reach Initialized");
}

void TestCallOnceFailureIsStable()
{
    g_failureActionCount.store(0U, std::memory_order_relaxed);
    slicesoft::module::ModuleInitialization initialization{FailedInitialization};

    Require(!initialization.EnsureInitialized(), "failed initialization reported success");
    Require(!initialization.EnsureInitialized(), "failed initialization was retried as success");
    Require(
        g_failureActionCount.load(std::memory_order_relaxed) == 1U,
        "the failed initialization action ran more than once");
    Require(
        initialization.GetInvocationCount() == 1U,
        "the failed initialization boundary recorded the wrong count");
    Require(
        initialization.GetState()
            == slicesoft::module::ModuleInitializationState::Failed,
        "the failed initialization boundary did not remain Failed");
}

void TestConcurrentModuleCreate()
{
    std::vector<pm_module_t*> modules(ConcurrentCreateCount, nullptr);
    std::vector<std::thread> threads;
    threads.reserve(ConcurrentCreateCount);
    for (std::size_t index = 0U; index < ConcurrentCreateCount; ++index)
    {
        threads.emplace_back([&modules, index]()
        {
            modules[index] = pm_create(nullptr);
        });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }

    std::unordered_set<pm_module_t*> uniqueModules;
    for (pm_module_t* const module : modules)
    {
        Require(module != nullptr, "concurrent pm_create returned a null module");
        uniqueModules.insert(module);
    }
    Require(
        uniqueModules.size() == ConcurrentCreateCount,
        "concurrent pm_create did not return independent module instances");

    for (pm_module_t* const module : modules)
    {
        pm_destroy(module);
    }
}

}  // namespace

int main()
{
    TestReadOnlyAbiBeforeCreate();
    TestCallOnceSuccess();
    TestCallOnceFailureIsStable();
    TestConcurrentModuleCreate();
    std::cout << "Stage 14C-07 DLL initialization tests: PASS\n";
    return 0;
}
