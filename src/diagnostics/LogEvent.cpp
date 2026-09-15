#include "LogEvent.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <functional>
#include <thread>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace slicesoft::diagnostics
{
std::uint32_t CurrentProcessId() noexcept
{
#ifdef _WIN32
    return ::GetCurrentProcessId();
#else
    return 0;
#endif
}
std::uint32_t CurrentThreadId() noexcept
{
#ifdef _WIN32
    return ::GetCurrentThreadId();
#else
    return static_cast<std::uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
}

std::string EncodeEvent(const LogEvent& event) noexcept
{
    try
    {
        if (event.level < 0 || event.level > 5) return {};
        bool truncated = event.message.size() > 2048;
        const auto field = [&truncated](const std::string& value)
        {
            truncated = truncated || value.size() > 128;
            return value.substr(0, 128);
        };
        nlohmann::json value{
            {"schema", "diagnostics.event.v1"}, {"level", event.level},
            {"domain", field(event.domain)}, {"module", field(event.module)},
            {"action", field(event.action)}, {"phase", field(event.phase)},
            {"code", field(event.code)}, {"jobId", field(event.jobId)},
            {"moduleInstanceId", field(event.moduleInstanceId)},
            {"sourceProcessRole", field(event.sourceProcessRole)},
            {"sourcePid", event.sourcePid != 0 ? event.sourcePid : CurrentProcessId()},
            {"sourceTid", event.sourceTid != 0 ? event.sourceTid : CurrentThreadId()},
            {"eventSequence", event.sequence},
            {"timestampUtcMs", event.timestampUtcMs > 0 ? event.timestampUtcMs : std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"message", event.message.substr(0, 2048)}, {"truncated", truncated}};
        if (event.droppedEventCount || event.discardedEventCount || event.callbackFailureCount || event.encodingFailureCount)
            value["diagnosticLoss"] = {{"queueOverflow", event.droppedEventCount},
                {"discardedAtClear", event.discardedEventCount}, {"callbackFailures", event.callbackFailureCount},
                {"encodingFailures", event.encodingFailureCount}};
        auto encoded = value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        if (encoded.size() > MaximumEventBytes)
        {
            value["message"] = "[event truncated: encoded size exceeded budget]";
            value["truncated"] = true;
            encoded = value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        }
        return encoded.size() <= MaximumEventBytes ? encoded : std::string{};
    }
    catch (...) { return {}; }
}

bool ValidEvent(const std::string_view json, const int level) noexcept
{
    try
    {
        if (level < 0 || level > 5 || json.empty() || json.size() > MaximumEventBytes) return false;
        const auto value = nlohmann::json::parse(json, nullptr, false);
        return value.is_object() && value.value("schema", "") == "diagnostics.event.v1"
            && value.value("level", -1) == level && value.contains("message")
            && value["message"].is_string();
    }
    catch (...) { return false; }
}
}
