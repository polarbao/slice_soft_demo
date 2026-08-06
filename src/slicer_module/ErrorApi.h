#pragma once

#include <string_view>

namespace slicesoft::module
{

/**
 * @brief Replaces this thread's most recent SPI failure JSON.
 * @param code Stable PM-SLICER error code.
 * @param message Human-readable UTF-8 error summary.
 * @param detail Human-readable UTF-8 diagnostic detail.
 *
 * Successful operations deliberately do not call this function, so the last
 * failure remains available until the next failure on the same thread.
 */
void SetThreadLastError(
    std::string_view code,
    std::string_view message,
    std::string_view detail) noexcept;

/**
 * @brief Returns this thread's most recent SPI failure as stable JSON.
 * @return A view valid until SetThreadLastError is next called on this thread.
 */
[[nodiscard]] std::string_view GetThreadLastErrorJson() noexcept;

}  // namespace slicesoft::module
