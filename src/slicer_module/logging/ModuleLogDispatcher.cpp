#include "ModuleLogDispatcher.h"

#include <chrono>
#include <utility>

namespace slicesoft::module::logging
{
namespace
{
thread_local const ModuleLogDispatcher* currentDispatcher{nullptr};
}

ModuleLogDispatcher::ModuleLogDispatcher(const std::uint64_t moduleId)
    : m_moduleId{std::to_string(moduleId)}, m_identity{moduleId}
{
}

ModuleLogDispatcher::~ModuleLogDispatcher()
{
    // The registry prevents destruction from this dispatcher's own callback.
    (void)Clear(-1);
}

bool ModuleLogDispatcher::IsCallbackThread() const noexcept
{
    return currentDispatcher == this;
}

bool ModuleLogDispatcher::IsModuleCallbackThread(const std::uint64_t moduleId) noexcept
{
    return currentDispatcher != nullptr && currentDispatcher->m_identity == moduleId;
}

int ModuleLogDispatcher::Set(
    const slicer_log_callback_v1 callback, void* const context,
    const int minimumLevel) noexcept
{
    if (callback == nullptr || minimumLevel < -1 || minimumLevel > 5)
        return SLICER_LOG_INVALID_ARGUMENT;
    if (IsCallbackThread()) return SLICER_LOG_INVALID_STATE;
    try
    {
        std::lock_guard lock{m_mutex};
        if (m_thread.joinable() || m_joining || m_clearers != 0) return SLICER_LOG_INVALID_STATE;
        m_callback = callback;
        m_context = context;
        m_minimumLevel = minimumLevel;
        m_active = true;
        m_finished = false;
        try { m_thread = std::thread{&ModuleLogDispatcher::Run, this}; }
        catch (...)
        {
            m_callback = nullptr;
            m_context = nullptr;
            m_active = false;
            m_finished = true;
            return SLICER_LOG_RESOURCE_ERROR;
        }
        return SLICER_LOG_OK;
    }
    catch (...) { return SLICER_LOG_RESOURCE_ERROR; }
}

int ModuleLogDispatcher::Clear(const int timeoutMs) noexcept
{
    if (timeoutMs < -1) return SLICER_LOG_INVALID_ARGUMENT;
    if (IsCallbackThread()) return SLICER_LOG_INVALID_STATE;
    try
    {
        std::unique_lock lock{m_mutex};
        ++m_clearers;
        struct ClearCountGuard { unsigned& count; ~ClearCountGuard() { --count; } } clearing{m_clearers};
        m_active = false;
        m_counters.discarded += m_pending.size();
        m_pending.clear();
        m_changed.notify_all();
        const auto ready = [this] { return m_finished && !m_joining; };
        if (timeoutMs < 0) m_changed.wait(lock, ready);
        else if (!m_changed.wait_for(lock, std::chrono::milliseconds{timeoutMs}, ready))
            return SLICER_LOG_TIMEOUT;
        if (!m_thread.joinable()) return SLICER_LOG_OK;
        // Only this clearer may join; concurrent clear calls remain barriers.
        m_joining = true;
        auto stopped = std::move(m_thread);
        lock.unlock();
        stopped.join();
        lock.lock();
        m_callback = nullptr;
        m_context = nullptr;
        m_joining = false;
        m_changed.notify_all();
        return SLICER_LOG_OK;
    }
    catch (...) { return SLICER_LOG_RESOURCE_ERROR; }
}

void ModuleLogDispatcher::Emit(diagnostics::LogEvent event) noexcept
{
    if (event.level < 0 || event.level > 5) return;
    try
    {
        std::lock_guard lock{m_mutex};
        if (!m_active || m_minimumLevel < 0 || event.level < m_minimumLevel) return;
        if (m_pending.size() >= QueueCapacity)
        {
            ++m_counters.overflow;
            return;
        }
        event.domain = "slicer.module";
        event.moduleInstanceId = m_moduleId;
        event.sequence = ++m_sequence;
        event.droppedEventCount = m_counters.overflow;
        event.discardedEventCount = m_counters.discarded;
        event.callbackFailureCount = m_counters.callbackFailures;
        event.encodingFailureCount = m_counters.encodingFailures;
        auto json = diagnostics::EncodeEvent(event);
        if (json.empty()) { ++m_counters.encodingFailures; return; }
        m_pending.push_back({event.level, std::move(json)});
        m_changed.notify_one();
    }
    catch (...)
    {
        try { std::lock_guard lock{m_mutex}; ++m_counters.encodingFailures; }
        catch (...) {}
    }
}

ModuleLogCounters ModuleLogDispatcher::Counters() const noexcept
{
    try { std::lock_guard lock{m_mutex}; return m_counters; }
    catch (...) { return {}; }
}

bool ModuleLogDispatcher::Enabled() const noexcept
{
    try { std::lock_guard lock{m_mutex}; return m_active && m_minimumLevel >= 0; }
    catch (...) { return false; }
}

void ModuleLogDispatcher::Run() noexcept
{
    currentDispatcher = this;
    std::unique_lock lock{m_mutex};
    for (;;)
    {
        m_changed.wait(lock, [this] { return !m_active || !m_pending.empty(); });
        if (!m_active) break;
        auto event = std::move(m_pending.front());
        m_pending.pop_front();
        const auto callback = m_callback;
        void* const context = m_context;
        lock.unlock();
        bool failed{false};
        try { callback(context, event.level, event.json.data(), static_cast<int>(event.json.size())); }
        catch (...) { failed = true; }
        lock.lock();
        if (failed)
        {
            ++m_counters.callbackFailures;
            m_active = false;
            m_counters.discarded += m_pending.size();
            m_pending.clear();
        }
        else ++m_counters.delivered;
    }
    currentDispatcher = nullptr;
    m_finished = true;
    m_changed.notify_all();
}
}
