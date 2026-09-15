#pragma once
#include "LogSession.h"
#include "SessionRetention.h"
#include "diagnostics/windows/CrashReporter.h"

namespace slicesoft::diagnostics
{
// Application startup owns this object. Do not construct it from a DLL.
class ProcessDiagnostics final
{
public:
    explicit ProcessDiagnostics(std::string role, std::string version = {});
    ~ProcessDiagnostics();
    const std::shared_ptr<LogSession>& Session() const noexcept { return m_session; }
    const std::string& StartupStatus() const noexcept { return m_status; }
    bool DumpEnabled() const noexcept { return m_crashReporter.Enabled(); }
private:
    std::unique_ptr<SessionLease> m_lease;
    std::shared_ptr<LogSession> m_session;
    CrashReporter m_crashReporter;
    std::string m_role;
    std::string m_status;
};
std::shared_ptr<LogSession> ApplicationLog() noexcept;
void WriteApplicationLog(std::string_view action, std::string_view phase,
    std::string_view message, int level = 2, std::string_view code = {},
    LogChannel channel = LogChannel::Application) noexcept;
}
