#include "diagnostics/windows/CrashReporter.h"
#include "diagnostics/windows/CrashProtocol.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <sstream>
#include <vector>

namespace slicesoft::diagnostics {
namespace {
using namespace crash_detail;

class Attributes final {
public:
    ~Attributes() { if (initialized_) DeleteProcThreadAttributeList(Get()); }
    bool Initialize(std::array<HANDLE, HandleCount>& handles) {
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        if (!bytes) return false;
        bytes_.resize(bytes);
        initialized_ = InitializeProcThreadAttributeList(Get(), 1, 0, &bytes) != FALSE;
        return initialized_ && UpdateProcThreadAttribute(Get(), 0,
            PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles.data(), sizeof(handles), nullptr, nullptr);
    }
    LPPROC_THREAD_ATTRIBUTE_LIST Get() {
        return reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(bytes_.data());
    }
private:
    std::vector<unsigned char> bytes_;
    bool initialized_{false};
};

bool ValidTimeout(std::chrono::milliseconds timeout) {
    return timeout.count() > 0 && timeout.count() <= 30000;
}

std::filesystem::path DumpPath(const std::filesystem::path& directory) {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    LARGE_INTEGER sequence{};
    QueryPerformanceCounter(&sequence);
    wchar_t name[160]{};
    swprintf_s(name, L"crash_%04u%02u%02uT%02u%02u%02u_%lu_%llu.dmp",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond,
        GetCurrentProcessId(), static_cast<unsigned long long>(sequence.QuadPart));
    return directory / name;
}

std::wstring ExtendedPath(const std::filesystem::path& path) {
    const auto value = path.lexically_normal().make_preferred().native();
    if (value.starts_with(L"\\\\?\\")) return value;
    if (value.starts_with(L"\\\\")) return L"\\\\?\\UNC\\" + value.substr(2);
    return L"\\\\?\\" + value;
}

} // namespace

struct CrashReporter::Impl {
    static Impl* volatile active;
    std::array<Handle, HandleCount> handles;
    MappedState mapping;
    Handle helper;
    DWORD crashTimeout{5000};
    LPTOP_LEVEL_EXCEPTION_FILTER previous{nullptr};
    volatile LONG handling{0};
    bool installed{false};

    ~Impl() { Shutdown(); }

    static LONG WINAPI Filter(EXCEPTION_POINTERS* exception) noexcept {
        auto* self = static_cast<Impl*>(InterlockedCompareExchangePointer(
            reinterpret_cast<PVOID volatile*>(&active), nullptr, nullptr));
        if (!self || !exception) return EXCEPTION_CONTINUE_SEARCH;
        if (InterlockedCompareExchange(&self->handling, 1, 0) != 0) {
            const HANDLE pending[]{self->handles[Complete].Get(), self->helper.Get()};
            WaitForMultipleObjects(2, pending, FALSE, self->crashTimeout);
            return EXCEPTION_EXECUTE_HANDLER;
        }
        auto* shared = self->mapping.Get();
        shared->threadId = GetCurrentThreadId();
        shared->exceptionPointers = reinterpret_cast<std::uintptr_t>(exception);
        shared->exceptionCode = exception->ExceptionRecord->ExceptionCode;
        InterlockedExchange(&shared->state, Capturing);
        SetEvent(self->handles[Request].Get());
        const HANDLE waits[]{self->handles[Complete].Get(), self->helper.Get()};
        const DWORD waited = WaitForMultipleObjects(2, waits, FALSE, self->crashTimeout);
        if (waited != WAIT_OBJECT_0) {
            shared->systemError = waited == WAIT_TIMEOUT ? ERROR_TIMEOUT : ERROR_PROCESS_ABORTED;
            InterlockedExchange(&shared->state, Failed);
        }
        return EXCEPTION_EXECUTE_HANDLER;
    }

    void Shutdown() noexcept {
        if (installed) {
            // Windows has no atomic filter query; the EXE serializes lifecycle changes.
            const auto current = SetUnhandledExceptionFilter(previous);
            if (current != &Filter) SetUnhandledExceptionFilter(current);
            InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(&active), nullptr, this);
            installed = false;
        }
        if (helper.Valid()) {
            if (handles[crash_detail::Stop].Valid()) SetEvent(handles[crash_detail::Stop].Get());
            if (WaitForSingleObject(helper.Get(), 1000) == WAIT_TIMEOUT) {
                TerminateProcess(helper.Get(), ERROR_TIMEOUT);
                WaitForSingleObject(helper.Get(), 1000);
            }
            helper.Reset();
        }
        mapping.Reset();
        for (auto& handle : handles) handle.Reset();
    }

