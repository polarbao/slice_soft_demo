#include "contracts/slicer_logging.h"
#include "diagnostics/LogEvent.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

struct Api
{
    HMODULE library;
    template<class Function> Function Load(const char* name)
    {
        const auto function = GetProcAddress(library, name);
        if (function == nullptr) throw std::runtime_error(std::string{"missing export: "} + name);
        return reinterpret_cast<Function>(function);
    }
    explicit Api(const std::filesystem::path& path) : library{LoadLibraryW(path.c_str())}
    {
        Require(library != nullptr, "could not load slicer_module.dll");
    }
    ~Api() { FreeLibrary(library); }
    Api(const Api&) = delete;
    Api& operator=(const Api&) = delete;
};

struct Instance
{
    pm_module_t* module;
    decltype(&pm_destroy) destroy;
    explicit Instance(Api& api) : module{api.Load<decltype(&pm_create)>("pm_create")(nullptr)},
        destroy{api.Load<decltype(&pm_destroy)>("pm_destroy")}
    { Require(module != nullptr, "pm_create failed"); }
    ~Instance() { destroy(module); }
};

struct Receiver
{
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::string> events;
    std::atomic_bool invalid{false};
    bool blocked{false};
    bool released{false};
};

void PM_CALL Receive(void* context, int level, const char* json, int bytes)
{
    auto& receiver = *static_cast<Receiver*>(context);
    const std::string event{json, static_cast<std::size_t>(bytes)};
    if (bytes <= 0 || bytes > SLICER_LOG_MAX_EVENT_BYTES
        || !slicesoft::diagnostics::ValidEvent(event, level)) receiver.invalid = true;
    std::unique_lock lock{receiver.mutex};
    receiver.events.push_back(event);
    receiver.changed.notify_all();
    if (receiver.blocked) receiver.changed.wait(lock, [&] { return receiver.released; });
}

bool WaitCount(Receiver& receiver, std::size_t count)
{
    std::unique_lock lock{receiver.mutex};
    return receiver.changed.wait_for(lock, std::chrono::seconds{5}, [&] { return receiver.events.size() >= count; });
}

std::string LastError(Api& api)
{
    const auto last = api.Load<decltype(&pm_last_error)>("pm_last_error");
    int required{0};
    Require(last(nullptr, 0, &required) == PM_ERR_BUFFER_SMALL, "last_error probe failed");
    std::string json(static_cast<std::size_t>(required) + 1, '\0');
    Require(last(json.data(), static_cast<int>(json.size()), nullptr) >= 0, "last_error read failed");
    json.resize(static_cast<std::size_t>(required));
    return json;
}

