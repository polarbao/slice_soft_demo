#include "SessionRetention.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace slicesoft::diagnostics
{
namespace
{
constexpr wchar_t OwnerName[] = L"session.owner";
constexpr wchar_t LockName[] = L"session.lock";

class Handle
{
public:
    HANDLE value{INVALID_HANDLE_VALUE};
    Handle() = default;
    explicit Handle(HANDLE handle) : value{handle} {}
    Handle(Handle&& other) noexcept : value{std::exchange(other.value, INVALID_HANDLE_VALUE)} {}
    Handle& operator=(Handle&& other) noexcept
    { if (this != &other) { Reset(); value = std::exchange(other.value, INVALID_HANDLE_VALUE); } return *this; }
    ~Handle() { Reset(); }
    void Reset() noexcept { if (Valid()) CloseHandle(value); value = INVALID_HANDLE_VALUE; }
    bool Valid() const noexcept { return value != INVALID_HANDLE_VALUE && value != nullptr; }
};

bool PlainPath(const std::filesystem::path& path)
{
    if (!path.is_absolute()) return false;
    std::filesystem::path partial;
    for (const auto& segment : path.lexically_normal())
    {
        partial /= segment;
        const DWORD attributes = GetFileAttributesW(partial.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    }
    return true;
}

Handle DirectoryGuard(const std::filesystem::path& path, bool deleting = false)
{
    Handle directory{CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES | (deleting ? DELETE : 0),
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr)};
    BY_HANDLE_FILE_INFORMATION information{};
    if (!directory.Valid() || !GetFileInformationByHandle(directory.value, &information)
        || !(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        || (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) directory.Reset();
    return directory;
}

std::string OwnerText(const std::filesystem::path& directory)
{
    return "SliceSoft.Diagnostics.Session.v1\n" + directory.filename().string() + "\n";
}

bool MatchesSessionName(const std::filesystem::path& directory)
{
    static const std::wregex pattern{LR"(ssdiag_[0-9]+_[0-9]+_[0-9]+_[a-z][a-z0-9_]{0,31})"};
    return std::regex_match(directory.filename().native(), pattern);
}

bool HasOwner(const std::filesystem::path& directory)
{
    const auto path = directory / OwnerName;
    Handle marker{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr)};
    BY_HANDLE_FILE_INFORMATION information{};
    if (!marker.Valid() || !GetFileInformationByHandle(marker.value, &information)
        || information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)
        || information.nNumberOfLinks != 1 || information.nFileSizeHigh || information.nFileSizeLow > 512) return false;
    char bytes[512]{};
    DWORD read{0};
    return ReadFile(marker.value, bytes, sizeof(bytes), &read, nullptr)
        && std::string_view(bytes, read) == OwnerText(directory);
}

bool IsLog(const std::wstring& name)
{
    static const std::wregex pattern{LR"((app|slicer_module|rip)\.log(\.[1-9][0-9]{0,3})?)"};
    return std::regex_match(name, pattern);
}
bool IsDump(const std::wstring& name)
{
    static const std::wregex pattern{LR"(crash_[0-9]{8}T[0-9]{6}_[0-9]+_[0-9]+\.dmp(\.json|\.partial)?)"};
    return std::regex_match(name, pattern);
}

struct FileEntry
{
    std::filesystem::path path;
    std::uint64_t identity{0};
    std::uintmax_t size{0};
};
struct Candidate
{
    std::filesystem::path path;
    std::vector<FileEntry> files;
    std::uintmax_t logs{0}, dumps{0};
    std::size_t dumpCount{0};
    std::filesystem::file_time_type time;
    bool hasDumps{false};
};

bool InspectFile(const std::filesystem::path& path, FileEntry& entry)
{
    Handle file{CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr)};
    BY_HANDLE_FILE_INFORMATION information{};
    if (!file.Valid() || !GetFileInformationByHandle(file.value, &information)
        || information.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)
        || information.nNumberOfLinks != 1) return false;
    entry = {path, (static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) | information.nFileIndexLow,
        (static_cast<std::uintmax_t>(information.nFileSizeHigh) << 32) | information.nFileSizeLow};
    return true;
}

