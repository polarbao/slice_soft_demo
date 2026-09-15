#include "diagnostics/host/ModuleLogBinding.h"

#include <Windows.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace slicesoft::diagnostics;
namespace fs = std::filesystem;

void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

class Api final {
public:
    explicit Api(const fs::path& path) : library(LoadLibraryW(path.c_str())) {
        Require(library != nullptr, "load real slicer_module");
    }
    ~Api() { if (library) FreeLibrary(library); }
    template<class T> T Get(const char* name) {
        const auto value = GetProcAddress(library, name);
        Require(value != nullptr, "required SPI export");
        return reinterpret_cast<T>(value);
    }
    HMODULE library;
};

class Module final {
public:
    explicit Module(Api& api) : destroy_(api.Get<decltype(&pm_destroy)>("pm_destroy")),
        value(api.Get<decltype(&pm_create)>("pm_create")(nullptr)) {
        Require(value != nullptr, "create module instance");
    }
    ~Module() { destroy_(value); }
private:
    decltype(&pm_destroy) destroy_;
public:
    pm_module_t* value;
};

std::string LastError(Api& api) {
    const auto read = api.Get<decltype(&pm_last_error)>("pm_last_error");
    int required = 0;
    Require(read(nullptr, 0, &required) == PM_ERR_BUFFER_SMALL, "last_error size");
    std::string text(static_cast<std::size_t>(required) + 1, '\0');
    Require(read(text.data(), static_cast<int>(text.size()), nullptr) >= 0, "last_error contents");
    text.resize(static_cast<std::size_t>(required));
    return text;
}

void Exercise(Api& api, pm_module_t* module) {
    const auto selfTest = api.Get<decltype(&pm_self_test)>("pm_self_test");
    int required = 0;
    Require(selfTest(module, nullptr, 0, &required) == PM_ERR_BUFFER_SMALL, "legal module self-test size");
    std::vector<char> text(static_cast<std::size_t>(required) + 1);
    Require(selfTest(module, text.data(), static_cast<int>(text.size()), nullptr) >= 0, "legal module self-test");
    Require(api.Get<decltype(&pm_submit)>("pm_submit")(module, nullptr) == nullptr,
        "module preserves invalid-request failure");
}

void WaitWritten(const std::shared_ptr<LogSession>& session, std::uint64_t count) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (session->Status().written < count && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Require(session->Status().written >= count, "real DLL callback reaches host sink");
}

void CheckEvents(const std::vector<nlohmann::json>& events) {
    bool app = false;
    bool module = false;
    for (const auto& event : events) {
        Require(event.at("schema") == "diagnostics.event.v1", "event schema preserved");
        if (event.at("domain") == "slicer.app") app = true;
        if (event.at("domain") == "slicer.module") {
            module = true;
            Require(event.at("sourcePid") == GetCurrentProcessId(), "DLL source PID preserved");
            Require(!event.at("moduleInstanceId").get<std::string>().empty(), "module identity preserved");
        }
    }
    Require(app && module, "both software and DLL domains reach one host service");
}

void BoundSession(Api& api, pm_module_t* module, const std::shared_ptr<LogSession>& session) {
    ModuleLogBinding binding(ResolveModuleLogging(api.library), module, session);
    Require(binding.Attached(), "host adapter binds optional C exports");
    LogEvent software;
    software.action = "host_request";
    software.message = "host business log";
    session->Write(LogChannel::Application, software);
    Exercise(api, module);
    const auto error = LastError(api);
    WaitWritten(session, 2);
    Require(binding.Clear(5000) == SLICER_LOG_OK, "unbind callback before unload");
    Require(LastError(api) == error, "adapter clear preserves business error");
    session->Close();
}

void CheckAdapters(Api& api, pm_module_t* module, const fs::path& root) {
    const auto globalLogger = spdlog::default_logger();
    const auto globalLevel = globalLogger->level();
    LogSessionOptions fileOptions;
    fileOptions.directory = root / L"file-host";
    std::string error;
    auto fileSession = LogSession::Create(fileOptions, &error);
    Require(fileSession != nullptr, "create private spdlog host");
    BoundSession(api, module, fileSession);
    std::vector<nlohmann::json> fileEvents;
    for (const auto* filename : {"app.log", "slicer_module.log"}) {
        std::ifstream input(fileOptions.directory / filename);
        std::string line;
        while (std::getline(input, line)) if (!line.empty()) fileEvents.push_back(nlohmann::json::parse(line));
    }
    CheckEvents(fileEvents);
    std::mutex mutex;
    std::vector<nlohmann::json> printHostEvents;
    LogSessionOptions printOptions;
    printOptions.directory = root / L"must-not-be-created";
    printOptions.sink = [&](LogChannel, int level, std::string_view json) {
        auto event = nlohmann::json::parse(json);
        Require(event.at("level") == level, "PrintApp adapter severity preserved");
        std::lock_guard lock(mutex);
        printHostEvents.push_back(std::move(event));
    };
    auto printSession = LogSession::Create(printOptions, &error);
    Require(printSession != nullptr, "create simulated PrintApp sink");
    BoundSession(api, module, printSession);
    CheckEvents(printHostEvents);
    Require(!fs::exists(printOptions.directory), "injected PrintApp sink does not double-write files");
    auto oldSession = LogSession::Create(printOptions, &error);
    Require(oldSession != nullptr, "create old-host compatibility session");
    {
        ModuleLogBinding oldBinding({}, module, oldSession);
        Require(!oldBinding.Attached(), "missing optional extension is a supported downgrade");
        Exercise(api, module);
        const auto businessError = LastError(api);
        Require(oldBinding.Clear(0) == SLICER_LOG_OK && LastError(api) == businessError,
            "old API fallback retains SPI behavior");
    }
    oldSession->Close();
    Require(spdlog::default_logger() == globalLogger && globalLogger->level() == globalLevel,
        "host adapter leaves process default logger unchanged");
}
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    try {
        const fs::path root = fs::absolute(argv[2]) / (L"host_adapter_" + std::to_wstring(GetCurrentProcessId())
            + L"_" + std::to_wstring(GetTickCount64()));
        Api api(fs::absolute(argv[1]));
        Module module(api);
        CheckAdapters(api, module.value, root);
        std::cout << "Real DLL: standalone file sink, simulated PrintApp sink, old API downgrade and logger ownership PASS\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
