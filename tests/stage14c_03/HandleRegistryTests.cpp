#include "slicer_module/ErrorApi.h"
#include "slicer_module/HandleRegistry.h"

#include <atomic>
#include <barrier>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace
{

using slicesoft::module::GetThreadLastErrorJson;
using slicesoft::module::HandleRegistry;
using slicesoft::module::JobLifecycleState;
using slicesoft::module::SetThreadLastError;

void Require(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "Stage 14C-03: " << message << '\n';
        std::exit(1);
    }
}

void TestCreateDestroyAndStaleHandles()
{
    HandleRegistry registry;
    pm_module_t* staleModule{nullptr};
    for (int iteration = 0; iteration < 100; ++iteration)
    {
        pm_module_t* const module = registry.CreateModule();
        Require(module != nullptr, "module creation should succeed");
        Require(registry.FindModule(module) != nullptr, "live module should resolve");
        Require(registry.DestroyModule(module), "live module should be destroyed");
        Require(registry.FindModule(module) == nullptr, "destroyed module should be stale");
        staleModule = module;
    }
    Require(registry.ActiveModuleCount() == 0U, "all modules should be retired");
    Require(!registry.DestroyModule(staleModule), "stale module destroy should be rejected");
    Require(registry.DestroyModule(nullptr), "null module destroy should be a no-op");
    Require(
        registry.FindModule(reinterpret_cast<pm_module_t*>(1U)) == nullptr,
        "foreign module pointer should be rejected without dereference");
}

void TestModuleJobOwnershipAndRelease()
{
    HandleRegistry registry;
    pm_module_t* const firstModule = registry.CreateModule();
    pm_module_t* const secondModule = registry.CreateModule();
    pm_job_t* const job = registry.CreateJob(firstModule);

    Require(job != nullptr, "job creation should succeed for a live module");
    Require(registry.CreateJob(nullptr) == nullptr, "null owner should be rejected");
    Require(
        registry.FindJob(firstModule, job) != nullptr,
        "job should resolve under its owner");
    Require(
        registry.FindJob(secondModule, job) == nullptr,
        "cross-module job lookup should be rejected");
    Require(registry.ReleaseJob(job), "live job release should succeed");
    Require(registry.FindJob(job) == nullptr, "released job should be stale");
    Require(!registry.ReleaseJob(job), "stale job release should be rejected");
    Require(registry.ReleaseJob(nullptr), "null job release should be a no-op");
    Require(
        registry.FindJob(reinterpret_cast<pm_job_t*>(1U)) == nullptr,
        "foreign job pointer should be rejected without dereference");
    Require(registry.DestroyModule(firstModule), "first module cleanup");
    Require(registry.DestroyModule(secondModule), "second module cleanup");
}

void TestDestroyModuleCollectsJobs()
{
    HandleRegistry registry;
    pm_module_t* const module = registry.CreateModule();
    pm_job_t* const firstJob = registry.CreateJob(module);
    pm_job_t* const secondJob = registry.CreateJob(module);
    const auto firstState = registry.FindJob(firstJob);
    const auto secondState = registry.FindJob(secondJob);

    Require(
        registry.SetJobLifecycleState(firstJob, JobLifecycleState::Running),
        "queued job should enter running state");
    Require(registry.DestroyModule(module), "module destruction should succeed");
    Require(registry.ActiveJobCount() == 0U, "owned jobs should be collected");
    Require(registry.FindJob(firstJob) == nullptr, "first job handle should be stale");
    Require(registry.FindJob(secondJob) == nullptr, "second job handle should be stale");
    Require(!firstState->IsActive(), "held first state should be inactive");
    Require(!secondState->IsActive(), "held second state should be inactive");
    Require(
        firstState->LifecycleState() == JobLifecycleState::Cancelled,
        "running job should be cancelled during module destruction");
    Require(
        secondState->LifecycleState() == JobLifecycleState::Cancelled,
        "queued job should be cancelled during module destruction");
}

