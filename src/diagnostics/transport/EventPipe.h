#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace slicesoft::diagnostics::transport
{
using EventSink = std::function<void(std::string_view, std::uint32_t)>;

// Best-effort local message transport. Full buffers drop entire events.
class EventPipeServer final
{
public:
    EventPipeServer() noexcept;
    ~EventPipeServer();
    EventPipeServer(const EventPipeServer&) = delete;
    EventPipeServer& operator=(const EventPipeServer&) = delete;
    const std::wstring& Endpoint() const noexcept;
    void Poll(std::uint32_t expectedPid, const EventSink& sink) noexcept;
private:
    class Implementation;
    std::unique_ptr<Implementation> m_impl;
};

class EventPipeClient final
{
public:
    EventPipeClient() noexcept;
    ~EventPipeClient();
    EventPipeClient(const EventPipeClient&) = delete;
    EventPipeClient& operator=(const EventPipeClient&) = delete;
    bool Connect(std::wstring_view endpoint, std::uint32_t expectedParentPid) noexcept;
    bool Send(std::string_view json) noexcept;
private:
    class Implementation;
    std::unique_ptr<Implementation> m_impl;
};

// Preserves inherited entries (including =C:) without changing the parent's env.
std::vector<wchar_t> WorkerEnvironment(std::wstring_view endpoint) noexcept;
bool ConnectFromWorkerEnvironment(EventPipeClient& client) noexcept;
}