bool Scan(Candidate& candidate)
{
    for (const auto& entry : std::filesystem::directory_iterator(candidate.path))
    {
        const auto name = entry.path().filename().native();
        if (name == LockName) continue; // The lease is separately opened exclusively.
        if (name == L"dumps")
        {
            auto guard = DirectoryGuard(entry.path());
            if (!guard.Valid()) return false;
            candidate.hasDumps = true;
            for (const auto& dump : std::filesystem::directory_iterator(entry.path()))
            {
                FileEntry file;
                if (!IsDump(dump.path().filename().native()) || !InspectFile(dump.path(), file)) return false;
                candidate.dumps += file.size;
                if (dump.path().extension() != L".json") ++candidate.dumpCount;
                candidate.files.push_back(std::move(file));
                if (candidate.files.size() > 1024) return false;
            }
        }
        else
        {
            FileEntry file;
            if ((name != OwnerName && !IsLog(name)) || !InspectFile(entry.path(), file)) return false;
            if (name != OwnerName) candidate.logs += file.size;
            candidate.files.push_back(std::move(file));
        }
        if (candidate.files.size() > 1024) return false;
    }
    return true;
}

Handle OpenLease(const std::filesystem::path& directory)
{
    Handle lock{CreateFileW((directory / LockName).c_str(), GENERIC_READ | DELETE,
        0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr)};
    BY_HANDLE_FILE_INFORMATION information{};
    if (!lock.Valid() || !GetFileInformationByHandle(lock.value, &information)
        || information.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)
        || information.nNumberOfLinks != 1 || information.nFileSizeLow || information.nFileSizeHigh) lock.Reset();
    return lock;
}

bool MarkDelete(HANDLE handle)
{
    FILE_DISPOSITION_INFO disposition{TRUE};
    return SetFileInformationByHandle(handle, FileDispositionInfo, &disposition, sizeof(disposition)) != FALSE;
}

bool DeleteSession(const Candidate& candidate)
{
    auto directory = DirectoryGuard(candidate.path, true);
    auto lease = OpenLease(candidate.path);
    if (!directory.Valid() || !lease.Valid() || !HasOwner(candidate.path)) return false;
    Candidate current;
    current.path = candidate.path;
    if (!Scan(current)) return false;
    std::stable_sort(current.files.begin(), current.files.end(), [](const auto& left, const auto& right)
        { return left.path.filename() != OwnerName && right.path.filename() == OwnerName; });
    auto dumps = current.hasDumps ? DirectoryGuard(current.path / L"dumps", true) : Handle{};
    if (current.hasDumps && !dumps.Valid()) return false;
    std::vector<Handle> files;
    for (const auto& entry : current.files)
    {
        Handle file{CreateFileW(entry.path.c_str(), FILE_READ_ATTRIBUTES | DELETE,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr)};
        BY_HANDLE_FILE_INFORMATION information{};
        if (!file.Valid() || !GetFileInformationByHandle(file.value, &information)
            || information.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)
            || information.nNumberOfLinks != 1
            || ((static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) | information.nFileIndexLow) != entry.identity)
            return false;
        files.push_back(std::move(file));
    }
    for (auto& file : files) { if (!MarkDelete(file.value)) return false; file.Reset(); }
    if (dumps.Valid()) { if (!MarkDelete(dumps.value)) return false; dumps.Reset(); }
    if (!MarkDelete(lease.value)) return false;
    lease.Reset();
    return MarkDelete(directory.value);
}

bool Exceeds(const SessionRetentionResult& result, const SessionRetentionPolicy& policy)
{
    return result.retainedLogBytes > policy.maximumLogBytes
        || result.retainedDumpBytes > policy.maximumDumpBytes
        || result.retainedDumpCount > policy.maximumDumpCount;
}
}

class SessionLease::Implementation
{
public:
    std::filesystem::path directory;
    Handle lock;
};
SessionLease::SessionLease(std::unique_ptr<Implementation> impl) : m_impl{std::move(impl)} {}
SessionLease::~SessionLease() = default;
const std::filesystem::path& SessionLease::Directory() const noexcept { return m_impl->directory; }

