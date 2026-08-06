#include "WorkerProtocol.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace slicesoft::module::worker_detail
{
namespace
{

constexpr std::string_view ProgressPrefix{"SLICE_PROGRESS"};
constexpr std::string_view TimingPrefix{"SLICE_TIMING"};

std::vector<std::string_view> SplitFields(const std::string_view body)
{
    std::vector<std::string_view> fields;
    std::size_t begin{0};
    while (begin < body.size())
    {
        const std::size_t end = body.find(' ', begin);
        const std::size_t length = (end == std::string_view::npos ? body.size() : end) - begin;
        if (length == 0)
        {
            return {};
        }
        fields.push_back(body.substr(begin, length));
        if (end == std::string_view::npos)
        {
            break;
        }
        begin = end + 1;
        if (begin == body.size())
        {
            return {};
        }
    }
    return fields;
}

bool IsToken(const std::string_view value)
{
    return !value.empty() && std::all_of(value.begin(), value.end(), [](const unsigned char character)
    {
        return std::isalnum(character) != 0 || character == '_' || character == '.' || character == '-';
    });
}

bool ParseUnsigned(const std::string_view value, std::uint64_t* output)
{
    if (value.empty() || output == nullptr)
    {
        return false;
    }
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), *output);
    return error == std::errc{} && end == value.data() + value.size();
}

bool ParseFixed3(const std::string_view value, double* output)
{
    const std::size_t point = value.find('.');
    if (output == nullptr || point == 0 || point == std::string_view::npos || point + 4 != value.size())
    {
        return false;
    }
    std::uint64_t whole{0};
    std::uint64_t fraction{0};
    if (!ParseUnsigned(value.substr(0, point), &whole)
        || !ParseUnsigned(value.substr(point + 1), &fraction)
        || fraction > 999)
    {
        return false;
    }
    *output = static_cast<double>(whole) + static_cast<double>(fraction) / 1000.0;
    return true;
}

bool ReadField(
    const std::string_view field,
    const std::string_view expectedKey,
    std::string_view* value)
{
    const std::size_t separator = field.find('=');
    if (value == nullptr || separator == std::string_view::npos
        || separator == 0 || separator + 1 >= field.size())
    {
        return false;
    }
    if (field.substr(0, separator) != expectedKey)
    {
        return false;
    }
    *value = field.substr(separator + 1);
    return true;
}

bool ParseTimingLine(
    const std::string_view line,
    WorkerTimingEvent* event,
    std::string* errorMessage)
{
    if (!line.starts_with("SLICE_TIMING "))
    {
        *errorMessage = "reserved SLICE_TIMING line has invalid prefix grammar";
        return false;
    }
    const std::vector<std::string_view> fields =
        SplitFields(std::string_view{line}.substr(TimingPrefix.size() + 1));
    std::unordered_map<std::string_view, std::string_view> values;
    for (const std::string_view field : fields)
    {
        const std::size_t separator = field.find('=');
        if (separator == std::string_view::npos || separator == 0 || separator + 1 >= field.size()
            || !IsToken(field.substr(0, separator))
            || !values.emplace(field.substr(0, separator), field.substr(separator + 1)).second)
        {
            *errorMessage = "reserved SLICE_TIMING line violates key/value grammar";
            return false;
        }
    }
    const auto engine = values.find("engine");
    const auto total = values.find("totalMs");
    const auto working = values.find("workingSetBytes");
    const auto peak = values.find("peakWorkingSetBytes");
    if (fields.empty() || !fields.front().starts_with("engine=")
        || engine == values.end() || total == values.end()
        || working == values.end() || peak == values.end()
        || !IsToken(engine->second)
        || !ParseFixed3(total->second, &event->totalMs)
        || !ParseUnsigned(working->second, &event->workingSetBytes)
        || !ParseUnsigned(peak->second, &event->peakWorkingSetBytes))
    {
        *errorMessage = "SLICE_TIMING is missing or has invalid required fields";
        return false;
    }
    event->engine.assign(engine->second);
    return true;
}

}  // namespace

WorkerProtocolParser::WorkerProtocolParser(
    WorkerRunResult* result,
    WorkerProgressSink progressSink)
    : m_result(result), m_progressSink(std::move(progressSink))
{
}

void WorkerProtocolParser::ProcessStdoutChunk(const std::string_view chunk)
{
    ProcessChunk(chunk, true, &m_stdoutBuffer);
}

void WorkerProtocolParser::ProcessStderrChunk(const std::string_view chunk)
{
    ProcessChunk(chunk, false, &m_stderrBuffer);
}

void WorkerProtocolParser::Finish()
{
    FinishPipe(true, &m_stdoutBuffer);
    FinishPipe(false, &m_stderrBuffer);
}

