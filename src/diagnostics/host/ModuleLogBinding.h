#pragma once
#include "contracts/slicer_logging.h"
#include "LogSession.h"
#include <memory>

namespace slicesoft::diagnostics
{
struct ModuleLogFunctions
{
    using Version = int (PM_CALL*)();
    using Set = int (PM_CALL*)(pm_module_t*, slicer_log_callback_v1, void*, int);
    using Clear = int (PM_CALL*)(pm_module_t*, int);
    Version version{nullptr};
    Set set{nullptr};
    Clear clear{nullptr};
};
// The EXE must keep the loaded DLL and module alive until this binding is reset.
class ModuleLogBinding final
{
public:
    ModuleLogBinding(ModuleLogFunctions functions, pm_module_t* module,
        std::shared_ptr<LogSession> session, int minimumLevel = 2);
    ~ModuleLogBinding();
    ModuleLogBinding(const ModuleLogBinding&) = delete;
    ModuleLogBinding& operator=(const ModuleLogBinding&) = delete;
    bool Attached() const noexcept { return m_attached; }
    int Clear(int timeoutMs) noexcept;
private:
    static void PM_CALL Receive(void*, int, const char*, int) noexcept;
    ModuleLogFunctions m_functions;
    pm_module_t* m_module;
    std::shared_ptr<LogSession> m_session;
    bool m_attached{false};
};
ModuleLogFunctions ResolveModuleLogging(void* library) noexcept;
}
