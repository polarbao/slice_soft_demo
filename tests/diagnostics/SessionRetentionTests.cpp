#include "diagnostics/host/SessionRetention.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winioctl.h>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace slicesoft::diagnostics;
void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
void Write(const std::filesystem::path& path, std::string_view bytes)
{
    std::ofstream file(path, std::ios::binary);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    Require(file.good(), "fixture write failed");
}
std::unique_ptr<SessionLease> Lease(const std::filesystem::path& root)
{
    std::string error;
    auto result = SessionLease::Create(root, "host", &error);
    if (!result) throw std::runtime_error(error);
    return result;
}

void TestOldestAndActive(const std::filesystem::path& root)
{
    auto active = Lease(root);
    auto old = Lease(root);
    auto newer = Lease(root);
    const auto oldPath = old->Directory();
    const auto newPath = newer->Directory();
    for (const auto& path : {active->Directory(), oldPath, newPath}) Write(path / "app.log", std::string(20, 'x'));
    std::filesystem::last_write_time(oldPath / "session.owner",
        std::filesystem::file_time_type::clock::now() - std::chrono::hours{2});
    std::filesystem::last_write_time(newPath / "session.owner",
        std::filesystem::file_time_type::clock::now() - std::chrono::hours{1});
    old.reset();
    newer.reset();
    SessionRetentionPolicy policy;
    policy.maximumLogBytes = 45;
    auto result = ApplySessionRetention(root, active->Directory(), policy);
    Require(result.removedSessions == 1 && !std::filesystem::exists(oldPath)
        && std::filesystem::exists(newPath) && result.retainedLogBytes == 40,
        "oldest history was not evicted first");
    policy.maximumLogBytes = 0;
    result = ApplySessionRetention(root, {}, policy);
    Require(result.removedSessions == 1 && result.skippedActive == 1 && result.budgetExceeded
        && std::filesystem::exists(active->Directory() / "app.log"), "active lease did not protect current log");
}

void TestUnknownIdentity(const std::filesystem::path& root)
{
    auto unknown = Lease(root);
    const auto unknownPath = unknown->Directory();
    Write(unknownPath / "app.log", "owned log");
    Write(unknownPath / "user-file.txt", "must survive");
    unknown.reset();
    const auto forged = root / "ssdiag_1_2_3_host";
    std::filesystem::create_directories(forged);
    Write(forged / "session.owner", "wrong schema or directory identity\n");
    Write(forged / "session.lock", "");
    Write(forged / "app.log", "must survive");
    SessionRetentionPolicy policy;
    policy.maximumLogBytes = 0;
    const auto result = ApplySessionRetention(root, {}, policy);
    Require(result.removedSessions == 0 && result.skippedUnrecognized >= 2
        && std::filesystem::exists(unknownPath / "app.log")
        && std::filesystem::exists(unknownPath / "user-file.txt")
        && std::filesystem::exists(forged / "app.log"), "unknown session data was removed");
}

void TestDumpBudget(const std::filesystem::path& root)
{
    auto old = Lease(root);
    auto newer = Lease(root);
    const auto oldPath = old->Directory();
    const auto newPath = newer->Directory();
    for (const auto& path : {oldPath, newPath})
    {
        std::filesystem::create_directory(path / "dumps");
        for (int i = 0; i < 3; ++i)
        {
            const auto name = "crash_20260914T010203_1_" + std::to_string(i) + ".dmp";
            Write(path / "dumps" / (name + (i == 1 ? ".partial" : "")), "dump fixture");
            Write(path / "dumps" / (name + ".json"), "{}");
        }
    }
    std::filesystem::last_write_time(oldPath / "session.owner",
        std::filesystem::file_time_type::clock::now() - std::chrono::hours{1});
    old.reset();
    newer.reset();
    const auto result = ApplySessionRetention(root, {});
    Require(result.removedSessions == 1 && result.retainedDumpCount == 3
        && !result.budgetExceeded && !std::filesystem::exists(oldPath) && std::filesystem::exists(newPath),
        "dump count did not evict oldest complete session");
}

void TestPartialDumpAndLogBudget(const std::filesystem::path& root)
{
    auto owned = Lease(root);
    const auto session = owned->Directory();
    std::filesystem::create_directory(session / "dumps");
    Write(session / "app.log", std::string(20, 'x'));
    Write(session / "dumps" / "crash_20260914T010203_1_1.dmp.partial", "partial");
    Write(session / "dumps" / "crash_20260914T010203_1_1.dmp.json", "{}");
    owned.reset();
    auto result = ApplySessionRetention(root, {});
    Require(result.errors == 0 && result.skippedUnrecognized == 0 && result.retainedDumpCount == 1
        && result.retainedDumpBytes == 9 && result.retainedLogBytes == 20,
        "failed capture files were not included in retention accounting");
    SessionRetentionPolicy policy;
    policy.maximumLogBytes = 0;
    result = ApplySessionRetention(root, {}, policy);
    Require(result.removedSessions == 1 && !result.budgetExceeded && result.errors == 0
        && result.retainedDumpCount == 0 && result.retainedDumpBytes == 0
        && result.retainedLogBytes == 0 && !std::filesystem::exists(session),
        "partial dump prevented removal of a retired session over the log budget");
}

