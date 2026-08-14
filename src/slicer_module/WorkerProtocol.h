#pragma once

#include "WorkerClient.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace slicesoft::module::worker_detail
{

/** @brief 解析 Worker 保留的标准输出协议，同时保留普通日志。 */
class WorkerProtocolParser final
{
public:
    /**
     * @brief 将解析器绑定到一次运行结果和可选进度回调。
     * @param result 接收已解析事件和普通日志行的可变结果。
     * @param progressSink 每个有效进度事件触发的可选回调。
     */
    WorkerProtocolParser(WorkerRunResult* result, WorkerProgressSink progressSink);

    /** @brief 向行解析器送入任意标准输出字节块。 */
    void ProcessStdoutChunk(std::string_view chunk);

    /** @brief 向行解析器送入任意标准错误字节块。 */
    void ProcessStderrChunk(std::string_view chunk);

    /** @brief 刷新标准输出和标准错误中末尾未换行的内容。 */
    void Finish();

    /** @brief 报告保留行是否违反文件合同。 */
    [[nodiscard]] bool HasContractError() const noexcept;

    /** @brief 报告进度回调是否抛出异常。 */
    [[nodiscard]] bool HasCallbackError() const noexcept;

    /** @brief 报告最后接受的进度事件是否达到 100%。 */
    [[nodiscard]] bool HasTerminalProgress() const noexcept;

    /** @brief 返回第一条协议或回调诊断。 */
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
