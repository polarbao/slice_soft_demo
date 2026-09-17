#pragma once
#include "diagnostics/LogEvent.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace slicesoft::diagnostics
{
enum class LogChannel { Application, Module, Rip };
using HostLogSink = std::function<void(LogChannel, int, std::string_view)>;
struct LogSessionOptions
{
    std::filesystem::path directory;
    int minimumLevel{2};
    std::size_t queueCapacity{1024};
    std::size_t maximumFileBytes{10 * 1024 * 1024};
    std::size_t archiveCount{3};
    HostLogSink sink;
    std::string processRole{"host"};
};
struct LogSessionStatus
{
    std::uint64_t accepted{0}, dropped{0}, written{0}, failed{0};
    std::string lastError;
};

// EXE-owned asynchronous service. The injected sink runs on its own consumer
// thread, must return promptly, and must not destroy/reenter the service.
class LogSession final
{
public:
    static std::shared_ptr<LogSession> Create(LogSessionOptions options, std::string* error) noexcept;
    ~LogSession();
    bool Submit(LogChannel channel, int level, std::string_view json) noexcept;
    void Write(LogChannel channel, const LogEvent& event) noexcept;
    LogSessionStatus Status() const;
    const std::filesystem::path& Directory() const noexcept;
    const std::string& ProcessRole() const noexcept;
    int MinimumLevel() const noexcept;
    // Stops intake and joins/drains the consumer. OS/sink stalls cannot be
    // force-cancelled safely; this is not a hard real-time shutdown guarantee.
    void Close() noexcept;
private:
    class Impl;
    explicit LogSession(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
}
