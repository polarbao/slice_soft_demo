#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace slicesoft::diagnostics {

struct CrashReporterOptions {
    std::filesystem::path helperExecutable;
    std::filesystem::path dumpDirectory;
    std::chrono::milliseconds startupTimeout{3000};
    std::chrono::milliseconds crashTimeout{5000};
    bool replaceExistingFilter{false};
};

struct CrashReporterStartResult {
    bool enabled{false};
    std::uint32_t systemError{0};
    std::string reason;
};

// An EXE owns this service. Start/Stop run during quiescent startup/shutdown,
// serialized with every other owner of the process exception filter.
class CrashReporter final {
public:
    CrashReporter();
    ~CrashReporter();
    CrashReporter(const CrashReporter&) = delete;
    CrashReporter& operator=(const CrashReporter&) = delete;

    CrashReporterStartResult Start(const CrashReporterOptions& options);
    void Stop() noexcept;
    bool Enabled() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace slicesoft::diagnostics
