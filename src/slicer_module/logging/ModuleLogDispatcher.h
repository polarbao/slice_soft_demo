#pragma once

#include "contracts/slicer_logging.h"
#include "diagnostics/LogEvent.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace slicesoft::module::logging
{
struct ModuleLogCounters
{
    std::uint64_t delivered{0};
    std::uint64_t overflow{0};
    std::uint64_t discarded{0};
    std::uint64_t callbackFailures{0};
    std::uint64_t encodingFailures{0};
};

// A dormant instance owns no thread, files, host logger, or crash handler.
class ModuleLogDispatcher final
{
public:
    static constexpr std::size_t QueueCapacity = 256;
    explicit ModuleLogDispatcher(std::uint64_t moduleId);
    ~ModuleLogDispatcher();
    ModuleLogDispatcher(const ModuleLogDispatcher&) = delete;
    ModuleLogDispatcher& operator=(const ModuleLogDispatcher&) = delete;

    int Set(slicer_log_callback_v1 callback, void* context, int minimumLevel) noexcept;
    int Clear(int timeoutMs) noexcept;
    void Emit(diagnostics::LogEvent event) noexcept;
    bool IsCallbackThread() const noexcept;
    static bool IsModuleCallbackThread(std::uint64_t moduleId) noexcept;
    ModuleLogCounters Counters() const noexcept;
    bool Enabled() const noexcept;

private:
    struct PendingEvent { int level; std::string json; };
    void Run() noexcept;
    const std::string m_moduleId;
    const std::uint64_t m_identity;
    mutable std::mutex m_mutex;
    std::condition_variable m_changed;
    std::deque<PendingEvent> m_pending;
    std::thread m_thread;
    slicer_log_callback_v1 m_callback{nullptr};
    void* m_context{nullptr};
    int m_minimumLevel{-1};
    bool m_active{false};
    bool m_finished{true};
    bool m_joining{false};
    unsigned m_clearers{0};
    std::uint64_t m_sequence{0};
    ModuleLogCounters m_counters;
};
}
