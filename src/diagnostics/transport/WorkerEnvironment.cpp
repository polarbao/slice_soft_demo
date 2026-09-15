#include "EventPipe.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <algorithm>
#include <cwchar>
#include <limits>

namespace slicesoft::diagnostics::transport
{
namespace
{
constexpr wchar_t PipeKey[] = L"SLICESOFT_LOG_PIPE";
constexpr wchar_t ParentKey[] = L"SLICESOFT_LOG_PIPE_PARENT";
bool IsEntry(const std::wstring_view entry, const std::wstring_view key)
{
    return entry.size() > key.size() && entry[key.size()] == L'='
        && CompareStringOrdinal(entry.data(), static_cast<int>(key.size()), key.data(),
            static_cast<int>(key.size()), TRUE) == CSTR_EQUAL;
}
std::wstring ReadVariable(const wchar_t* key)
{
    const auto size = GetEnvironmentVariableW(key, nullptr, 0);
    if (size == 0 || size > 256) return {};
    std::wstring value(size, L'\0');
    const auto used = GetEnvironmentVariableW(key, value.data(), size);
    if (used == 0 || used >= size) return {};
    value.resize(used);
    return value;
}
}

std::vector<wchar_t> WorkerEnvironment(const std::wstring_view endpoint) noexcept
{
    try
    {
        std::vector<std::wstring> entries;
        LPWCH inherited = GetEnvironmentStringsW();
        if (inherited == nullptr) return {};
        try
        {
            for (const wchar_t* cursor = inherited; *cursor != L'\0'; cursor += std::wcslen(cursor) + 1)
            {
                const std::wstring_view entry{cursor};
                if (!IsEntry(entry, PipeKey) && !IsEntry(entry, ParentKey)) entries.emplace_back(entry);
            }
        }
        catch (...) { FreeEnvironmentStringsW(inherited); throw; }
        FreeEnvironmentStringsW(inherited);
        if (!endpoint.empty())
        {
            entries.emplace_back(std::wstring{PipeKey} + L"=" + std::wstring{endpoint});
            entries.emplace_back(std::wstring{ParentKey} + L"=" + std::to_wstring(GetCurrentProcessId()));
        }
        std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right)
            { return _wcsicmp(left.c_str(), right.c_str()) < 0; });
        std::vector<wchar_t> result;
        for (const auto& entry : entries)
        {
            result.insert(result.end(), entry.begin(), entry.end());
            result.push_back(L'\0');
        }
        result.push_back(L'\0');
        if (entries.empty()) result.push_back(L'\0');
        return result;
    }
    catch (...) { return {}; }
}

bool ConnectFromWorkerEnvironment(EventPipeClient& client) noexcept
{
    try
    {
        const auto endpoint = ReadVariable(PipeKey);
        const auto parent = ReadVariable(ParentKey);
        if (endpoint.empty() || parent.empty()) return false;
        wchar_t* end{nullptr};
        const auto parentPid = std::wcstoull(parent.c_str(), &end, 10);
        if (end == parent.c_str() || *end != L'\0' || parentPid == 0
            || parentPid > (std::numeric_limits<std::uint32_t>::max)()) return false;
        return client.Connect(endpoint, static_cast<std::uint32_t>(parentPid));
    }
    catch (...) { return false; }
}
}
