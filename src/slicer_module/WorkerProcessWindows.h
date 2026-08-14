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

/** @brief 仅可移动的 Win32 HANDLE 所有者。 */
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

/** @brief 封装用于限制子进程 HANDLE 继承范围的属性列表。 */
class ProcessAttributeList final
{
public:
    ProcessAttributeList() = default;
    ~ProcessAttributeList();

    ProcessAttributeList(const ProcessAttributeList&) = delete;
    ProcessAttributeList& operator=(const ProcessAttributeList&) = delete;

    /** @brief 初始化仅允许两个指定 HANDLE 继承的属性列表。 */
    bool Initialize(const std::array<HANDLE, 2>& inheritedHandles);

    /** @brief 返回已初始化的 Win32 属性列表。 */
    [[nodiscard]] PPROC_THREAD_ATTRIBUTE_LIST Get() const noexcept;

private:
    std::vector<unsigned char> m_storage;
    PPROC_THREAD_ATTRIBUTE_LIST m_list{nullptr};
};

/** @brief 根据可执行文件和 UTF-8 参数构造 Windows 命令行。 */
bool BuildCommandLine(const WorkerLaunchOptions& options, std::wstring* commandLine);

/** @brief 使用稳定的系统分类文本格式化 Win32 错误。 */
std::string WindowsError(std::string_view action, DWORD code = GetLastError());

/** @brief 创建写端可继承、读端不可继承的匿名管道对。 */
bool CreatePipePair(UniqueHandle* read, UniqueHandle* write);

/** @brief 原子创建或替换协作式取消标记。 */
bool WriteCancellationMarker(const std::filesystem::path& marker);

}  // namespace slicesoft::module::worker_detail
