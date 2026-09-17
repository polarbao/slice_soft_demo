#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace slicesoft::diagnostics
{
inline constexpr std::size_t MaximumEventBytes = 16384;
struct LogEvent
{
    int level{2};
    std::string domain{"slicer.app"};
    std::string module;
    std::string action;
    std::string phase;
    std::string code;
    std::string message;
    std::string jobId;
    std::string moduleInstanceId;
    std::string sourceProcessRole{"host"};
    std::uint64_t sequence{0};
    std::uint32_t sourcePid{0};
    std::uint32_t sourceTid{0};
    std::int64_t timestampUtcMs{0};
    std::uint64_t droppedEventCount{0};
    std::uint64_t discardedEventCount{0};
    std::uint64_t callbackFailureCount{0};
    std::uint64_t encodingFailureCount{0};
};

// Empty means encoding failed. Serialization bounds fields before encoding.
std::string EncodeEvent(const LogEvent& event) noexcept;
bool ValidEvent(std::string_view json, int level) noexcept;
std::uint32_t CurrentProcessId() noexcept;
std::uint32_t CurrentThreadId() noexcept;
}
