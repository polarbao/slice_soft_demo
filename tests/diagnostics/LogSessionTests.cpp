#include "diagnostics/host/LogSession.h"
#include "diagnostics/host/ModuleLogBinding.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace slicesoft::diagnostics;
namespace
{
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
std::string Event(int level = 2)
{
    LogEvent value; value.level = level; value.message = "test {message}";
    return EncodeEvent(value);
}
struct ClearRetryFixture
{
    slicer_log_callback_v1 callback = nullptr;
    void* context = nullptr;
    int calls = 0;
    bool validBorrow = true;
    std::string event = Event();
};
int PM_CALL LoggingVersion() { return SLICER_LOG_API_VERSION; }
int PM_CALL SetBorrowedCallback(pm_module_t* module, slicer_log_callback_v1 callback,
    void* context, int)
{
    auto& fixture = *reinterpret_cast<ClearRetryFixture*>(module);
    fixture.callback = callback;
    fixture.context = context;
    return SLICER_LOG_OK;
}
int PM_CALL RetryClear(pm_module_t* module, int timeoutMs)
{
    auto& fixture = *reinterpret_cast<ClearRetryFixture*>(module);
    ++fixture.calls;
    fixture.validBorrow = fixture.validBorrow && fixture.callback && fixture.context && timeoutMs >= 0;
    if (fixture.callback && fixture.context)
        fixture.callback(fixture.context, 2, fixture.event.data(), static_cast<int>(fixture.event.size()));
    if (fixture.calls == 1) return SLICER_LOG_RESOURCE_ERROR;
    if (fixture.calls == 2) return SLICER_LOG_TIMEOUT;
    return SLICER_LOG_OK;
}
void BindingClearRetriesPreserveContext()
{
    std::vector<std::string> received;
    LogSessionOptions options;
    options.sink = [&](LogChannel channel, int level, std::string_view event)
    {
        Require(channel == LogChannel::Module && level == 2, "borrowed callback routing changed");
        received.emplace_back(event);
    };
    auto session = LogSession::Create(options, nullptr);
    Require(session != nullptr, "binding retry session unavailable");
    ClearRetryFixture fixture;
    {
        ModuleLogBinding binding({LoggingVersion, SetBorrowedCallback, RetryClear},
            reinterpret_cast<pm_module_t*>(&fixture), session);
        Require(binding.Attached(), "test callback failed to attach");
    }
    session->Close();
    Require(fixture.calls == 3, "binding destruction did not wait for successful clear");
    Require(fixture.validBorrow, "callback borrow became invalid before successful clear");
    Require(received.size() == 3 && session->Status().failed == 0,
        "borrowed callback/context was not usable during every clear retry");
    for (const auto& event : received)
        Require(event == fixture.event, "borrowed callback payload changed during clear retries");
}
void BackpressureAndFailures()
{
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false, release = false;
    LogSessionOptions options;
    options.queueCapacity = 2;
    options.sink = [&](LogChannel, int, std::string_view)
    {
        std::unique_lock lock(mutex);
        entered = true; changed.notify_all();
        changed.wait(lock, [&] { return release; });
    };
    std::string error;
    const auto session = LogSession::Create(options, &error);
    Require(session != nullptr, "injected host sink init failed");
    Require(session->Submit(LogChannel::Module, 2, Event()), "first event refused");
    {
        std::unique_lock lock(mutex);
        Require(changed.wait_for(lock, std::chrono::seconds(3), [&] { return entered; }), "sink did not start");
    }
    const bool second = session->Submit(LogChannel::Module, 2, Event());
    const bool third = session->Submit(LogChannel::Module, 2, Event());
    const bool overflow = session->Submit(LogChannel::Module, 2, Event());
    { std::lock_guard lock(mutex); release = true; }
    changed.notify_all();
    session->Close();
    Require(second && third && !overflow, "bounded queue policy failed");
    const auto status = session->Status();
    Require(status.accepted == 3 && status.written == 3 && status.dropped == 1, "queue accounting incorrect");
    Require(!session->Submit(LogChannel::Module, 2, Event()), "closed session accepted data");
    options.sink = [](LogChannel, int, std::string_view) { throw std::runtime_error("simulated disk failure"); };
    auto failing = LogSession::Create(options, &error);
    failing->Submit(LogChannel::Application, 2, Event());
    failing->Close();
    Require(failing->Status().failed == 1 && !failing->Status().lastError.empty(), "sink failure hidden");
}
void FileAndFormat(const std::filesystem::path& root)
{
    const auto defaultLogger = spdlog::default_logger();
    LogSessionOptions options;
    options.directory = root / std::filesystem::path(L"Unicode_\x65e5\x5fd7");
    options.maximumFileBytes = MaximumEventBytes;
    options.archiveCount = 2;
    std::string error;
    auto session = LogSession::Create(options, &error);
    Require(session != nullptr, "Unicode file sink unavailable");
    Require(!session->Submit(LogChannel::Application, 0, Event(0)), "minimum level ignored");
    Require(!session->Submit(LogChannel::Application, 2, "{}"), "malformed event admitted");
    LogEvent event; event.message = std::string(9000, '\x01');
    const auto encoded = EncodeEvent(event);
    Require(encoded.size() <= MaximumEventBytes && ValidEvent(encoded, 2), "event budget failed");
    Require(nlohmann::json::parse(encoded)["truncated"].get<bool>(), "truncation not reported");
    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i)
        producers.emplace_back([session, encoded] { for (int n = 0; n < 20; ++n) session->Submit(LogChannel::Application, 2, encoded); });
    for (auto& thread : producers) thread.join();
    session->Close();
    Require(session->Status().written == 80 && session->Status().failed == 0, "concurrent file writes lost");
    std::size_t files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(options.directory))
    {
        if (entry.path().filename().string().starts_with("app.log"))
        {
            ++files;
            Require(entry.file_size() <= MaximumEventBytes, "rotated file exceeds cap");
            std::ifstream stream(entry.path()); std::string line;
            while (std::getline(stream, line)) Require(ValidEvent(line, 2), "partial/invalid event on disk");
        }
    }
    Require(files == 3, "archive count wrong");
    Require(spdlog::default_logger() == defaultLogger, "host default logger changed");
    options.directory = root / "blocked";
    { std::ofstream marker(options.directory); marker << "file blocks directory"; }
    Require(!LogSession::Create(options, &error) && !error.empty(), "invalid directory silently accepted");
    options.directory = root / "retry";
    Require(LogSession::Create(options, &error) != nullptr, "failed init could not be retried");
}
}
int main(int argc, char** argv)
{
    try
    {
        Require(argc == 2, "expected output directory");
        const auto root = std::filesystem::path(argv[1]) / std::to_string(CurrentProcessId());
        std::filesystem::create_directories(root);
        BackpressureAndFailures();
        FileAndFormat(root);
        BindingClearRetriesPreserveContext();
        ModuleLogBinding oldDll({}, nullptr, {});
        Require(!oldDll.Attached(), "old DLL fallback failed");
        std::cout << "LOGDUMP host logs PASS\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
