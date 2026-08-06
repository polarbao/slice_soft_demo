#include "WorkerProcessWindows.h"

#include <optional>
#include <system_error>

namespace slicesoft::module::worker_detail
{
namespace
{

std::optional<std::wstring> Utf8ToWide(const std::string_view value)
{
    if (value.empty())
    {
        return std::wstring{};
    }
    const int count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0)
    {
        return std::nullopt;
    }
    std::wstring output(static_cast<std::size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), output.data(), count) != count)
    {
        return std::nullopt;
    }
    return output;
}

std::wstring QuoteArgument(const std::wstring_view argument)
{
    if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring_view::npos)
    {
        return std::wstring{argument};
    }
    std::wstring quoted{L'"'};
    std::size_t slashes{0};
    for (const wchar_t character : argument)
    {
        if (character == L'\\')
        {
            ++slashes;
            continue;
        }
        if (character == L'"')
        {
            quoted.append(slashes * 2 + 1, L'\\');
        }
        else
        {
            quoted.append(slashes, L'\\');
        }
        slashes = 0;
        quoted.push_back(character);
    }
    quoted.append(slashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

}  // namespace

UniqueHandle::UniqueHandle(const HANDLE handle) : m_handle(handle)
{
}

UniqueHandle::~UniqueHandle()
{
    Reset();
}

UniqueHandle::UniqueHandle(UniqueHandle&& other) noexcept : m_handle(other.Release())
{
}

UniqueHandle& UniqueHandle::operator=(UniqueHandle&& other) noexcept
{
    if (this != &other)
    {
        Reset(other.Release());
    }
    return *this;
}

HANDLE UniqueHandle::Get() const noexcept
{
    return m_handle;
}

bool UniqueHandle::IsValid() const noexcept
{
    return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
}

HANDLE UniqueHandle::Release() noexcept
{
    const HANDLE handle = m_handle;
    m_handle = nullptr;
    return handle;
}

void UniqueHandle::Reset(const HANDLE handle) noexcept
{
    if (IsValid())
    {
        CloseHandle(m_handle);
    }
    m_handle = handle;
}

ProcessAttributeList::~ProcessAttributeList()
{
    if (m_list != nullptr)
    {
        DeleteProcThreadAttributeList(m_list);
    }
}

bool ProcessAttributeList::Initialize(const std::array<HANDLE, 2>& inheritedHandles)
{
    SIZE_T bytes{0};
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    if (bytes == 0)
    {
        return false;
    }
    m_storage.resize(bytes);
    m_list = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(m_storage.data());
    if (InitializeProcThreadAttributeList(m_list, 1, 0, &bytes) == FALSE)
    {
        m_list = nullptr;
        return false;
    }
    return UpdateProcThreadAttribute(m_list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        const_cast<HANDLE*>(inheritedHandles.data()), sizeof(inheritedHandles), nullptr, nullptr) != FALSE;
}

PPROC_THREAD_ATTRIBUTE_LIST ProcessAttributeList::Get() const noexcept
{
    return m_list;
}

bool BuildCommandLine(const WorkerLaunchOptions& options, std::wstring* commandLine)
{
    *commandLine = QuoteArgument(options.executablePath.wstring());
    for (const std::string& argument : options.arguments)
    {
        const std::optional<std::wstring> wide = Utf8ToWide(argument);
        if (!wide.has_value())
        {
            return false;
        }
        commandLine->push_back(L' ');
        commandLine->append(QuoteArgument(*wide));
    }
    return true;
}

std::string WindowsError(const std::string_view action, const DWORD code)
{
    return std::string{action} + " failed: "
        + std::system_category().message(static_cast<int>(code));
}

bool CreatePipePair(UniqueHandle* read, UniqueHandle* write)
{
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE readHandle{nullptr};
    HANDLE writeHandle{nullptr};
    if (CreatePipe(&readHandle, &writeHandle, &attributes, 0) == FALSE)
    {
        return false;
    }
    read->Reset(readHandle);
    write->Reset(writeHandle);
    return SetHandleInformation(read->Get(), HANDLE_FLAG_INHERIT, 0) != FALSE;
}

bool WriteCancellationMarker(const std::filesystem::path& marker)
{
    if (marker.empty())
    {
        return true;
    }
    const std::filesystem::path temporary = marker.wstring() + L".tmp."
        + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
    UniqueHandle file{CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY, nullptr)};
    if (!file.IsValid())
    {
        return false;
    }
    file.Reset();
    if (MoveFileExW(temporary.c_str(), marker.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE)
    {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

}  // namespace slicesoft::module::worker_detail
