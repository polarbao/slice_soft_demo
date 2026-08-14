#pragma once

#include <iosfwd>
#include <string_view>

namespace slicer_worker
{

/**
 * @brief 提供 slicer_worker 的文件合同命令行外壳。
 *
 * 通过 --contract-info 发现合同。请求执行进入共享 Worker 运行时；
 * 未安装真实执行器时按失败即拒绝处理。
 */
class WorkerApplication final
{
public:
    /**
     * @brief 解析命令行并分派受支持的外壳操作。
     * @param argc 命令行参数数量。
     * @param argv 由进程运行时持有的命令行参数数组。
     * @return PrintHelp() 中约定的稳定进程退出码。
     */
    int Run(int argc, char* const argv[]) const;

private:
    enum class ExitCode : int
    {
        Success = 0,
        Internal = 1,
        InvalidArguments = 2,
        Profile = 3,
        Topology = 4,
        Resource = 5,
        Output = 6,
        Contract = 7,
        Cancelled = 8
    };

    static void PrintHelp(std::ostream& output);
    static int PrintFailure(
        std::ostream& output,
        ExitCode exitCode,
        std::string_view errorCode,
        std::string_view message);
    static int PrintContractInfo(std::ostream& output);
    static int HandleSpiRequest(int argc, char* const argv[]);
};

}  // namespace slicer_worker
