#pragma once

#include <iosfwd>
#include <string_view>

namespace slicer_worker
{

/**
 * @brief Provides the Stage 14D-01 command-line shell for slicer_worker.
 *
 * The shell deliberately does not execute file-contract jobs. Contract
 * negotiation and request execution are added by later Stage 14D tasks.
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
        NotImplemented = 1,
        InvalidArguments = 2
    };

    static void PrintHelp(std::ostream& output);
    static int PrintFailure(
        std::ostream& output,
        ExitCode exitCode,
        std::string_view errorCode,
        std::string_view message);
    static int HandleSpiRequest(int argc, char* const argv[]);
};

}  // namespace slicer_worker
