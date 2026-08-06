#pragma once

#include <string_view>

namespace slicesoft::module
{

/**
 * @brief Writes UTF-8 content using the print module SPI three-state buffer protocol.
 * @param content Content to write, without an implicit trailing NUL byte.
 * @param output Caller-owned output buffer, or nullptr for a size probe.
 * @param capacity Output-buffer capacity in bytes; zero also requests a size probe.
 * @param outRequired Optional destination for the required byte count, excluding NUL.
 * @return The written byte count on success, or a PM_ERR_* value on failure.
 * @note A buffer smaller than content.size() + 1 is never modified.
 */
[[nodiscard]] int WriteOut(
    std::string_view content,
    char* output,
    int capacity,
    int* outRequired) noexcept;

}  // namespace slicesoft::module
