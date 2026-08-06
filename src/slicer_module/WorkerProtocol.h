#pragma once

#include "WorkerClient.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace slicesoft::module::worker_detail
{

/** @brief Parses the reserved Worker stdout protocol while retaining ordinary logs. */
class WorkerProtocolParser final
{
public:
    /**
     * @brief Binds the parser to one run result and optional progress callback.
     * @param result Mutable result receiving parsed events and ordinary log lines.
     * @param progressSink Optional callback invoked for every valid progress event.
     */
    WorkerProtocolParser(WorkerRunResult* result, WorkerProgressSink progressSink);

    /** @brief Feeds an arbitrary stdout byte chunk into the line parser. */
    void ProcessStdoutChunk(std::string_view chunk);

    /** @brief Feeds an arbitrary stderr byte chunk into the line parser. */
    void ProcessStderrChunk(std::string_view chunk);

    /** @brief Flushes final unterminated stdout and stderr lines. */
    void Finish();

    /** @brief Reports whether a reserved line violated the file contract. */
    [[nodiscard]] bool HasContractError() const noexcept;

    /** @brief Reports whether the progress callback threw an exception. */
    [[nodiscard]] bool HasCallbackError() const noexcept;

    /** @brief Reports whether the last accepted progress event reached 100 percent. */
    [[nodiscard]] bool HasTerminalProgress() const noexcept;

    /** @brief Returns the first protocol or callback diagnostic. */
    [[nodiscard]] const std::string& ErrorMessage() const noexcept;

private:
    struct PipeBuffer
    {
        std::string pending;
    };

    void ProcessChunk(std::string_view chunk, bool parseProtocol, PipeBuffer* buffer);
    void ProcessStdoutLine(const std::string& line);
    void FinishPipe(bool parseProtocol, PipeBuffer* buffer);

    WorkerRunResult* m_result{nullptr};
    WorkerProgressSink m_progressSink;
    PipeBuffer m_stdoutBuffer;
    PipeBuffer m_stderrBuffer;
    bool m_hasProgress{false};
    bool m_terminalProgress{false};
    bool m_contractError{false};
    bool m_callbackError{false};
    std::uint32_t m_lastPercent{0};
    double m_lastElapsedMs{0.0};
    std::string m_lastPhase;
    std::uint64_t m_lastCurrent{0};
    std::uint64_t m_lastTotal{0};
    std::string m_errorMessage;
};

}  // namespace slicesoft::module::worker_detail
