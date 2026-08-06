#pragma once

#include "WorkerClient.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace slicesoft::module::worker_detail
{

/** @brief Move-only owner for a Win32 HANDLE. */
class UniqueHandle final
{
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE handle);
    ~UniqueHandle();

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept;
    UniqueHandle& operator=(UniqueHandle&& other) noexcept;

    [[nodiscard]] HANDLE Get() const noexcept;
    [[nodiscard]] bool IsValid() const noexcept;
    HANDLE Release() noexcept;
    void Reset(HANDLE handle = nullptr) noexcept;

private:
    HANDLE m_handle{nullptr};
};

/** @brief Owns the process handle-inheritance attribute list. */
class ProcessAttributeList final
{
public:
    ProcessAttributeList() = default;
    ~ProcessAttributeList();

    ProcessAttributeList(const ProcessAttributeList&) = delete;
    ProcessAttributeList& operator=(const ProcessAttributeList&) = delete;

    /** @brief Initializes an explicit two-handle inheritance list. */
    bool Initialize(const std::array<HANDLE, 2>& inheritedHandles);

    /** @brief Returns the initialized Win32 attribute list. */
    [[nodiscard]] PPROC_THREAD_ATTRIBUTE_LIST Get() const noexcept;

private:
    std::vector<unsigned char> m_storage;
    PPROC_THREAD_ATTRIBUTE_LIST m_list{nullptr};
};

/** @brief Builds a Windows command line from one executable and UTF-8 arguments. */
bool BuildCommandLine(const WorkerLaunchOptions& options, std::wstring* commandLine);

/** @brief Formats a Win32 error using the stable system category text. */
std::string WindowsError(std::string_view action, DWORD code = GetLastError());

/** @brief Creates one inheritable write / non-inheritable read anonymous pipe pair. */
bool CreatePipePair(UniqueHandle* read, UniqueHandle* write);

/** @brief Atomically creates or replaces the cooperative cancellation marker. */
bool WriteCancellationMarker(const std::filesystem::path& marker);

}  // namespace slicesoft::module::worker_detail
