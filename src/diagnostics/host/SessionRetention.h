#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace slicesoft::diagnostics
{
struct SessionRetentionPolicy
{
    std::uintmax_t maximumLogBytes{256ULL * 1024 * 1024};
    std::uintmax_t maximumDumpBytes{512ULL * 1024 * 1024};
    std::size_t maximumDumpCount{5};
};
struct SessionRetentionResult
{
    std::size_t removedSessions{0}, skippedActive{0}, skippedUnrecognized{0}, errors{0};
    std::uintmax_t retainedLogBytes{0}, retainedDumpBytes{0};
    std::size_t retainedDumpCount{0};
    bool budgetExceeded{false};
};

// EXE-owned lease. Keep alive until file sinks and the crash reporter have closed.
class SessionLease final
{
public:
    static std::unique_ptr<SessionLease> Create(const std::filesystem::path& root,
        const std::string& role, std::string* error) noexcept;
    ~SessionLease();
    SessionLease(const SessionLease&) = delete;
    SessionLease& operator=(const SessionLease&) = delete;
    const std::filesystem::path& Directory() const noexcept;
private:
    class Implementation;
    explicit SessionLease(std::unique_ptr<Implementation> impl);
    std::unique_ptr<Implementation> m_impl;
};

// Best-effort whole-session eviction. Unknown/reparse trees are never traversed.
SessionRetentionResult ApplySessionRetention(const std::filesystem::path& root,
    const std::filesystem::path& currentDirectory,
    const SessionRetentionPolicy& policy = {}) noexcept;
}
