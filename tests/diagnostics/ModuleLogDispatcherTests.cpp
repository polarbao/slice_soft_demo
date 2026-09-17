#include "slicer_module/logging/ModuleLogDispatcher.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
using slicesoft::module::logging::ModuleLogDispatcher;
using slicesoft::diagnostics::LogEvent;
using namespace std::chrono_literals;

void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

struct Receiver
{
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::string> events;
    bool blocked{false};
    bool entered{false};
    bool release{false};
    std::atomic_bool invalid{false};
};

void PM_CALL Receive(void* context, int level, const char* json, int bytes)
{
    auto& receiver = *static_cast<Receiver*>(context);
    if (bytes <= 0 || bytes > SLICER_LOG_MAX_EVENT_BYTES
        || !slicesoft::diagnostics::ValidEvent(std::string_view{json, static_cast<std::size_t>(bytes)}, level))
        receiver.invalid = true;
    std::unique_lock lock{receiver.mutex};
    receiver.events.emplace_back(json, static_cast<std::size_t>(bytes));
    receiver.entered = true;
    receiver.changed.notify_all();
    if (receiver.blocked) receiver.changed.wait(lock, [&] { return receiver.release; });
}

bool WaitCount(Receiver& receiver, std::size_t count)
{
    std::unique_lock lock{receiver.mutex};
    return receiver.changed.wait_for(lock, 3s, [&] { return receiver.events.size() >= count; });
}

void Unblock(Receiver& receiver)
{
    std::lock_guard lock{receiver.mutex};
    receiver.release = true;
    receiver.changed.notify_all();
}

LogEvent Event(int level = 2)
{
    LogEvent event;
    event.level = level;
    event.action = "test";
    event.message = "literal {} %s message";
    return event;
}

void TestIsolationAndFiltering()
{
    Receiver first;
    Receiver second;
    ModuleLogDispatcher one{101};
    ModuleLogDispatcher two{202};
    one.Emit(Event());
    Require(one.Set(Receive, &first, 3) == SLICER_LOG_OK, "first binding failed");
    Require(two.Set(Receive, &second, 2) == SLICER_LOG_OK, "second binding failed");
    Require(one.Set(Receive, &second, 2) == SLICER_LOG_INVALID_STATE, "replacement must be rejected");
    Require(one.Set(nullptr, nullptr, 0) == SLICER_LOG_INVALID_ARGUMENT, "null callback accepted");
    one.Emit(Event(2));
    one.Emit(Event(4));
    two.Emit(Event(2));
    const bool received = WaitCount(first, 1) && WaitCount(second, 1);
    Require(one.Clear(3000) == SLICER_LOG_OK, "first clear failed");
    Require(two.Clear(3000) == SLICER_LOG_OK, "second clear failed");
    Require(received && !first.invalid && !second.invalid, "invalid or missing events");
    Require(first.events.size() == 1 && second.events.size() == 1, "filter or instance isolation failed");
    Require(first.events.front().find("101") != std::string::npos, "first identity missing");
    Require(second.events.front().find("202") != std::string::npos, "second identity missing");
    one.Emit(Event(5));
    Require(one.Clear(0) == SLICER_LOG_OK, "clear must be idempotent");
    Require(one.Set(Receive, &first, -1) == SLICER_LOG_OK, "off binding failed");
    one.Emit(Event(5));
    Require(one.Clear(3000) == SLICER_LOG_OK && first.events.size() == 1, "off must not deliver");
}

void TestTimeoutAndBudget()
{
    Receiver receiver;
    receiver.blocked = true;
    ModuleLogDispatcher dispatcher{1};
    Require(dispatcher.Set(Receive, &receiver, 0) == SLICER_LOG_OK, "binding failed");
    dispatcher.Emit(Event());
    if (!WaitCount(receiver, 1)) { Unblock(receiver); throw std::runtime_error("callback did not enter"); }
    for (int i = 0; i < 300; ++i) dispatcher.Emit(Event());
    const auto counters = dispatcher.Counters();
    const auto clear = dispatcher.Clear(10);
    const auto replacement = dispatcher.Set(Receive, &receiver, 0);
    dispatcher.Emit(Event(5));
    Unblock(receiver);
    Require(dispatcher.Clear(3000) == SLICER_LOG_OK, "retry did not quiesce callback");
    Require(clear == SLICER_LOG_TIMEOUT, "blocked callback must return timeout");
    Require(replacement == SLICER_LOG_INVALID_STATE, "timed-out context replaced");
    Require(counters.overflow == 44, "queue exceeded 256 events");
    Require(dispatcher.Counters().discarded == 256, "clear discard accounting failed");
    Require(receiver.events.size() == 1, "events delivered after stop");
    receiver.blocked = false;
    Require(dispatcher.Set(Receive, &receiver, 0) == SLICER_LOG_OK, "rebinding failed after completed clear");
    dispatcher.Emit(Event());
    const bool summary = WaitCount(receiver, 2);
    Require(dispatcher.Clear(3000) == SLICER_LOG_OK, "summary binding did not clear");
    Require(summary && receiver.events.back().find("\"queueOverflow\":44") != std::string::npos
        && receiver.events.back().find("\"discardedAtClear\":256") != std::string::npos,
        "next event did not report cumulative diagnostic loss");
}

struct Reentrant
{
    ModuleLogDispatcher* dispatcher;
    std::atomic_int clearResult{99};
    std::atomic_int setResult{99};
    std::atomic_bool completed{false};
};

void PM_CALL Reenter(void* context, int, const char*, int)
{
    auto& value = *static_cast<Reentrant*>(context);
    value.clearResult = value.dispatcher->Clear(1);
    value.setResult = value.dispatcher->Set(Reenter, context, 0);
    value.completed = true;
    throw std::runtime_error("host callback exception");
}

void TestReentryAndException()
{
    ModuleLogDispatcher dispatcher{1};
    Reentrant context{&dispatcher};
    Require(dispatcher.Set(Reenter, &context, 0) == SLICER_LOG_OK, "binding failed");
    dispatcher.Emit(Event());
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!context.completed && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    Require(dispatcher.Clear(3000) == SLICER_LOG_OK, "failed callback could not clear");
    Require(context.completed && context.clearResult == SLICER_LOG_INVALID_STATE
        && context.setResult == SLICER_LOG_INVALID_STATE, "callback reentry was not rejected");
    Require(dispatcher.Counters().callbackFailures == 1, "callback exception not isolated");
}

void TestConcurrentProducersAndClear()
{
    Receiver receiver;
    ModuleLogDispatcher dispatcher{1};
    Require(dispatcher.Set(Receive, &receiver, 0) == SLICER_LOG_OK, "binding failed");
    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i)
        producers.emplace_back([&] { for (int n = 0; n < 100; ++n) dispatcher.Emit(Event()); });
    for (auto& producer : producers) producer.join();
    int first{99};
    int second{99};
    std::thread clearOne{[&] { first = dispatcher.Clear(3000); }};
    std::thread clearTwo{[&] { second = dispatcher.Clear(3000); }};
    clearOne.join();
    clearTwo.join();
    const auto counters = dispatcher.Counters();
    Require(first == SLICER_LOG_OK && second == SLICER_LOG_OK, "concurrent clear failed");
    Require(!receiver.invalid && counters.delivered + counters.overflow + counters.discarded == 400,
        "concurrent accounting or JSON validity failed");
}
}

int main()
{
    try
    {
        TestIsolationAndFiltering();
        TestTimeoutAndBudget();
        TestReentryAndException();
        TestConcurrentProducersAndClear();
        std::cout << "module log dispatcher tests passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