bool WorkerProtocolParser::HasContractError() const noexcept
{
    return m_contractError;
}

bool WorkerProtocolParser::HasCallbackError() const noexcept
{
    return m_callbackError;
}

bool WorkerProtocolParser::HasTerminalProgress() const noexcept
{
    return m_terminalProgress;
}

const std::string& WorkerProtocolParser::ErrorMessage() const noexcept
{
    return m_errorMessage;
}

void WorkerProtocolParser::ProcessChunk(
    const std::string_view chunk,
    const bool parseProtocol,
    PipeBuffer* buffer)
{
    buffer->pending.append(chunk);
    std::size_t newline{0};
    while ((newline = buffer->pending.find('\n')) != std::string::npos)
    {
        std::string line = buffer->pending.substr(0, newline);
        buffer->pending.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (parseProtocol)
        {
            ProcessStdoutLine(line);
        }
        else
        {
            m_result->stderrLogLines.push_back(std::move(line));
        }
    }
}

void WorkerProtocolParser::ProcessStdoutLine(const std::string& line)
{
    if (line.starts_with(ProgressPrefix))
    {
        WorkerProgressEvent event;
        if (!line.starts_with("SLICE_PROGRESS "))
        {
            m_errorMessage = "reserved SLICE_PROGRESS line has invalid prefix grammar";
            m_contractError = true;
            return;
        }
        const std::vector<std::string_view> fields =
            SplitFields(std::string_view{line}.substr(ProgressPrefix.size() + 1));
        std::string_view phase;
        std::string_view current;
        std::string_view total;
        std::string_view percent;
        std::string_view elapsed;
        std::uint64_t parsedPercent{0};
        const bool fieldsValid = fields.size() == 5
            && ReadField(fields[0], "phase", &phase)
            && ReadField(fields[1], "current", &current)
            && ReadField(fields[2], "total", &total)
            && ReadField(fields[3], "percent", &percent)
            && ReadField(fields[4], "elapsedMs", &elapsed);
        const bool phaseValid = fieldsValid && IsToken(phase);
        const bool currentValid = fieldsValid && ParseUnsigned(current, &event.current);
        const bool totalValid = fieldsValid && ParseUnsigned(total, &event.total);
        const bool percentValid = fieldsValid && ParseUnsigned(percent, &parsedPercent);
        const bool elapsedValid = fieldsValid && ParseFixed3(elapsed, &event.elapsedMs);
        if (!fieldsValid || !phaseValid || !currentValid || !totalValid
            || !percentValid || !elapsedValid
            || event.current > event.total
            || parsedPercent > 100)
        {
            m_errorMessage = "reserved SLICE_PROGRESS line violates file_contract_v1 grammar";
            m_contractError = true;
            return;
        }
        event.phase.assign(phase);
        event.percent = static_cast<std::uint32_t>(parsedPercent);
        if (m_hasProgress
            && (event.percent < m_lastPercent || event.elapsedMs < m_lastElapsedMs))
        {
            m_errorMessage = "SLICE_PROGRESS percent or elapsedMs moved backwards";
            m_contractError = true;
            return;
        }
        if (m_hasProgress && event.phase == m_lastPhase
            && (event.total != m_lastTotal || event.current < m_lastCurrent))
        {
            m_errorMessage = "SLICE_PROGRESS current/total is not monotonic within one phase";
            m_contractError = true;
            return;
        }
        m_hasProgress = true;
        m_terminalProgress = event.percent == 100;
        m_lastPercent = event.percent;
        m_lastElapsedMs = event.elapsedMs;
        m_lastPhase = event.phase;
        m_lastCurrent = event.current;
        m_lastTotal = event.total;
        m_result->progressEvents.push_back(event);
        if (m_progressSink)
        {
            try
            {
                m_progressSink(event);
            }
            catch (...)
            {
                m_errorMessage = "progress callback threw an exception";
                m_callbackError = true;
            }
        }
        return;
    }
    if (line.starts_with(TimingPrefix))
    {
        WorkerTimingEvent event;
        if (!ParseTimingLine(line, &event, &m_errorMessage))
        {
            m_contractError = true;
            return;
        }
        m_result->timingEvents.push_back(event);
        return;
    }
    m_result->stdoutLogLines.push_back(line);
}

void WorkerProtocolParser::FinishPipe(const bool parseProtocol, PipeBuffer* buffer)
{
    if (buffer->pending.empty())
    {
        return;
    }
    if (parseProtocol)
    {
        ProcessStdoutLine(buffer->pending);
    }
    else
    {
        m_result->stderrLogLines.push_back(buffer->pending);
    }
    buffer->pending.clear();
}

}  // namespace slicesoft::module::worker_detail
