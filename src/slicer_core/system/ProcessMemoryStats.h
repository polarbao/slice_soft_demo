#pragma once

#include <cstdint>

namespace slicer_core
{

/**
 * @brief Current process memory counters.
 */
struct ProcessMemoryStats
{
    bool available{false};
    std::uint64_t working_set_bytes{0};
    std::uint64_t peak_working_set_bytes{0};
};

/**
 * @brief Capture current process memory statistics when the platform supports it.
 * @return Process memory statistics. Non-Windows builds return unavailable.
 */
ProcessMemoryStats CaptureProcessMemoryStats();

}  // namespace slicer_core
