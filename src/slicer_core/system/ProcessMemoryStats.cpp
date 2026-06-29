#include "slicer_core/system/ProcessMemoryStats.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace slicer_core
{

ProcessMemoryStats CaptureProcessMemoryStats()
{
    ProcessMemoryStats stats;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) != 0)
    {
        stats.available = true;
        stats.working_set_bytes = static_cast<std::uint64_t>(counters.WorkingSetSize);
        stats.peak_working_set_bytes = static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
    }
#endif
    return stats;
}

}  // namespace slicer_core
