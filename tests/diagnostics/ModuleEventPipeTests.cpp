#include "diagnostics/LogEvent.h"
#include "diagnostics/transport/EventPipe.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <chrono>
#include <cwchar>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace slicesoft::diagnostics;
using namespace slicesoft::diagnostics::transport;

void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

void TestMessageBudgetAndAuthentication()
{
    EventPipeServer server;
    Require(!server.Endpoint().empty(), "current-user named pipe creation failed");
    EventPipeClient client;
    Require(client.Connect(server.Endpoint(), GetCurrentProcessId()), "local pipe connection failed");
    LogEvent event;
    event.message = "pipe test";
    event.sourceProcessRole = "worker";
    const auto json = EncodeEvent(event);
    Require(client.Send(json), "bounded message send failed");
    int received{0};
    server.Poll(GetCurrentProcessId(), [&](std::string_view data, std::uint32_t pid)
        { Require(pid == GetCurrentProcessId() && data == json, "payload or verified PID drift"); ++received; });
    Require(received == 1, "message not received");
    Require(!client.Send(std::string(MaximumEventBytes + 1, 'x')), "oversized send accepted");
    const std::string full(MaximumEventBytes, 'x');
    const auto started = std::chrono::steady_clock::now();
    int dropped{0};
    for (int i = 0; i < 1000; ++i) if (!client.Send(full)) ++dropped;
    Require(dropped > 0 && std::chrono::steady_clock::now() - started < std::chrono::seconds{2},
        "full message pipe blocked or exceeded its buffer");
    server.Poll(GetCurrentProcessId() + 1, [&](std::string_view, std::uint32_t) { ++received; });
    Require(received == 1, "unexpected client PID was trusted");
    Require(!client.Send(json), "rejected connection remained writable");

    EventPipeServer wrongParent;
    EventPipeClient rejected;
    Require(!rejected.Connect(wrongParent.Endpoint(), GetCurrentProcessId() + 1), "unexpected server PID trusted");
    Require(!rejected.Connect(L"\\\\remote\\pipe\\slicesoft-log-1", GetCurrentProcessId()), "remote endpoint accepted");
}

std::vector<std::wstring> Entries(const std::vector<wchar_t>& environment)
{
    std::vector<std::wstring> entries;
    for (const auto* cursor = environment.data(); *cursor != L'\0'; cursor += std::wcslen(cursor) + 1)
        entries.emplace_back(cursor);
    return entries;
}

void TestEnvironment()
{
    const auto original = WorkerEnvironment(L"");
    Require(original.size() >= 2 && original.back() == L'\0' && original[original.size() - 2] == L'\0',
        "Unicode environment lacks double termination");
    EventPipeServer server;
    const auto withPipe = Entries(WorkerEnvironment(server.Endpoint()));
    Require(withPipe.size() == Entries(original).size() + 2, "private environment entries did not replace exactly two keys");
    Require(original == WorkerEnvironment(L""), "constructing child environment mutated parent state");
    auto inherited = GetEnvironmentStringsW();
    Require(inherited != nullptr, "cannot read inherited environment");
    bool preserved{true};
    for (const wchar_t* cursor = inherited; *cursor; cursor += std::wcslen(cursor) + 1)
        if (*cursor == L'=')
        {
            const auto entries = Entries(original);
            bool found{false};
            for (const auto& entry : entries) found = found || entry == cursor;
            preserved = preserved && found;
        }
    FreeEnvironmentStringsW(inherited);
    Require(preserved, "drive-specific =C: entries were lost");
}

int Child()
{
    EventPipeClient client;
    if (!ConnectFromWorkerEnvironment(client)) return 10;
    LogEvent event;
    event.sourceProcessRole = "worker";
    event.message = "short lived child";
    return client.Send(EncodeEvent(event)) ? 0 : 11;
}

void TestShortLivedProcess()
{
    EventPipeServer server;
    auto environment = WorkerEnvironment(server.Endpoint());
    std::vector<wchar_t> path(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    Require(length > 0 && length < path.size(), "cannot resolve test executable");
    std::wstring command = std::wstring{L"\""} + path.data() + L"\" --child";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    Require(CreateProcessW(path.data(), command.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, environment.data(), nullptr, &startup, &process) != FALSE,
        "could not start telemetry test child");
    CloseHandle(process.hThread);
    const auto waited = WaitForSingleObject(process.hProcess, 5000);
    DWORD code{99};
    GetExitCodeProcess(process.hProcess, &code);
    if (waited != WAIT_OBJECT_0) { TerminateProcess(process.hProcess, 99); WaitForSingleObject(process.hProcess, 5000); }
    CloseHandle(process.hProcess);
    int count{0};
    bool valid{false};
    server.Poll(process.dwProcessId, [&](std::string_view event, std::uint32_t pid)
    {
        ++count;
        valid = pid == process.dwProcessId && ValidEvent(event, 2)
            && event.find("short lived child") != std::string_view::npos;
    });
    Require(waited == WAIT_OBJECT_0 && code == 0 && count == 1 && valid,
        "short-lived child telemetry lost after process exit");
}
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view{argv[1]} == "--child") return Child();
    try
    {
        TestMessageBudgetAndAuthentication();
        TestEnvironment();
        TestShortLivedProcess();
        std::cout << "module event pipe tests passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
