#include "diagnostics/windows/CrashReporter.h"
#include "diagnostics/windows/CrashProtocol.h"

#include <DbgHelp.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using slicesoft::diagnostics::CrashReporter;
using slicesoft::diagnostics::CrashReporterOptions;
using slicesoft::diagnostics::crash_detail::Handle;

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message + " (win32=" + std::to_string(GetLastError()) + ')');
}

class Child final {
public:
    ~Child() {
        if (process_.Valid() && WaitForSingleObject(process_.Get(), 0) == WAIT_TIMEOUT) {
            TerminateProcess(process_.Get(), ERROR_TIMEOUT);
            WaitForSingleObject(process_.Get(), 1000);
        }
    }
    void Start(const fs::path& child, const wchar_t* mode, const fs::path& helper, const fs::path& output) {
        std::wstring args = L'"' + child.native() + L"\" " + mode + L" \"" + helper.native()
            + L"\" \"" + output.native() + L'"';
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        Require(CreateProcessW(child.c_str(), args.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE, "start controlled child");
        process_.Reset(process.hProcess);
        CloseHandle(process.hThread);
    }
    DWORD Wait(DWORD timeout = 15000) {
        Require(WaitForSingleObject(process_.Get(), timeout) == WAIT_OBJECT_0, "controlled child deadline");
        DWORD code = 0;
        Require(GetExitCodeProcess(process_.Get(), &code) != FALSE, "controlled child exit code");
        return code;
    }
private:
    Handle process_;
};

class Dump final {
public:
    explicit Dump(const fs::path& path) {
        file_.Reset(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        Require(file_.Valid(), "open dump");
        LARGE_INTEGER size{};
        Require(GetFileSizeEx(file_.Get(), &size) && size.QuadPart > 0, "nonempty dump");
        size_ = static_cast<std::uint64_t>(size.QuadPart);
        mapping_.Reset(CreateFileMappingW(file_.Get(), nullptr, PAGE_READONLY, 0, 0, nullptr));
        Require(mapping_.Valid(), "map dump file");
        data_ = MapViewOfFile(mapping_.Get(), FILE_MAP_READ, 0, 0, 0);
        Require(data_ != nullptr, "map dump view");
    }
    ~Dump() { if (data_) UnmapViewOfFile(data_); }
    template<class T> const T* Stream(ULONG type) {
        PMINIDUMP_DIRECTORY directory = nullptr;
        PVOID stream = nullptr;
        ULONG size = 0;
        Require(MiniDumpReadDumpStream(data_, type, &directory, &stream, &size) != FALSE
            && size >= sizeof(T), "required minidump stream");
        return static_cast<const T*>(stream);
    }
    const void* At(std::uint64_t offset, std::uint64_t count) {
        Require(offset <= size_ && count <= size_ - offset, "dump location range");
        return static_cast<const unsigned char*>(data_) + offset;
    }
private:
    Handle file_;
    Handle mapping_;
    void* data_{nullptr};
    std::uint64_t size_{0};
};

void VerifySymbols(Dump& dump, const MINIDUMP_MODULE& module, DWORD64 address, const fs::path& child) {
    struct CodeView { DWORD signature; GUID guid; DWORD age; } codeView{};
    Require(module.CvRecord.DataSize >= sizeof(codeView), "CodeView record exists");
    std::memcpy(&codeView, dump.At(module.CvRecord.Rva, sizeof(codeView)), sizeof(codeView));
    Require(codeView.signature == 0x53445352, "CodeView RSDS signature");
    const HANDLE session = GetCurrentProcess();
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS
        | SYMOPT_NO_PROMPTS | SYMOPT_EXACT_SYMBOLS);
    Require(SymInitializeW(session, child.parent_path().c_str(), FALSE) != FALSE, "initialize symbols");
    struct Cleanup { HANDLE session; ~Cleanup() { SymCleanup(session); } } cleanup{session};
    const auto loaded = SymLoadModuleExW(session, nullptr, child.c_str(), nullptr,
        module.BaseOfImage, module.SizeOfImage, nullptr, 0);
    Require(loaded == module.BaseOfImage, "load matching child image symbols");
    alignas(SYMBOL_INFO) unsigned char bytes[sizeof(SYMBOL_INFO) + 512]{};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(bytes);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 512;
    DWORD64 displacement = 0;
    Require(SymFromAddr(session, address, &displacement, symbol) != FALSE, "resolve crash instruction");
    Require(std::string(symbol->Name).find("ControlledCrashSite") != std::string::npos,
        "crash instruction resolves to ControlledCrashSite");
    IMAGEHLP_MODULE64 information{};
    information.SizeOfStruct = sizeof(information);
    Require(SymGetModuleInfo64(session, module.BaseOfImage, &information) != FALSE, "loaded PDB metadata");
    Require(information.SymType == SymPdb && !information.PdbUnmatched,
        "matching PDB actually loaded");
    Require(information.PdbAge == codeView.age
        && std::memcmp(&information.PdbSig70, &codeView.guid, sizeof(GUID)) == 0,
        "PDB GUID and age match dump CodeView");
}

void VerifyDump(const fs::path& path, const fs::path& child) {
    Dump dump(path);
    const auto* exception = dump.Stream<MINIDUMP_EXCEPTION_STREAM>(ExceptionStream);
    Require(exception->ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION,
        "actual access violation exception stream");
    Require(exception->ThreadContext.DataSize >= sizeof(CONTEXT), "exception context captured");
    const auto* threads = dump.Stream<MINIDUMP_THREAD_LIST>(ThreadListStream);
    Require(threads->NumberOfThreads > 0, "thread list populated");
    bool faultThreadFound = false;
    for (ULONG i = 0; i < threads->NumberOfThreads; ++i)
        faultThreadFound |= threads->Threads[i].ThreadId == exception->ThreadId;
    Require(faultThreadFound, "faulting thread is in thread stream");
    const auto* modules = dump.Stream<MINIDUMP_MODULE_LIST>(ModuleListStream);
    Require(modules->NumberOfModules > 0, "module list populated");
    const auto address = exception->ExceptionRecord.ExceptionAddress;
    const MINIDUMP_MODULE* image = nullptr;
    for (ULONG i = 0; i < modules->NumberOfModules; ++i) {
        const auto& module = modules->Modules[i];
        if (address >= module.BaseOfImage && address - module.BaseOfImage < module.SizeOfImage)
            image = &module;
    }
    Require(image != nullptr, "exception address belongs to captured module");
    VerifySymbols(dump, *image, address, child);
    std::ifstream sidecar(fs::path(path.native() + L".json"), std::ios::binary);
    const std::string json{std::istreambuf_iterator<char>(sidecar), std::istreambuf_iterator<char>()};
    Require(json.find("\"success\":true") != std::string::npos
        && json.find("\"systemError\":0") != std::string::npos, "successful capture sidecar");
}

std::vector<fs::path> Dumps(const fs::path& directory) {
    std::vector<fs::path> result;
    if (fs::exists(directory))
        for (const auto& entry : fs::directory_iterator(directory))
            if (entry.path().extension() == L".dmp") result.push_back(entry.path());
    return result;
}

LONG WINAPI ExistingFilter(EXCEPTION_POINTERS*) { return EXCEPTION_CONTINUE_SEARCH; }

void CheckLifecycle(const fs::path& helper, const fs::path& child, const fs::path& root) {
    CrashReporter reporter;
    CrashReporterOptions options{helper, root / L"lifecycle"};
    auto missing = options;
    missing.helperExecutable = helper.parent_path() / L"absent_crash_helper.exe";
    Require(!reporter.Start(missing).enabled, "missing helper is a visible downgrade");
    auto invalid = options;
    invalid.crashTimeout = std::chrono::milliseconds(0);
    Require(!reporter.Start(invalid).enabled, "unbounded or invalid wait rejected");
    invalid = options;
    invalid.dumpDirectory = L"relative-dumps";
    Require(!reporter.Start(invalid).enabled, "relative output rejected");
    invalid = options;
    invalid.helperExecutable = child;
    invalid.dumpDirectory = root / L"startup-timeout";
    invalid.startupTimeout = std::chrono::milliseconds(50);
    const auto stalled = reporter.Start(invalid);
    Require(!stalled.enabled && stalled.reason == "helper_not_ready", "startup deadline enforced");
    const fs::path blocker = root / L"file-instead-of-directory";
    { std::ofstream out(blocker); out << "fixture"; }
    invalid = options;
    invalid.dumpDirectory = blocker;
    Require(!reporter.Start(invalid).enabled, "invalid destination rejected before filter install");
    const auto saved = SetUnhandledExceptionFilter(&ExistingFilter);
    if (saved) {
        MEMORY_BASIC_INFORMATION memory{};
        wchar_t image[32768]{};
        if (VirtualQuery(reinterpret_cast<const void*>(saved), &memory, sizeof(memory))
            && GetModuleFileNameW(static_cast<HMODULE>(memory.AllocationBase), image, 32768))
            std::wcout << L"Preexisting EXE filter module: " << image << L'\n';
    }
    const auto rejected = reporter.Start(options);
    Require(!rejected.enabled && rejected.reason == "existing_exception_filter", "existing filter retained");
    Require(SetUnhandledExceptionFilter(&ExistingFilter) == &ExistingFilter, "existing filter not replaced");
    options.replaceExistingFilter = true;
    Require(reporter.Start(options).enabled, "explicit EXE ownership can replace existing filter");
    reporter.Stop();
    Require(SetUnhandledExceptionFilter(saved) == &ExistingFilter, "explicit ownership restores previous filter");
    for (int i = 0; i < 2; ++i) {
        const auto started = reporter.Start(options);
        Require(started.enabled && reporter.Enabled(), "start succeeds after previous failure: "
            + started.reason + ':' + std::to_string(started.systemError));
        CrashReporter duplicate;
        Require(!duplicate.Start(options).enabled, "one reporter per process");
        reporter.Stop();
        Require(!reporter.Enabled(), "stop returns to disabled");
    }
    Require(Dumps(options.dumpDirectory).empty(), "normal lifecycle creates no dump");
}

void CheckCaptures(const fs::path& child, const fs::path& helper, const fs::path& root) {
    const auto unicodeOutput = root / fs::path(L"\u65e5\u5fd7_\u5d29\u6e83_\u6d4b\u8bd5");
    Child one;
    Child two;
    one.Start(child, L"--crash", helper, unicodeOutput);
    two.Start(child, L"--crash", helper, unicodeOutput);
    Require(one.Wait() == EXCEPTION_ACCESS_VIOLATION, "first child retains exception exit");
    Require(two.Wait() == EXCEPTION_ACCESS_VIOLATION, "second child retains exception exit");
    const auto captures = Dumps(unicodeOutput);
    Require(captures.size() == 2, "concurrent crashes produce distinct dumps");
    for (const auto& path : captures) VerifyDump(path, child);
    Child normal;
    normal.Start(child, L"--normal", helper, root / L"normal");
    Require(normal.Wait() == 0 && Dumps(root / L"normal").empty(), "normal child exits without dump");
    Child timeout;
    const auto started = std::chrono::steady_clock::now();
    timeout.Start(child, L"--timeout", child, root / L"timeout");
    Require(timeout.Wait(5000) == EXCEPTION_ACCESS_VIOLATION, "helper stall preserves exception exit");
    Require(std::chrono::steady_clock::now() - started < std::chrono::seconds(5), "crash wait bounded");
    Require(Dumps(root / L"timeout").empty(), "stalled helper cannot claim a valid dump");
}
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 4) return 2;
    try {
        const fs::path child = fs::absolute(argv[1]);
        const fs::path helper = fs::absolute(argv[2]);
        const fs::path root = fs::absolute(argv[3])
            / (L"crash_tests_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64()));
        fs::create_directories(root);
        CheckLifecycle(helper, child, root);
        CheckCaptures(child, helper, root);
        std::cout << "Crash reporter lifecycle, Unicode concurrent dumps, exception/thread/module streams, "
            "matching PDB GUID/age, crash symbol and bounded helper timeout: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
