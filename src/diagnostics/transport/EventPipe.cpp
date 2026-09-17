#include "EventPipe.h"
#include "diagnostics/LogEvent.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <sddl.h>
#include <bcrypt.h>

#include <array>
#include <utility>

namespace slicesoft::diagnostics::transport
{
namespace
{
class Handle
{
public:
    HANDLE value{INVALID_HANDLE_VALUE};
    ~Handle() { Reset(); }
    void Reset(HANDLE next = INVALID_HANDLE_VALUE) noexcept
    {
        if (value != INVALID_HANDLE_VALUE && value != nullptr) CloseHandle(value);
        value = next;
    }
    bool Valid() const noexcept { return value != INVALID_HANDLE_VALUE && value != nullptr; }
};

std::wstring CurrentUserSddl()
{
    Handle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value)) return {};
    DWORD bytes{0};
    (void)GetTokenInformation(token.value, TokenUser, nullptr, 0, &bytes);
    if (bytes == 0) return {};
    std::vector<unsigned char> data(bytes);
    if (!GetTokenInformation(token.value, TokenUser, data.data(), bytes, &bytes)) return {};
    LPWSTR sid{nullptr};
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid, &sid)) return {};
    std::wstring sddl;
    try { sddl = std::wstring{L"D:P(A;;GA;;;"} + sid + L")"; }
    catch (...) { LocalFree(sid); throw; }
    LocalFree(sid);
    return sddl;
}

std::wstring NewEndpoint()
{
    std::array<unsigned char, 16> random{};
    if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        return {};
    std::wstring name = L"\\\\.\\pipe\\slicesoft-log-" + std::to_wstring(GetCurrentProcessId()) + L"-";
    constexpr wchar_t hex[] = L"0123456789abcdef";
    for (const auto byte : random) { name += hex[byte >> 4]; name += hex[byte & 15]; }
    return name;
}
}

class EventPipeServer::Implementation
{
public:
    Handle pipe;
    std::wstring endpoint;
};

EventPipeServer::EventPipeServer() noexcept
{
    try
    {
        m_impl = std::make_unique<Implementation>();
        const auto sddl = CurrentUserSddl();
        if (sddl.empty()) return;
        PSECURITY_DESCRIPTOR descriptor{nullptr};
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) return;
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
        try { m_impl->endpoint = NewEndpoint(); }
        catch (...) { LocalFree(descriptor); throw; }
        if (!m_impl->endpoint.empty())
            m_impl->pipe.Reset(CreateNamedPipeW(m_impl->endpoint.c_str(),
                PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
                1, 65536, 65536, 0, &security));
        LocalFree(descriptor);
        if (!m_impl->pipe.Valid()) { m_impl->endpoint.clear(); return; }
        (void)ConnectNamedPipe(m_impl->pipe.value, nullptr);
    }
    catch (...) { m_impl.reset(); }
}

EventPipeServer::~EventPipeServer() = default;

const std::wstring& EventPipeServer::Endpoint() const noexcept
{
    static const std::wstring empty;
    return m_impl ? m_impl->endpoint : empty;
}

void EventPipeServer::Poll(const std::uint32_t expectedPid, const EventSink& sink) noexcept
{
    if (!m_impl || !m_impl->pipe.Valid() || expectedPid == 0 || !sink) return;
    try
    {
        ULONG clientPid{0};
        if (!GetNamedPipeClientProcessId(m_impl->pipe.value, &clientPid)) return;
        if (clientPid != expectedPid)
        {
            (void)DisconnectNamedPipe(m_impl->pipe.value);
            (void)ConnectNamedPipe(m_impl->pipe.value, nullptr);
            return;
        }
        std::array<char, MaximumEventBytes> bytes{};
        for (int count = 0; count < 16; ++count)
        {
            DWORD received{0};
            if (!ReadFile(m_impl->pipe.value, bytes.data(), static_cast<DWORD>(bytes.size()), &received, nullptr))
            {
                if (GetLastError() == ERROR_MORE_DATA)
                {
                    // Oversized frames cannot be allowed to grow a reassembly buffer.
                    (void)DisconnectNamedPipe(m_impl->pipe.value);
                    (void)ConnectNamedPipe(m_impl->pipe.value, nullptr);
                }
                break;
            }
            if (received == 0) break;
            try { sink(std::string_view{bytes.data(), received}, clientPid); }
            catch (...) { break; }
        }
    }
    catch (...) {}
}

class EventPipeClient::Implementation { public: Handle pipe; };
EventPipeClient::EventPipeClient() noexcept
{
    try { m_impl = std::make_unique<Implementation>(); } catch (...) {}
}
EventPipeClient::~EventPipeClient() = default;

bool EventPipeClient::Connect(const std::wstring_view endpoint, const std::uint32_t expectedParentPid) noexcept
{
    if (!m_impl || expectedParentPid == 0 || endpoint.size() > 240
        || !endpoint.starts_with(L"\\\\.\\pipe\\slicesoft-log-") || endpoint.find(L'\0') != std::wstring_view::npos)
        return false;
    try
    {
        m_impl->pipe.Reset(CreateFileW(std::wstring{endpoint}.c_str(), GENERIC_WRITE | FILE_WRITE_ATTRIBUTES,
            0, nullptr, OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
        if (!m_impl->pipe.Valid()) return false;
        ULONG serverPid{0};
        DWORD mode{PIPE_NOWAIT};
        if (!GetNamedPipeServerProcessId(m_impl->pipe.value, &serverPid) || serverPid != expectedParentPid
            || !SetNamedPipeHandleState(m_impl->pipe.value, &mode, nullptr, nullptr))
        { m_impl->pipe.Reset(); return false; }
        return true;
    }
    catch (...) { return false; }
}

bool EventPipeClient::Send(const std::string_view json) noexcept
{
    if (!m_impl || !m_impl->pipe.Valid() || json.empty() || json.size() > MaximumEventBytes) return false;
    DWORD written{0};
    const auto success = WriteFile(m_impl->pipe.value, json.data(), static_cast<DWORD>(json.size()), &written, nullptr);
    return success != FALSE && written == json.size();
}
}