void TestDeletionFailureStopsSweep(const std::filesystem::path& root)
{
    auto old = Lease(root);
    auto newer = Lease(root);
    const auto oldPath = old->Directory();
    const auto newPath = newer->Directory();
    for (const auto& path : {oldPath, newPath}) Write(path / "app.log", std::string(20, 'x'));
    std::filesystem::last_write_time(oldPath / "session.owner",
        std::filesystem::file_time_type::clock::now() - std::chrono::hours{2});
    std::filesystem::last_write_time(newPath / "session.owner",
        std::filesystem::file_time_type::clock::now() - std::chrono::hours{1});
    old.reset();
    newer.reset();
    const auto reader = CreateFileW((oldPath / "app.log").c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(reader != INVALID_HANDLE_VALUE, "could not hold a log open for deletion failure fixture");
    SessionRetentionPolicy policy;
    policy.maximumLogBytes = 0;
    const auto result = ApplySessionRetention(root, {}, policy);
    CloseHandle(reader);
    Require(result.errors == 1 && result.removedSessions == 0 && result.budgetExceeded
        && result.retainedLogBytes == 40 && std::filesystem::exists(oldPath / "app.log")
        && std::filesystem::exists(newPath / "app.log"),
        "cleanup continued to evict history after a deletion failure");
}

bool CreateJunction(const std::filesystem::path& link, const std::filesystem::path& target)
{
    std::filesystem::create_directory(link);
    const auto handle = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    const std::wstring substitute = L"\\??\\" + target.native();
    const auto printable = target.native();
    struct Header { DWORD tag; WORD length, reserved, substituteOffset, substituteLength, printOffset, printLength; };
    Header header{IO_REPARSE_TAG_MOUNT_POINT,
        static_cast<WORD>(8 + (substitute.size() + printable.size() + 2) * sizeof(wchar_t)), 0, 0,
        static_cast<WORD>(substitute.size() * sizeof(wchar_t)),
        static_cast<WORD>((substitute.size() + 1) * sizeof(wchar_t)),
        static_cast<WORD>(printable.size() * sizeof(wchar_t))};
    std::vector<unsigned char> bytes(8 + header.length, 0);
    std::memcpy(bytes.data(), &header, sizeof(header));
    std::memcpy(bytes.data() + sizeof(header), substitute.c_str(), (substitute.size() + 1) * sizeof(wchar_t));
    std::memcpy(bytes.data() + sizeof(header) + header.printOffset, printable.c_str(), (printable.size() + 1) * sizeof(wchar_t));
    DWORD returned{0};
    const bool result = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, bytes.data(), static_cast<DWORD>(bytes.size()),
        nullptr, 0, &returned, nullptr) != FALSE;
    CloseHandle(handle);
    return result;
}

void TestReparseProtection(const std::filesystem::path& root)
{
    auto owned = Lease(root / "sessions");
    const auto session = owned->Directory();
    const auto target = root / "outside";
    std::filesystem::create_directories(target);
    Write(target / "crash_20260914T010203_1_1.dmp", "must survive junction scan");
    Require(CreateJunction(session / "dumps", target), "temporary junction creation failed");
    owned.reset();
    SessionRetentionPolicy policy;
    policy.maximumDumpBytes = 0;
    const auto result = ApplySessionRetention(root / "sessions", {}, policy);
    Require(result.removedSessions == 0 && result.skippedUnrecognized == 1
        && std::filesystem::exists(target / "crash_20260914T010203_1_1.dmp"), "retention traversed a reparse subtree");
    std::string error;
    Require(!SessionLease::Create(session / "dumps", "host", &error), "lease followed a reparse root");
}
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc == 2, "expected retention fixture root");
        const auto root = std::filesystem::absolute(argv[1]) / (L"retention_\u6d4b\u8bd5_"
            + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64()));
        Require(!std::filesystem::exists(root), "fixture path already exists");
        TestOldestAndActive(root / "budget");
        TestUnknownIdentity(root / "unknown");
        TestDumpBudget(root / "dumps");
        TestPartialDumpAndLogBudget(root / "partial");
        TestDeletionFailureStopsSweep(root / "delete_failure");
        TestReparseProtection(root / "reparse");
        std::cout << "session retention tests passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
