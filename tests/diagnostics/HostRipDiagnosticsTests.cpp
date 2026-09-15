#include "apps/slicer_ui_host_sim/HostRipDiagnostics.h"
#include <nlohmann/json.hpp>
#include <QString>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using namespace slicesoft::diagnostics;
namespace
{
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
class EnvironmentOverride final
{
public:
    EnvironmentOverride(std::wstring name, const std::wstring& value) : m_name(std::move(name))
    {
        const auto size = GetEnvironmentVariableW(m_name.c_str(), nullptr, 0);
        if (size)
        {
            std::wstring old(size, L'\0');
            const auto written = GetEnvironmentVariableW(m_name.c_str(), old.data(), size);
            Require(written < size, "environment changed while saving test configuration");
            old.resize(written);
            m_old = std::move(old);
        }
        Require(SetEnvironmentVariableW(m_name.c_str(), value.c_str()) != FALSE,
            "test environment override failed");
    }
    ~EnvironmentOverride()
    {
        SetEnvironmentVariableW(m_name.c_str(), m_old ? m_old->c_str() : nullptr);
    }
    EnvironmentOverride(const EnvironmentOverride&) = delete;
    EnvironmentOverride& operator=(const EnvironmentOverride&) = delete;
private:
    std::wstring m_name;
    std::optional<std::wstring> m_old;
};
void LongRipOutput(const std::filesystem::path& root)
{
    EnvironmentOverride enabled(L"SLICESOFT_DIAGNOSTICS_ENABLED", L"1");
    EnvironmentOverride dump(L"SLICESOFT_DUMP_ENABLED", L"0");
    EnvironmentOverride level(L"SLICESOFT_LOG_LEVEL", L"info");
    EnvironmentOverride directory(L"SLICESOFT_DIAGNOSTICS_DIR", root.wstring());
    const auto stdoutText = QByteArray("START_")
        + QStringLiteral("\u6d4b\u8bd5\u8f93\u51fa-").repeated(1900).toUtf8()
        + QByteArray("_STDOUT_TAIL_MARKER");
    const auto stderrText = QByteArray("ERR_")
        + QStringLiteral("\u9519\u8bef\u4fe1\u606f-").repeated(700).toUtf8()
        + QByteArray("_STDERR_TAIL_MARKER");
    Require(stdoutText.size() > 2048 && stderrText.size() > 2048, "test payload too short");
    std::filesystem::path sessionDirectory;
    {
        ProcessDiagnostics diagnostics("rip_test", "test");
        Require(diagnostics.Session() != nullptr, "test process diagnostics failed to initialize");
        sessionDirectory = diagnostics.Session()->Directory();
        QByteArray capture;
        AppendCapped(&capture, stdoutText);
        Require(capture == stdoutText, "RIP stdout business capture changed");
        constexpr int maximumCapturedBytes = 1024 * 1024;
        QByteArray capped(maximumCapturedBytes - 3, 'x');
        AppendCapped(&capped, stderrText, "stderr");
        Require(capped.size() == maximumCapturedBytes && capped.right(3) == stderrText.left(3),
            "RIP business capture no longer obeys the original one MiB byte cap");
    }
    std::ifstream stream(sessionDirectory / "rip.log", std::ios::binary);
    Require(stream.is_open(), "RIP log was not saved");
    std::string line, savedStdout, savedStderr;
    std::size_t count = 0;
    while (std::getline(stream, line))
    {
        const auto event = nlohmann::json::parse(line);
        const auto phase = event.at("phase").get<std::string>();
        Require(phase == "stdout" || phase == "stderr", "unexpected RIP log phase");
        Require(ValidEvent(line, phase == "stderr" ? 3 : 2), "invalid RIP JSON record");
        Require(event.at("action") == "rip_process", "RIP action changed");
        Require(!event.at("truncated").get<bool>(), "RIP chunk was still truncated");
        (phase == "stderr" ? savedStderr : savedStdout) += event.at("message").get<std::string>();
        ++count;
    }
    Require(count > 2, "long RIP output was not split into bounded events");
    Require(savedStdout == stdoutText.toStdString(), "RIP stdout Unicode content or tail was lost");
    Require(savedStderr == stderrText.toStdString(), "RIP stderr Unicode content or tail was lost");
}
}
int main(int argc, char** argv)
{
    try
    {
        Require(argc == 2, "expected test evidence root");
        const auto root = std::filesystem::absolute(std::filesystem::path(argv[1]))
            / ("rip_chunks_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount64()))
            / std::filesystem::path(L"Unicode_\x65e5\x5fd7");
        Require(!std::filesystem::exists(root), "test evidence directory already exists");
        LongRipOutput(root);
        std::cout << "LOGDUMP host RIP output PASS\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
