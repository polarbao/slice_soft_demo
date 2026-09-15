#include "diagnostics/windows/CrashCapture.h"
#include "diagnostics/windows/CrashProtocol.h"

#include <DbgHelp.h>

#include <array>
#include <string>

namespace slicesoft::diagnostics {
namespace {
using namespace crash_detail;

void WriteMetadata(const SharedState& shared, bool success, DWORD error, std::uint64_t bytes) {
    const std::wstring path = std::wstring(shared.dumpPath) + L".json";
    Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.Valid()) return;
    const std::string json = "{\"schema\":\"diagnostics.crash.v1\",\"processId\":"
        + std::to_string(shared.processId) + ",\"threadId\":" + std::to_string(shared.threadId)
        + ",\"exceptionCode\":" + std::to_string(shared.exceptionCode)
        + ",\"success\":" + (success ? "true" : "false")
        + ",\"systemError\":" + std::to_string(error)
        + ",\"bytes\":" + std::to_string(bytes) + "}\n";
    DWORD written = 0;
    WriteFile(file.Get(), json.data(), static_cast<DWORD>(json.size()), &written, nullptr);
    FlushFileBuffers(file.Get());
}

void Capture(SharedState& shared, HANDLE target) {
    DWORD error = ERROR_SUCCESS;
    bool success = false;
    bool created = false;
    std::uint64_t bytes = 0;
    {
        Handle output(CreateFileW(shared.dumpPath, GENERIC_WRITE, FILE_SHARE_READ,
            nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!output.Valid()) error = GetLastError();
        else {
            created = true;
            MINIDUMP_EXCEPTION_INFORMATION exception{};
            exception.ThreadId = shared.threadId;
            exception.ExceptionPointers = reinterpret_cast<EXCEPTION_POINTERS*>(
                static_cast<std::uintptr_t>(shared.exceptionPointers));
            exception.ClientPointers = TRUE;
            success = MiniDumpWriteDump(target, shared.processId, output.Get(),
                static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo),
                &exception, nullptr, nullptr) != FALSE;
            if (!success) error = GetLastError();
            else if (!FlushFileBuffers(output.Get())) { success = false; error = GetLastError(); }
            LARGE_INTEGER size{};
            if (GetFileSizeEx(output.Get(), &size)) bytes = static_cast<std::uint64_t>(size.QuadPart);
        }
    }
    if (!success && created) {
        // An incomplete dump is explicitly named so it is not mistaken for valid evidence.
        const std::wstring partial = std::wstring(shared.dumpPath) + L".partial";
        MoveFileW(shared.dumpPath, partial.c_str());
    }
    WriteMetadata(shared, success, error, bytes);
    shared.systemError = error;
    InterlockedExchange(&shared.state, success ? Succeeded : Failed);
}

} // namespace

int RunCrashReporter(int argc, wchar_t** argv) {
    std::array<HANDLE, HandleCount> raw{};
    if (!ParseHandles(argc, argv, raw)) return 2;
    std::array<Handle, HandleCount> handles;
    for (std::size_t i = 0; i < raw.size(); ++i) handles[i].Reset(raw[i]);
    MappedState mapping;
    if (!mapping.Map(handles[Mapping].Get())) return 3;
    auto& shared = *mapping.Get();
    if (!ValidateState(shared, handles[Target].Get())) return 4;
    InterlockedExchange(&shared.state, Armed);
    if (!SetEvent(handles[Ready].Get())) return 5;
    const HANDLE waits[]{handles[Request].Get(), handles[Stop].Get(), handles[Target].Get()};
    const DWORD waited = WaitForMultipleObjects(3, waits, FALSE, INFINITE);
    if (waited != WAIT_OBJECT_0) return waited == WAIT_FAILED ? 6 : 0;
    if (shared.state != Capturing || !shared.threadId || !shared.exceptionPointers) return 7;
    Capture(shared, handles[Target].Get());
    SetEvent(handles[Complete].Get());
    return shared.state == Succeeded ? 0 : 8;
}

} // namespace slicesoft::diagnostics
