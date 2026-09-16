// P0FIX / P0-03：Worker 诊断容器上界。
//
// 这里要守的不只是"别无限增长"，更关键的是【末段必须保留】：
// file_contract_v1 §4 规定终态成功前必须发出 percent=100，且仓库中有三处
// 断言 progressEvents.back().percent == 100（EngineConformanceGate.cpp、
// EngineConformanceSupport.cpp、stage16 用例）。任何"丢最新"的实现都会
// 打断这条不变量——本用例的第 3 组就是专门钉死这一点。
//
// 2026-09-15：本文件是 tests/support/Expect.h 的试点之一（F-17 + F-44）。
// 原先自带的 ExpectTrue 只打印一句文字、不打印实参，且 main() 没有任何 catch——
// 一旦被测代码抛出未捕获异常，进程不是快速失败而是挂住等 ctest 超时。

#include "slicer_module/WorkerProtocol.h"
#include "tests/support/Expect.h"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

namespace {

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
    using slicesoft_test::RunCases;

    return RunCases("worker protocol bounds", {
        {"under_the_cap_nothing_is_dropped",
         []
         {
             WorkerRunResult result;
             WorkerProtocolParser parser{&result, nullptr};
             for (int i = 1; i <= 10; ++i)
             {
                 parser.ProcessStdoutChunk(ProgressLine(i, 10));
             }
             parser.Finish();
             SLICESOFT_EXPECT_EQ(result.progressEvents.size(), std::size_t{10},
                 "under the cap all progress events are kept");
             SLICESOFT_EXPECT_EQ(result.droppedProgressEvents, std::uint64_t{0},
                 "under the cap nothing is counted as dropped");
             SLICESOFT_EXPECT_EQ(result.progressEvents.back().percent, 100U,
                 "terminal progress survives under the cap");
         }},

        {"over_the_cap_is_pinned_to_head_plus_tail",
         []
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
             SLICESOFT_EXPECT_EQ(kept, std::size_t{8} + std::size_t{256},
                 "progress events are capped at head+tail");
             SLICESOFT_EXPECT_EQ(result.droppedProgressEvents,
                 static_cast<std::uint64_t>(kTotal) - kept,
                 "dropped count accounts for every discarded event");
         }},

        // 最关键的一组：末段必须是最新的，首段必须是最早的。
        {"truncation_keeps_newest_tail_and_oldest_head",
         []
         {
             constexpr int kTotal = 5000;
             WorkerRunResult result;
             WorkerProtocolParser parser{&result, nullptr};
             for (int i = 1; i <= kTotal; ++i)
             {
                 parser.ProcessStdoutChunk(ProgressLine(i, kTotal));
             }
             parser.Finish();

             SLICESOFT_EXPECT_EQ(result.progressEvents.back().current,
                 static_cast<std::uint64_t>(kTotal),
                 "back() is the newest event, not an old one");
             SLICESOFT_EXPECT_EQ(result.progressEvents.back().percent, 100U,
                 "terminal percent=100 survives truncation (file_contract_v1 §4)");
             SLICESOFT_EXPECT_EQ(result.progressEvents.front().current, std::uint64_t{1},
                 "front() keeps the earliest event for diagnosis");
         }},

        {"stdout_log_lines_are_bounded",
         []
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
             SLICESOFT_EXPECT_EQ(kept, std::size_t{32} + std::size_t{2048},
                 "stdout log lines are capped");
             SLICESOFT_EXPECT_EQ(result.droppedStdoutLogLines,
                 static_cast<std::uint64_t>(kLines) - kept,
                 "dropped stdout count is exact");
             SLICESOFT_EXPECT_EQ(result.stdoutLogLines.back(),
                 std::string{"ordinary line 4000"},
                 "newest stdout line is retained");
             SLICESOFT_EXPECT_EQ(result.stdoutLogLines.front(),
                 std::string{"ordinary line 1"},
                 "earliest stdout line is retained");
         }},

        // stderr 走另一条代码路径，同样受限。
        {"stderr_log_lines_are_bounded",
         []
         {
             constexpr int kLines = 4000;
             WorkerRunResult result;
             WorkerProtocolParser parser{&result, nullptr};
             for (int i = 1; i <= kLines; ++i)
             {
                 parser.ProcessStderrChunk("err " + std::to_string(i) + "\n");
             }
             parser.Finish();
             SLICESOFT_EXPECT_EQ(result.stderrLogLines.size(),
                 std::size_t{32} + std::size_t{2048},
                 "stderr log lines are capped");
             SLICESOFT_EXPECT_TRUE(result.droppedStderrLogLines > 0U,
                 "stderr drops are counted");
             SLICESOFT_EXPECT_EQ(result.stderrLogLines.back(), std::string{"err 4000"},
                 "newest stderr line is retained");
         }},
    });
}
