#include "ModuleLogBinding.h"
#include <chrono>
#include <thread>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace slicesoft::diagnostics
{
ModuleLogFunctions ResolveModuleLogging(void* library) noexcept
{
    ModuleLogFunctions functions;
#ifdef _WIN32
    if (library)
    {
        const auto module = static_cast<HMODULE>(library);
        functions.version = reinterpret_cast<ModuleLogFunctions::Version>(GetProcAddress(module, "slicer_log_api_version"));
        functions.set = reinterpret_cast<ModuleLogFunctions::Set>(GetProcAddress(module, "slicer_set_log_callback_v1"));
        functions.clear = reinterpret_cast<ModuleLogFunctions::Clear>(GetProcAddress(module, "slicer_clear_log_callback_v1"));
    }
#endif
    return functions;
}
ModuleLogBinding::ModuleLogBinding(ModuleLogFunctions functions, pm_module_t* module,
    std::shared_ptr<LogSession> session, int minimumLevel)
    : m_functions(functions), m_module(module), m_session(std::move(session))
{
    if (m_session && functions.version && functions.set && functions.clear
        && functions.version() == SLICER_LOG_API_VERSION)
        m_attached = functions.set(module, Receive, this, minimumLevel) == SLICER_LOG_OK;
}
ModuleLogBinding::~ModuleLogBinding()
{
    // Our thunk only copies bounded data. Preserve its lifetime even if an
    // unexpected delay occurs; never unload code beneath an active callback.
    while (Clear(5000) != SLICER_LOG_OK)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
int ModuleLogBinding::Clear(int timeoutMs) noexcept
{
    if (!m_attached) return SLICER_LOG_OK;
    const int result = m_functions.clear(m_module, timeoutMs);
    if (result == SLICER_LOG_OK) m_attached = false;
    return result;
}
void PM_CALL ModuleLogBinding::Receive(void* context, int level, const char* json, int size) noexcept
{
    if (!context || !json || size <= 0 || size > SLICER_LOG_MAX_EVENT_BYTES) return;
    const auto self = static_cast<ModuleLogBinding*>(context);
    self->m_session->Submit(LogChannel::Module, level, {json, static_cast<std::size_t>(size)});
}
}
