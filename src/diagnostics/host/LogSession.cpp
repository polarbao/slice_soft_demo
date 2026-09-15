#include "LogSession.h"
#include "diagnostics/spdlog/FileLogSink.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace slicesoft::diagnostics
{
class LogSession::Impl
{
public:
    struct Record { LogChannel channel; int level; std::string json; };
    explicit Impl(LogSessionOptions value) : options(std::move(value))
    {
        if (options.minimumLevel < -1 || options.minimumLevel > 5
            || options.queueCapacity == 0 || options.queueCapacity > 4096
            || options.maximumFileBytes < MaximumEventBytes || options.archiveCount == 0
            || options.archiveCount > 10)
            throw std::invalid_argument("Invalid diagnostic log settings");
        if (!options.sink) options.sink = CreateFileLogSink(options);
        worker = std::thread([this] { Consume(); });
    }
    ~Impl() { Close(); }
    void Close() noexcept
    {
        std::lock_guard closeLock(closeMutex);
        { std::lock_guard lock(mutex); stopping = true; }
        ready.notify_all();
        if (worker.joinable()) worker.join();
    }
    void Consume() noexcept
    {
        for (;;)
        {
            Record record;
            {
                std::unique_lock lock(mutex);
                ready.wait(lock, [this] { return stopping || !queue.empty(); });
                if (queue.empty()) return;
                record = std::move(queue.front());
                queue.pop_front();
            }
            try
            {
                options.sink(record.channel, record.level, record.json);
                std::lock_guard lock(mutex);
                ++status.written;
            }
            catch (const std::exception& error)
            {
                std::lock_guard lock(mutex);
                ++status.failed;
                try { status.lastError = error.what(); } catch (...) {}
            }
            catch (...)
            {
                std::lock_guard lock(mutex);
                ++status.failed;
            }
        }
    }
    LogSessionOptions options;
    mutable std::mutex mutex;
    std::mutex closeMutex;
    std::condition_variable ready;
    std::deque<Record> queue;
    LogSessionStatus status;
    bool stopping{false};
    std::thread worker;
};

LogSession::LogSession(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
LogSession::~LogSession() = default;
std::shared_ptr<LogSession> LogSession::Create(LogSessionOptions options, std::string* error) noexcept
{
    try { return std::shared_ptr<LogSession>(new LogSession(std::make_unique<Impl>(std::move(options)))); }
    catch (const std::exception& exception)
    {
        if (error) { try { *error = exception.what(); } catch (...) {} }
    }
    catch (...) {}
    return {};
}
bool LogSession::Submit(LogChannel channel, int level, std::string_view json) noexcept
{
    try
    {
        if (static_cast<unsigned>(channel) > 2 || !ValidEvent(json, level)) return false;
        std::lock_guard lock(m_impl->mutex);
        if (m_impl->stopping || m_impl->options.minimumLevel < 0
            || level < m_impl->options.minimumLevel) return false;
        if (m_impl->queue.size() >= m_impl->options.queueCapacity)
        {
            ++m_impl->status.dropped;
            return false;
        }
        m_impl->queue.push_back({channel, level, std::string(json)});
        ++m_impl->status.accepted;
        m_impl->ready.notify_one();
        return true;
    }
    catch (...) { return false; }
}
void LogSession::Write(LogChannel channel, const LogEvent& event) noexcept
{
    Submit(channel, event.level, EncodeEvent(event));
}
LogSessionStatus LogSession::Status() const
{
    std::lock_guard lock(m_impl->mutex);
    return m_impl->status;
}
const std::filesystem::path& LogSession::Directory() const noexcept { return m_impl->options.directory; }
const std::string& LogSession::ProcessRole() const noexcept { return m_impl->options.processRole; }
int LogSession::MinimumLevel() const noexcept { return m_impl->options.minimumLevel; }
void LogSession::Close() noexcept { m_impl->Close(); }
}