    CrashReporterStartResult Initialize(const CrashReporterOptions& options) {
        if (active) return {false, ERROR_ALREADY_EXISTS, "reporter_already_installed"};
        if (!ValidTimeout(options.startupTimeout) || !ValidTimeout(options.crashTimeout)
            || !options.helperExecutable.is_absolute() || !options.dumpDirectory.is_absolute()
            || options.helperExecutable.native().find(L'\0') != std::wstring::npos
            || options.dumpDirectory.native().find(L'\0') != std::wstring::npos)
            return {false, ERROR_INVALID_PARAMETER, "invalid_options"};
        if (!std::filesystem::is_regular_file(options.helperExecutable))
            return {false, ERROR_FILE_NOT_FOUND, "helper_missing"};
        std::filesystem::create_directories(options.dumpDirectory);
        const auto path = ExtendedPath(DumpPath(options.dumpDirectory));
        if (path.size() >= PathCapacity) return {false, ERROR_FILENAME_EXCED_RANGE, "path_too_long"};
        {
            Handle probe(CreateFileW((path + L".probe").c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr));
            if (!probe.Valid()) return {false, GetLastError(), "dump_directory_not_writable"};
        }
        SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
        handles[Mapping].Reset(CreateFileMappingW(INVALID_HANDLE_VALUE, &security, PAGE_READWRITE,
            0, sizeof(SharedState), nullptr));
        if (!handles[Mapping].Valid()) return {false, GetLastError(), "ipc_mapping_creation_failed"};
        for (std::size_t i = Request; i <= crash_detail::Stop; ++i) {
            handles[i].Reset(CreateEventW(&security, TRUE, FALSE, nullptr));
            if (!handles[i].Valid()) return {false, GetLastError(), "ipc_event_creation_failed"};
        }
        handles[Target].Reset(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE,
            TRUE, GetCurrentProcessId()));
        if (!handles[Target].Valid()) return {false, GetLastError(), "target_handle_creation_failed"};
        if (!mapping.Map(handles[Mapping].Get())) return {false, GetLastError(), "ipc_map_failed"};
        auto& shared = *mapping.Get();
        std::memset(&shared, 0, sizeof(shared));
        shared.magic = ProtocolMagic;
        shared.version = ProtocolVersion;
        shared.size = sizeof(SharedState);
        shared.pointerSize = sizeof(void*);
        shared.processId = GetCurrentProcessId();
        std::copy(path.begin(), path.end(), shared.dumpPath);
        std::array<HANDLE, HandleCount> inherited{};
        std::wostringstream command;
        command << L'"' << options.helperExecutable.native() << L"\" --monitor-v1";
        for (std::size_t i = 0; i < handles.size(); ++i) {
            inherited[i] = handles[i].Get();
            command << L' ' << reinterpret_cast<std::uintptr_t>(inherited[i]);
        }
        Attributes attributes;
        if (!attributes.Initialize(inherited)) return {false, GetLastError(), "ipc_inheritance_failed"};
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.lpAttributeList = attributes.Get();
        PROCESS_INFORMATION process{};
        auto arguments = command.str();
        if (!CreateProcessW(options.helperExecutable.c_str(), arguments.data(), nullptr, nullptr,
                TRUE, CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                &startup.StartupInfo, &process))
            return {false, GetLastError(), "helper_start_failed"};
        helper.Reset(process.hProcess);
        CloseHandle(process.hThread);
        for (auto& handle : handles) SetHandleInformation(handle.Get(), HANDLE_FLAG_INHERIT, 0);
        const HANDLE waits[]{handles[Ready].Get(), helper.Get()};
        const auto waited = WaitForMultipleObjects(2, waits, FALSE,
            static_cast<DWORD>(options.startupTimeout.count()));
        if (waited != WAIT_OBJECT_0 || shared.state != Armed)
            return {false, static_cast<std::uint32_t>(waited == WAIT_TIMEOUT ? ERROR_TIMEOUT : ERROR_PROCESS_ABORTED),
                "helper_not_ready"};
        if (InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(&active), this, nullptr))
            return {false, ERROR_ALREADY_EXISTS, "reporter_already_installed"};
        previous = SetUnhandledExceptionFilter(&Filter);
        if (previous && !options.replaceExistingFilter) {
            SetUnhandledExceptionFilter(previous);
            InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(&active), nullptr);
            return {false, ERROR_ALREADY_EXISTS, "existing_exception_filter"};
        }
        crashTimeout = static_cast<DWORD>(options.crashTimeout.count());
        installed = true;
        return {true, ERROR_SUCCESS, "enabled"};
    }
};

CrashReporter::Impl* volatile CrashReporter::Impl::active = nullptr;

CrashReporter::CrashReporter() = default;
CrashReporter::~CrashReporter() = default;

CrashReporterStartResult CrashReporter::Start(const CrashReporterOptions& options) {
    if (Enabled()) return {false, ERROR_ALREADY_EXISTS, "reporter_already_installed"};
    try {
        auto candidate = std::make_unique<Impl>();
        auto result = candidate->Initialize(options);
        if (result.enabled) impl_ = std::move(candidate);
        return result;
    } catch (const std::filesystem::filesystem_error& error) {
        return {false, static_cast<std::uint32_t>(error.code().value()), "filesystem_error"};
    } catch (const std::exception&) {
        return {false, ERROR_NOT_ENOUGH_MEMORY, "reporter_initialization_failed"};
    }
}

void CrashReporter::Stop() noexcept { impl_.reset(); }
bool CrashReporter::Enabled() const noexcept { return impl_ && impl_->installed; }

} // namespace slicesoft::diagnostics
