#include "ProcessDiagnostics.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace slicesoft::diagnostics
{
namespace
{
std::atomic<std::shared_ptr<LogSession>> activeSession;
std::wstring Environment(const wchar_t* name)
{
    const auto size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) return {};
    std::wstring result(size, L'\0');
    const auto written = GetEnvironmentVariableW(name, result.data(), size);
    if (written == 0 || written >= size) return {};
    result.resize(written);
    return result;
}
std::filesystem::path ExecutableDirectory()
{
    std::wstring path(32768, L'\0');
    const auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (size == 0 || size >= path.size()) return {};
    path.resize(size);
    return std::filesystem::path(path).parent_path();
}
}
std::shared_ptr<LogSession> ApplicationLog() noexcept { return activeSession.load(); }
void WriteApplicationLog(std::string_view action, std::string_view phase,
    std::string_view message, int level, std::string_view code, LogChannel channel) noexcept
{
    try
    {
        if (auto session = ApplicationLog())
        {
            LogEvent event;
            event.level = level;
            event.module = channel == LogChannel::Rip ? "rip" : "application";
            event.sourceProcessRole = session->ProcessRole();
            event.action = action.substr(0, 128); event.phase = phase.substr(0, 128);
            event.message = message.substr(0, MaximumEventBytes); event.code = code.substr(0, 128);
            session->Write(channel, event);
        }
    }
    catch (...) {}
}
ProcessDiagnostics::ProcessDiagnostics(std::string role, std::string version) : m_role(std::move(role))
{
    try
    {
        if (Environment(L"SLICESOFT_DIAGNOSTICS_ENABLED") == L"0")
        {
            m_status = "Diagnostics explicitly disabled";
            return;
        }
        auto root = std::filesystem::path(Environment(L"SLICESOFT_DIAGNOSTICS_DIR"));
        if (root.empty()) root = std::filesystem::path(Environment(L"LOCALAPPDATA")) / "SliceSoft" / "diagnostics";
        if (!root.is_absolute()) throw std::runtime_error("Diagnostics root must be absolute");
        m_lease = SessionLease::Create(root, m_role, &m_status);
        if (!m_lease) throw std::runtime_error(m_status);
        const auto retention = ApplySessionRetention(root, m_lease->Directory());
        LogSessionOptions options;
        options.processRole = m_role;
        options.directory = m_lease->Directory();
        const auto level = Environment(L"SLICESOFT_LOG_LEVEL");
        if (level == L"debug") options.minimumLevel = 1;
        else if (level == L"trace") options.minimumLevel = 0;
        else if (level == L"off") options.minimumLevel = -1;
        else if (level == L"warn") options.minimumLevel = 3;
        m_session = LogSession::Create(std::move(options), &m_status);
        if (!m_session) { std::fprintf(stderr, "Diagnostics unavailable: %s\n", m_status.c_str()); return; }
        activeSession.store(m_session);
        LogEvent event;
        event.sourceProcessRole = m_role; event.module = m_role;
        event.action = "startup"; event.message = version;
        m_session->Write(LogChannel::Application, event);
        WriteApplicationLog("retention", retention.budgetExceeded || retention.errors ? "incomplete" : "complete",
            "removedSessions=" + std::to_string(retention.removedSessions)
            + "; skippedActive=" + std::to_string(retention.skippedActive)
            + "; errors=" + std::to_string(retention.errors), retention.budgetExceeded || retention.errors ? 3 : 2);
        if (Environment(L"SLICESOFT_DUMP_ENABLED") != L"0")
        {
            CrashReporterOptions crashOptions;
            crashOptions.helperExecutable = ExecutableDirectory() / "slicer_crash_reporter.exe";
            crashOptions.dumpDirectory = m_session->Directory() / "dumps";
            crashOptions.replaceExistingFilter = true;
            const auto result = m_crashReporter.Start(crashOptions);
            m_status = result.reason;
            WriteApplicationLog("crash_reporter", result.enabled ? "ready" : "unavailable",
                result.reason, result.enabled ? 2 : 3, std::to_string(result.systemError));
        }
    }
    catch (const std::exception& error) { m_status = error.what(); std::fprintf(stderr, "Diagnostics unavailable: %s\n", error.what()); }
}
ProcessDiagnostics::~ProcessDiagnostics()
{
    m_crashReporter.Stop();
    if (m_session)
    {
        WriteApplicationLog("shutdown", "end", m_role);
        auto expected = m_session;
        activeSession.compare_exchange_strong(expected, {});
        m_session->Close();
    }
}
}