void TestRealEventsAndQueries(Api& api)
{
    const auto set = api.Load<decltype(&slicer_set_log_callback_v1)>("slicer_set_log_callback_v1");
    const auto clear = api.Load<decltype(&slicer_clear_log_callback_v1)>("slicer_clear_log_callback_v1");
    const auto submit = api.Load<decltype(&pm_submit)>("pm_submit");
    Receiver receiver;
    Receiver other;
    Instance one{api};
    Instance two{api};
    Require(api.Load<decltype(&slicer_log_api_version)>("slicer_log_api_version")() == 1, "extension version mismatch");
    Require(api.Load<decltype(&pm_spi_version)>("pm_spi_version")() == PM_SPI_VERSION, "SPI version drift");
    Require(set(nullptr, Receive, &receiver, 0) == SLICER_LOG_INVALID_ARGUMENT, "invalid module accepted");
    Require(set(one.module, Receive, &receiver, 0) == SLICER_LOG_OK, "set failed");
    Require(set(two.module, Receive, &other, 0) == SLICER_LOG_OK, "second set failed");
    int required{0};
    Require(api.Load<decltype(&pm_module_info)>("pm_module_info")(nullptr, 0, &required) == PM_ERR_BUFFER_SMALL, "module_info failed");
    Require(api.Load<decltype(&pm_self_test)>("pm_self_test")(one.module, nullptr, 0, &required) == PM_ERR_BUFFER_SMALL, "self_test failed");
    (void)LastError(api);
    Require(submit(one.module, nullptr) == nullptr, "null request accepted");
    const auto originalError = LastError(api);
    Require(set(one.module, Receive, &receiver, 0) == SLICER_LOG_INVALID_STATE, "replacement accepted");
    Require(clear(one.module, -1) == SLICER_LOG_INVALID_ARGUMENT, "negative public timeout accepted");
    Require(LastError(api) == originalError, "logging extension overwrote business last_error");
    Require(submit(two.module, "{bad json") == nullptr, "malformed request accepted");
    const bool observed = WaitCount(receiver, 1) && WaitCount(other, 1);
    Require(clear(one.module, 5000) == SLICER_LOG_OK, "first clear failed");
    Require(clear(two.module, 5000) == SLICER_LOG_OK, "second clear failed");
    Require(observed && !receiver.invalid && !other.invalid, "real DLL events missing or invalid");
    Require(receiver.events.size() == 1 && other.events.size() == 1, "queries emitted events or instances crossed");
    Require(receiver.events.front().find("request_json is required") != std::string::npos,
        "DLL internal validation event missing");
    Require(receiver.events.front().find("slicer.module") != std::string::npos, "module domain missing");
    Require(clear(one.module, 0) == SLICER_LOG_OK, "clear is not idempotent");
}

struct ReentryContext
{
    pm_module_t* module;
    decltype(&slicer_clear_log_callback_v1) clear;
    decltype(&pm_destroy) destroy;
    decltype(&pm_submit) submit;
    std::mutex mutex;
    std::condition_variable changed;
    bool completed{false};
    int result{99};
};

void PM_CALL Reenter(void* context, int, const char*, int)
{
    auto& value = *static_cast<ReentryContext*>(context);
    const auto result = value.clear(value.module, 1);
    value.destroy(value.module);
    const auto job = value.submit(value.module, "{}");
    std::lock_guard lock{value.mutex};
    value.result = job == nullptr ? result : 99;
    value.completed = true;
    value.changed.notify_all();
}

void TestPublicReentry(Api& api)
{
    ReentryContext context{};
    Instance instance{api};
    context.module = instance.module;
    context.clear = api.Load<decltype(&slicer_clear_log_callback_v1)>("slicer_clear_log_callback_v1");
    context.destroy = api.Load<decltype(&pm_destroy)>("pm_destroy");
    context.submit = api.Load<decltype(&pm_submit)>("pm_submit");
    const auto set = api.Load<decltype(&slicer_set_log_callback_v1)>("slicer_set_log_callback_v1");
    Require(set(instance.module, Reenter, &context, 0) == SLICER_LOG_OK, "reentry set failed");
    (void)context.submit(instance.module, nullptr);
    bool completed{false};
    {
        std::unique_lock lock{context.mutex};
        completed = context.changed.wait_for(lock, std::chrono::seconds{5}, [&] { return context.completed; });
    }
    Require(context.clear(instance.module, 5000) == SLICER_LOG_OK, "reentry clear failed");
    Require(completed && context.result == SLICER_LOG_INVALID_STATE, "reentrant clear not rejected");
    int required{0};
    Require(api.Load<decltype(&pm_self_test)>("pm_self_test")(instance.module, nullptr, 0, &required)
        == PM_ERR_BUFFER_SMALL, "callback destroyed its own instance");
}

