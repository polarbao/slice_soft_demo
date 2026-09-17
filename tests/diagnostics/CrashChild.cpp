#include "diagnostics/windows/CrashReporter.h"
#include "diagnostics/windows/CrashProtocol.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <string>

// The dedicated test executable is the only process deliberately crashed.
extern "C" __declspec(noinline) void ControlledCrashSite() {
    volatile std::uintptr_t address = 1;
    *reinterpret_cast<volatile unsigned int*>(address) = 0x1234;
}

namespace {
int StallHelper(int argc, wchar_t** argv) {
    using namespace slicesoft::diagnostics::crash_detail;
    std::array<HANDLE, HandleCount> handles{};
    if (!ParseHandles(argc, argv, handles)) return 10;
    MappedState mapping;
    if (!mapping.Map(handles[Mapping])) return 11;
    auto& state = *mapping.Get();
    if (!ValidateState(state, handles[Target])) return 12;
    if (std::wstring(state.dumpPath).find(L"startup-timeout") == std::wstring::npos) {
        InterlockedExchange(&state.state, Armed);
        SetEvent(handles[Ready]);
    }
    const HANDLE waits[]{handles[Stop], handles[Target]};
    WaitForMultipleObjects(2, waits, FALSE, 10000);
    return 0;
}
}

int wmain(int argc, wchar_t** argv) {
    if (argc == 8 && std::wstring(argv[1]) == L"--monitor-v1") return StallHelper(argc, argv);
    if (argc != 4) return 2;
    const std::wstring mode = argv[1];
    if (mode != L"--crash" && mode != L"--timeout" && mode != L"--normal") return 3;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(nullptr);
    slicesoft::diagnostics::CrashReporterOptions options;
    options.helperExecutable = std::filesystem::path(argv[2]);
    options.dumpDirectory = std::filesystem::path(argv[3]);
    if (mode == L"--timeout") options.crashTimeout = std::chrono::milliseconds(200);
    slicesoft::diagnostics::CrashReporter reporter;
    const auto started = reporter.Start(options);
    if (!started.enabled) {
        std::cerr << started.reason << ':' << started.systemError << '\n';
        return 4;
    }
    if (mode == L"--normal") { reporter.Stop(); return 0; }
    ControlledCrashSite();
    return 5;
}