void TestMinimalStateAndIdempotentCancellation()
{
    HandleRegistry registry;
    pm_module_t* const module = registry.CreateModule();
    pm_job_t* const job = registry.CreateJob(module);
    Require(
        registry.SetJobLifecycleState(job, JobLifecycleState::Running),
        "job should support the running state used by pm_result validation");
    Require(registry.RequestCancel(job), "first cancel should be accepted");
    Require(registry.RequestCancel(job), "repeated cancel should be idempotent");
    const auto cancelling = registry.FindJob(job);
    Require(cancelling->IsCancellationRequested(), "cancel flag should be retained");
    Require(
        cancelling->LifecycleState() == JobLifecycleState::Cancelling,
        "active cancelled job should enter cancelling state");
    Require(
        registry.SetJobLifecycleState(job, JobLifecycleState::Cancelled),
        "cancelling job should enter cancelled state");
    Require(registry.RequestCancel(job), "terminal cancel should be a successful no-op");
    Require(registry.ReleaseJob(job), "cancelled job should release");
    Require(registry.DestroyModule(module), "module cleanup");
}

void TestConcurrentModuleJobRegistration()
{
    HandleRegistry registry;
    pm_module_t* const module = registry.CreateModule();
    constexpr int threadCount{8};
    constexpr int jobsPerThread{100};
    std::barrier startGate{threadCount};
    std::atomic_bool succeeded{true};
    std::thread threads[threadCount];

    for (std::thread& thread : threads)
    {
        thread = std::thread{
            [&]()
            {
                startGate.arrive_and_wait();
                for (int index = 0; index < jobsPerThread; ++index)
                {
                    pm_job_t* const job = registry.CreateJob(module);
                    if (job == nullptr || !registry.ReleaseJob(job))
                    {
                        succeeded.store(false, std::memory_order_release);
                        return;
                    }
                }
            }};
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }

    Require(succeeded.load(std::memory_order_acquire), "concurrent jobs should register safely");
    Require(registry.ActiveJobCount() == 0U, "concurrent jobs should all release");
    Require(registry.DestroyModule(module), "concurrent module cleanup");
}

void TestThreadLocalLastError()
{
    SetThreadLastError(
        "PM-SLICER-INPUT-0001",
        "main message",
        "main detail");
    const std::string mainError{GetThreadLastErrorJson()};
    std::barrier rendezvous{3};
    std::string firstError;
    std::string secondError;

    std::thread firstThread{
        [&]()
        {
            SetThreadLastError(
                "PM-SLICER-STATE-0004",
                "first \"message\"",
                "line one\nline two");
            rendezvous.arrive_and_wait();
            firstError = GetThreadLastErrorJson();
            rendezvous.arrive_and_wait();
        }};
    std::thread secondThread{
        [&]()
        {
            SetThreadLastError(
                "PM-SLICER-INTERNAL-0099",
                "second message",
                "slash\\detail");
            rendezvous.arrive_and_wait();
            secondError = GetThreadLastErrorJson();
            rendezvous.arrive_and_wait();
        }};

    rendezvous.arrive_and_wait();
    Require(
        GetThreadLastErrorJson() == mainError,
        "worker failures must not overwrite the caller thread");
    rendezvous.arrive_and_wait();
    firstThread.join();
    secondThread.join();

    Require(
        firstError
            == R"({"code":"PM-SLICER-STATE-0004","message":"first \"message\"","detail":"line one\nline two"})",
        "first thread should retain stable escaped JSON");
    Require(
        secondError
            == R"({"code":"PM-SLICER-INTERNAL-0099","message":"second message","detail":"slash\\detail"})",
        "second thread should retain its own stable JSON");

    SetThreadLastError(
        "PM-SLICER-INPUT-0002",
        "replacement",
        "next failure wins");
    const std::string replacement{GetThreadLastErrorJson()};
    Require(replacement != mainError, "the next failure should overwrite the prior one");
    const std::string simulatedSuccess{GetThreadLastErrorJson()};
    Require(
        simulatedSuccess == replacement,
        "a successful call path should not clear the last failure");

    const std::string oversizedDetail(9000U, 'x');
    SetThreadLastError(
        "PM-SLICER-INPUT-0001",
        "oversized",
        oversizedDetail);
    Require(
        GetThreadLastErrorJson()
            == R"({"code":"PM-SLICER-INTERNAL-0099","message":"error detail exceeds TLS capacity","detail":""})",
        "oversized errors should fail closed to stable JSON without throwing");
}

}  // namespace

int main()
{
    TestCreateDestroyAndStaleHandles();
    TestModuleJobOwnershipAndRelease();
    TestDestroyModuleCollectsJobs();
    TestMinimalStateAndIdempotentCancellation();
    TestConcurrentModuleJobRegistration();
    TestThreadLocalLastError();
    std::cout << "Stage 14C-03 HandleRegistry/ErrorApi tests: PASS\n";
    return 0;
}
