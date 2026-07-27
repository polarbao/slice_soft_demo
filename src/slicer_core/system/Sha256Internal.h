#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace slicer_core::detail
{

inline constexpr std::array<std::uint32_t, 64> kSha256RoundConstants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

class Sha256Hasher
{
public:
    void Update(const std::uint8_t* data, const std::size_t size)
    {
        for (std::size_t index{0U}; index < size; ++index)
        {
            m_buffer.at(m_bufferSize++) = data[index];
            ++m_totalBytes;
            if (m_bufferSize == m_buffer.size())
            {
                ProcessBlock(m_buffer.data());
                m_bufferSize = 0U;
            }
        }
    }

    std::array<std::uint8_t, 32> Finalize()
    {
        const std::uint64_t totalBits = m_totalBytes * 8U;
        m_buffer.at(m_bufferSize++) = 0x80U;
        if (m_bufferSize > 56U)
        {
            while (m_bufferSize < m_buffer.size())
            {
                m_buffer.at(m_bufferSize++) = 0U;
            }
            ProcessBlock(m_buffer.data());
            m_bufferSize = 0U;
        }
        while (m_bufferSize < 56U)
        {
            m_buffer.at(m_bufferSize++) = 0U;
        }
        for (int shift{56}; shift >= 0; shift -= 8)
        {
            m_buffer.at(m_bufferSize++) =
                static_cast<std::uint8_t>(totalBits >> shift);
        }
        ProcessBlock(m_buffer.data());

        std::array<std::uint8_t, 32> digest{};
        for (std::size_t index{0U}; index < m_state.size(); ++index)
        {
            digest.at(index * 4U) =
                static_cast<std::uint8_t>(m_state.at(index) >> 24U);
            digest.at(index * 4U + 1U) =
                static_cast<std::uint8_t>(m_state.at(index) >> 16U);
            digest.at(index * 4U + 2U) =
                static_cast<std::uint8_t>(m_state.at(index) >> 8U);
            digest.at(index * 4U + 3U) =
                static_cast<std::uint8_t>(m_state.at(index));
        }
        return digest;
    }

private:
    static std::uint32_t RotateRight(
        const std::uint32_t value,
        const int bits)
    {
        return (value >> bits) | (value << (32 - bits));
    }

    void ProcessBlock(const std::uint8_t* block)
    {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index{0U}; index < 16U; ++index)
        {
            words.at(index) =
                (static_cast<std::uint32_t>(block[index * 4U]) << 24U)
                | (static_cast<std::uint32_t>(block[index * 4U + 1U]) << 16U)
                | (static_cast<std::uint32_t>(block[index * 4U + 2U]) << 8U)
                | static_cast<std::uint32_t>(block[index * 4U + 3U]);
        }
        for (std::size_t index{16U}; index < words.size(); ++index)
        {
            const std::uint32_t s0 =
                RotateRight(words.at(index - 15U), 7)
                ^ RotateRight(words.at(index - 15U), 18)
                ^ (words.at(index - 15U) >> 3U);
            const std::uint32_t s1 =
                RotateRight(words.at(index - 2U), 17)
                ^ RotateRight(words.at(index - 2U), 19)
                ^ (words.at(index - 2U) >> 10U);
            words.at(index) =
                words.at(index - 16U) + s0 + words.at(index - 7U) + s1;
        }

        std::uint32_t a = m_state.at(0U);
        std::uint32_t b = m_state.at(1U);
        std::uint32_t c = m_state.at(2U);
        std::uint32_t d = m_state.at(3U);
        std::uint32_t e = m_state.at(4U);
        std::uint32_t f = m_state.at(5U);
        std::uint32_t g = m_state.at(6U);
        std::uint32_t h = m_state.at(7U);

        for (std::size_t index{0U}; index < words.size(); ++index)
        {
            const std::uint32_t sum1 =
                RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
            const std::uint32_t choose = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 =
                h + sum1 + choose + kSha256RoundConstants.at(index)
                + words.at(index);
            const std::uint32_t sum0 =
                RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        m_state.at(0U) += a;
        m_state.at(1U) += b;
        m_state.at(2U) += c;
        m_state.at(3U) += d;
        m_state.at(4U) += e;
        m_state.at(5U) += f;
        m_state.at(6U) += g;
        m_state.at(7U) += h;
    }

    std::array<std::uint32_t, 8> m_state{
        0x6a09e667U,
        0xbb67ae85U,
        0x3c6ef372U,
        0xa54ff53aU,
        0x510e527fU,
        0x9b05688cU,
        0x1f83d9abU,
        0x5be0cd19U};
    std::array<std::uint8_t, 64> m_buffer{};
    std::size_t m_bufferSize{0U};
    std::uint64_t m_totalBytes{0U};
};

}  // namespace slicer_core::detail
