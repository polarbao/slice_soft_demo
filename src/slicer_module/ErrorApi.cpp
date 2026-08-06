#include "ErrorApi.h"

#include <array>
#include <cstring>

namespace slicesoft::module
{
namespace
{

constexpr std::size_t kErrorJsonCapacity{8192U};
constexpr std::string_view kEmptyErrorJson{
    R"({"code":"","message":"","detail":""})"};
constexpr std::string_view kOverflowErrorJson{
    R"({"code":"PM-SLICER-INTERNAL-0099","message":"error detail exceeds TLS capacity","detail":""})"};

class ThreadErrorStorage final
{
public:
    ThreadErrorStorage() noexcept
    {
        Assign(kEmptyErrorJson);
    }

    bool Append(const char value) noexcept
    {
        if (m_size >= m_bytes.size())
        {
            return false;
        }
        m_bytes[m_size++] = value;
        return true;
    }

    bool Append(const std::string_view value) noexcept
    {
        if (value.size() > m_bytes.size() - m_size)
        {
            return false;
        }
        std::memcpy(m_bytes.data() + m_size, value.data(), value.size());
        m_size += value.size();
        return true;
    }

    bool AppendEscaped(const std::string_view value) noexcept
    {
        constexpr char hexDigits[]{"0123456789abcdef"};
        for (const unsigned char byte : value)
        {
            switch (byte)
            {
            case '"':
                if (!Append(R"(\")"))
                {
                    return false;
                }
                break;
            case '\\':
                if (!Append(R"(\\)"))
                {
                    return false;
                }
                break;
            case '\b':
                if (!Append(R"(\b)"))
                {
                    return false;
                }
                break;
            case '\f':
                if (!Append(R"(\f)"))
                {
                    return false;
                }
                break;
            case '\n':
                if (!Append(R"(\n)"))
                {
                    return false;
                }
                break;
            case '\r':
                if (!Append(R"(\r)"))
                {
                    return false;
                }
                break;
            case '\t':
                if (!Append(R"(\t)"))
                {
                    return false;
                }
                break;
            default:
                if (byte < 0x20U)
                {
                    if (!Append(R"(\u00)")
                        || !Append(hexDigits[(byte >> 4U) & 0x0fU])
                        || !Append(hexDigits[byte & 0x0fU]))
                    {
                        return false;
                    }
                }
                else if (!Append(static_cast<char>(byte)))
                {
                    return false;
                }
                break;
            }
        }
        return true;
    }

    void Assign(const std::string_view value) noexcept
    {
        m_size = 0U;
        (void)Append(value);
    }

    void Reset() noexcept
    {
        m_size = 0U;
    }

    [[nodiscard]] std::string_view View() const noexcept
    {
        return {m_bytes.data(), m_size};
    }

private:
    std::array<char, kErrorJsonCapacity> m_bytes{};
    std::size_t m_size{0U};
};

thread_local ThreadErrorStorage g_lastError;

}  // namespace

void SetThreadLastError(
    const std::string_view code,
    const std::string_view message,
    const std::string_view detail) noexcept
{
    g_lastError.Reset();
    const bool written = g_lastError.Append(R"({"code":")")
        && g_lastError.AppendEscaped(code)
        && g_lastError.Append(R"(","message":")")
        && g_lastError.AppendEscaped(message)
        && g_lastError.Append(R"(","detail":")")
        && g_lastError.AppendEscaped(detail)
        && g_lastError.Append(R"("})");
    if (!written)
    {
        g_lastError.Assign(kOverflowErrorJson);
    }
}

std::string_view GetThreadLastErrorJson() noexcept
{
    return g_lastError.View();
}

}  // namespace slicesoft::module