void TestDestroyWaits(Api& api)
{
    const auto create = api.Load<decltype(&pm_create)>("pm_create");
    const auto destroy = api.Load<decltype(&pm_destroy)>("pm_destroy");
    const auto set = api.Load<decltype(&slicer_set_log_callback_v1)>("slicer_set_log_callback_v1");
    const auto clear = api.Load<decltype(&slicer_clear_log_callback_v1)>("slicer_clear_log_callback_v1");
    Receiver receiver;
    receiver.blocked = true;
    const auto module = create(nullptr);
    Require(module != nullptr, "create failed");
    if (set(module, Receive, &receiver, 0) != SLICER_LOG_OK)
    { destroy(module); throw std::runtime_error("blocking binding failed"); }
    (void)api.Load<decltype(&pm_submit)>("pm_submit")(module, nullptr);
    const bool entered = WaitCount(receiver, 1);
    const auto timed = clear(module, 5);
    std::atomic_bool destroyed{false};
    std::thread teardown{[&] { destroy(module); destroyed = true; receiver.changed.notify_all(); }};
    bool prematurelyDestroyed{false};
    {
        std::unique_lock lock{receiver.mutex};
        prematurelyDestroyed = receiver.changed.wait_for(lock, std::chrono::milliseconds{40},
            [&] { return destroyed.load(); });
        receiver.released = true;
        receiver.changed.notify_all();
    }
    teardown.join();
    Require(entered && timed == SLICER_LOG_TIMEOUT && !prematurelyDestroyed,
        "active callback did not hold teardown barrier");
    Require(destroyed && clear(module, 0) == SLICER_LOG_INVALID_ARGUMENT,
        "module handle survived completed teardown");
}

void TestRealWorkerTelemetry(Api& api)
{
    Receiver receiver;
    Instance instance{api};
    const auto set = api.Load<decltype(&slicer_set_log_callback_v1)>("slicer_set_log_callback_v1");
    const auto clear = api.Load<decltype(&slicer_clear_log_callback_v1)>("slicer_clear_log_callback_v1");
    const auto result = api.Load<decltype(&pm_result)>("pm_result");
    Require(set(instance.module, Receive, &receiver, 0) == SLICER_LOG_OK, "Worker log registration failed");
    // The outer request routes to the real Worker; its invalid scene is rejected there.
    const char* request = R"({"capability":"geometry.preflight","mode":"full","jobId":"logdump-worker-test","scene":{},"sceneHash":"invalid","expectedSceneRevision":1,"profile":{},"profileHash":"invalid","targetMode":"legacy"})";
    const auto job = api.Load<decltype(&pm_submit)>("pm_submit")(instance.module, request);
    Require(job != nullptr, "real Worker request was not accepted by module");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{15};
    int required{0};
    int status{PM_ERR_INVALID_STATE};
    while (std::chrono::steady_clock::now() < deadline)
    {
        status = result(job, nullptr, 0, &required);
        if (status != PM_ERR_INVALID_STATE) break;
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    api.Load<decltype(&pm_release)>("pm_release")(job);
    bool workerEvent{false};
    {
        std::unique_lock lock{receiver.mutex};
        workerEvent = receiver.changed.wait_for(lock, std::chrono::seconds{5}, [&]
        {
            for (const auto& event : receiver.events)
                if (event.find("\"sourceProcessRole\":\"worker\"") != std::string::npos
                    && event.find("\"sourcePid\":" + std::to_string(GetCurrentProcessId()) + ",") == std::string::npos
                    && event.find("logdump-worker-test") != std::string::npos) return true;
            return false;
        });
    }
    Require(clear(instance.module, 5000) == SLICER_LOG_OK, "Worker callback did not clear");
    Require(status == PM_ERR_BUFFER_SMALL && required > 0, "telemetry changed Worker terminal protocol");
    Require(workerEvent && !receiver.invalid, "real Worker source PID or job identity was lost");
}
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc == 2, "expected slicer_module.dll path");
        Api api{std::filesystem::path{argv[1]}};
        TestRealEventsAndQueries(api);
        TestPublicReentry(api);
        TestDestroyWaits(api);
        TestRealWorkerTelemetry(api);
        std::cout << "module logging integration tests passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