std::unique_ptr<SessionLease> SessionLease::Create(const std::filesystem::path& root,
    const std::string& role, std::string* error) noexcept
{
    try
    {
        if (!PlainPath(root) || !std::regex_match(role, std::regex{"[a-z][a-z0-9_]{0,31}"}))
            throw std::runtime_error("diagnostic session root or role is invalid");
        std::filesystem::create_directories(root);
        auto rootGuard = DirectoryGuard(root);
        if (!rootGuard.Valid() || !PlainPath(root)) throw std::runtime_error("diagnostic root is not a plain directory");
        auto impl = std::make_unique<Implementation>();
        static std::atomic_uint64_t counter{0};
        const auto tick = std::chrono::system_clock::now().time_since_epoch().count();
        impl->directory = root / ("ssdiag_" + std::to_string(tick) + "_" + std::to_string(GetCurrentProcessId())
            + "_" + std::to_string(++counter) + "_" + role);
        if (!std::filesystem::create_directory(impl->directory)) throw std::runtime_error("diagnostic session already exists");
        impl->lock.value = CreateFileW((impl->directory / LockName).c_str(), GENERIC_READ | GENERIC_WRITE,
            0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!impl->lock.Valid()) throw std::runtime_error("cannot acquire diagnostic session lease");
        Handle marker{CreateFileW((impl->directory / OwnerName).c_str(), GENERIC_WRITE, 0,
            nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
        const auto owner = OwnerText(impl->directory);
        DWORD written{0};
        if (!marker.Valid() || !WriteFile(marker.value, owner.data(), static_cast<DWORD>(owner.size()), &written, nullptr)
            || written != owner.size()) throw std::runtime_error("cannot write diagnostic session identity");
        if (error) error->clear();
        return std::unique_ptr<SessionLease>{new SessionLease{std::move(impl)}};
    }
    catch (const std::exception& failure) { try { if (error) *error = failure.what(); } catch (...) {} return {}; }
    catch (...) { return {}; }
}

SessionRetentionResult ApplySessionRetention(const std::filesystem::path& root,
    const std::filesystem::path& currentDirectory, const SessionRetentionPolicy& policy) noexcept
{
    SessionRetentionResult result;
    try
    {
        if (!PlainPath(root)) { ++result.errors; return result; }
        auto rootGuard = DirectoryGuard(root);
        if (!rootGuard.Valid()) { ++result.errors; return result; }
        std::vector<Candidate> retired;
        for (const auto& entry : std::filesystem::directory_iterator(root))
        {
            if (!MatchesSessionName(entry.path())) { ++result.skippedUnrecognized; continue; }
            auto guard = DirectoryGuard(entry.path());
            Candidate candidate;
            candidate.path = entry.path();
            if (!guard.Valid() || !HasOwner(entry.path()) || !Scan(candidate))
            { ++result.skippedUnrecognized; continue; }
            result.retainedLogBytes += candidate.logs;
            result.retainedDumpBytes += candidate.dumps;
            result.retainedDumpCount += candidate.dumpCount;
            auto lease = OpenLease(entry.path());
            if (entry.path() == currentDirectory || !lease.Valid()) { ++result.skippedActive; continue; }
            candidate.time = std::filesystem::last_write_time(entry.path() / OwnerName);
            retired.push_back(std::move(candidate));
            if (retired.size() >= 1024) { ++result.errors; break; }
        }
        std::sort(retired.begin(), retired.end(), [](const auto& left, const auto& right) { return left.time < right.time; });
        for (const auto& candidate : retired)
        {
            if (!Exceeds(result, policy)) break;
            if (DeleteSession(candidate))
            {
                ++result.removedSessions;
                result.retainedLogBytes -= candidate.logs;
                result.retainedDumpBytes -= candidate.dumps;
                result.retainedDumpCount -= candidate.dumpCount;
            }
            else
            {
                ++result.errors;
                // A failed deletion may have removed some files. Keep conservative totals
                // and rescan next time instead of evicting more history against stale bytes.
                break;
            }
        }
        result.budgetExceeded = Exceeds(result, policy);
    }
    catch (...) { ++result.errors; result.budgetExceeded = Exceeds(result, policy); }
    return result;
}
}
