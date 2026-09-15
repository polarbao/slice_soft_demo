// P0FIX / P0-03：Worker 诊断容器上界。
//
// 这里要守的不只是"别无限增长"，更关键的是【末段必须保留】：
// file_contract_v1 §4 规定终态成功前必须发出 percent=100，且仓库中有三处
// 断言 progressEvents.back().percent == 100（EngineConformanceGate.cpp、
// EngineConformanceSupport.cpp、stage16 用例）。任何"丢最新"的实现都会
// 打断这条不变量——本用例的第 3 组就是专门钉死这一点。

#include "slicer_module/WorkerProtocol.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

std::string ProgressLine(const int index, const int total)
{
    std::ostringstream line;
    line << "SLICE_PROGRESS phase=compose"
         << " current=" << index
         << " total=" << total
         << " percent=" << (index * 100 / total)
         << " elapsedMs=" << index << ".000\n";
    return line.str();
}

}  // namespace

int main()
{
    using slicesoft::module::WorkerRunResult;
    using slicesoft::module::worker_detail::WorkerProtocolParser;

    bool passed = true;

    // 1. 低于上限时行为完全不变：不丢弃、不计数。
    {
        WorkerRunResult result;
        WorkerProtocolParser parser{&result, nullptr};
        for (int i = 1; i <= 10; ++i)
        {
            parser.ProcessStdoutChunk(ProgressLine(i, 10));
        }
        parser.Finish();
        passed = ExpectTrue(result.progressEvents.size() == 10U,
                     "under the cap all progress events are kept")
            && passed;
        passed = ExpectTrue(result.droppedProgressEvents == 0U,
                     "under the cap nothing is counted as dropped")
            && passed;
        passed = ExpectTrue(result.progressEvents.back().percent == 100U,
                     "terminal progress survives under the cap")
            && passed;
    }

    // 2. 超过上限：容器被钉在 head+tail，丢弃计数非零。
    {
        constexpr int kTotal = 5000;
        WorkerRunResult result;
        WorkerProtocolParser parser{&result, nullptr};
        for (int i = 1; i <= kTotal; ++i)
        {
            parser.ProcessStdoutChunk(ProgressLine(i, kTotal));
        }
        parser.Finish();

        const std::size_t kept = result.progressEvents.size();
        passed = ExpectTrue(kept == 8U + 256U,
                     "progress events are capped at head+tail")
            && passed;
        passed = ExpectTrue(
                     result.droppedProgressEvents
                         == static_cast<std::uint64_t>(kTotal) - kept,
                     "dropped count accounts for every discarded event")
            && passed;

        // 3. 【最关键】末段必须是最新的，首段必须是最早的。
        passed = ExpectTrue(result.progressEvents.back().current
                         == static_cast<std::uint64_t>(kTotal),
                     "back() is the newest event, not an old one")
            && passed;
        passed = ExpectTrue(result.progressEvents.back().percent == 100U,
                     "terminal percent=100 survives truncation (file_contract_v1 §4)")
            && passed;
        passed = ExpectTrue(result.progressEvents.front().current == 1U,
                     "front() keeps the earliest event for diagnosis")
            && passed;
    }

    // 4. 普通日志行同样有上界，且末段保留最新。
    {
        constexpr int kLines = 4000;
        WorkerRunResult result;
        WorkerProtocolParser parser{&result, nullptr};
        for (int i = 1; i <= kLines; ++i)
        {
            parser.ProcessStdoutChunk("ordinary line " + std::to_string(i) + "\n");
        }
        parser.Finish();

        const std::size_t kept = result.stdoutLogLines.size();
        passed = ExpectTrue(kept == 32U + 2048U, "stdout log lines are capped")
            && passed;
        passed = ExpectTrue(
                     result.droppedStdoutLogLines
                         == static_cast<std::uint64_t>(kLines) - kept,
                     "dropped stdout count is exact")
            && passed;
        passed = ExpectTrue(
                     result.stdoutLogLines.back() == "ordinary line 4000",
                     "newest stdout line is retained")
            && passed;
        passed = ExpectTrue(
                     result.stdoutLogLines.front() == "ordinary line 1",
                     "earliest stdout line is retained")
            && passed;
    }

    // 5. stderr 走另一条代码路径，同样受限。
    {
        constexpr int kLines = 4000;
        WorkerRunResult result;
        WorkerProtocolParser parser{&result, nullptr};
        for (int i = 1; i <= kLines; ++i)
        {
            parser.ProcessStderrChunk("err " + std::to_string(i) + "\n");
        }
        parser.Finish();
        passed = ExpectTrue(result.stderrLogLines.size() == 32U + 2048U,
                     "stderr log lines are capped")
            && passed;
        passed = ExpectTrue(result.droppedStderrLogLines > 0U,
                     "stderr drops are counted")
            && passed;
        passed = ExpectTrue(result.stderrLogLines.back() == "err 4000",
                     "newest stderr line is retained")
            && passed;
    }

    if (!passed)
    {
        std::cerr << "worker protocol bounds: FAIL\n";
        return 1;
    }
    std::cout << "worker protocol bounds: PASS\n";
    return 0;
}
