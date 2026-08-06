#pragma once

#include <iosfwd>
#include <string_view>

namespace slicer_worker
{

/**
 * @brief Provides the file-contract command-line shell for slicer_worker.
 *
 * Contract discovery is available through --contract-info. Request execution
 * enters the shared Worker runtime and fails closed when no real executor is installed.
 */
class WorkerApplication final
{
public:
    /**
     * @brief Parses the command line and dispatches the supported shell action.
     * @param argc Number of command-line arguments.
     * @param argv Command-line argument array owned by the process runtime.
     * @return Stable process exit code documented by PrintHelp().
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
