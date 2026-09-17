#include "ModuleClient.h"
#include "diagnostics/host/ProcessDiagnostics.h"
#include "diagnostics/host/ModuleLogBinding.h"

void ModuleClient::AttachLogging() noexcept
{
    using namespace slicesoft::diagnostics;
    try
    {
        auto session = ApplicationLog();
        if (!session) return;
        const auto level = session->MinimumLevel();
        m_logging = std::make_unique<ModuleLogBinding>(
            ResolveModuleLogging(m_library), m_module, std::move(session), level);
        WriteApplicationLog("module_logging", m_logging->Attached() ? "attached" : "unsupported",
            m_logging->Attached() ? "Module log callback registered" : "Optional log extension unavailable",
            m_logging->Attached() ? 2 : 3);
    }
    catch (...) { WriteApplicationLog("module_logging", "unavailable", "Optional logger initialization failed", 3); }
}
