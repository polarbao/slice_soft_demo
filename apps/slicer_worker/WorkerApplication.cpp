#include "WorkerApplication.h"

#include "slicer_worker/preflight/WorkerPreflightExecutor.h"
#include "slicer_worker/repair/WorkerRepairExecutor.h"
#include "slicer_worker/runtime/WorkerJobDispatcher.h"
#include "slicer_worker/runtime/WorkerJobRuntime.h"
#include "slicer_worker/slice/WorkerSliceExecutor.h"

#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>

namespace slicer_worker
{
namespace
{

constexpr std::string_view InvalidArgumentsCode{"invalid_arguments"};
constexpr std::string_view EngineVersion{"0.1.0"};

}  // namespace

int WorkerApplication::Run(const int argc, char* const argv[]) const
{
    if (argc == 2)
    {
        const std::string argument{argv[1]};
        if (argument == "--help" || argument == "-h")
        {
            PrintHelp(std::cout);
            return static_cast<int>(ExitCode::Success);
        }
        if (argument == "--contract-info")
        {
            return PrintContractInfo(std::cout);
        }
        if (argument == "--spi-request")
        {
            return PrintFailure(
                std::cerr,
                ExitCode::InvalidArguments,
                InvalidArgumentsCode,
                "--spi-request requires one absolute request JSON path");
        }
    }

    if (argc >= 2 && std::string_view{argv[1]} == "--spi-request")
    {
        return HandleSpiRequest(argc, argv);
    }

    if (argc <= 1)
    {
        PrintHelp(std::cerr);
        return PrintFailure(
            std::cerr,
            ExitCode::InvalidArguments,
            InvalidArgumentsCode,
            "no command was provided");
    }

    return PrintFailure(
        std::cerr,
        ExitCode::InvalidArguments,
        InvalidArgumentsCode,
        std::string{"unknown argument: "} + argv[1]);
}

void WorkerApplication::PrintHelp(std::ostream& output)
{
    output
        << "SliceSoft slicer_worker file-contract shell\n"
        << "Usage:\n"
        << "  slicer_worker --help\n"
        << "  slicer_worker --contract-info\n"
        << "  slicer_worker --spi-request <absolute-request-json-path>\n"
        << "\n"
        << "Implemented:\n"
        << "  --help             Print this help and exit successfully.\n"
        << "  --contract-info    Print the file_contract_v1 discovery JSON.\n"
        << "\n"
        << "Implemented fail-closed runtime:\n"
        << "  --spi-request      Parse, dispatch, and publish one file-contract result.\n"
        << "                     Slice and full-preflight executors are registered.\n"
        << "\n"
        << "Exit codes:\n"
        << "  0  Shell command completed successfully.\n"
        << "  1  Internal failure or production executor is not installed.\n"
        << "  2  Invalid input or command-line arguments.\n"
        << "  3..8  Frozen profile/topology/resource/output/contract/cancel categories.\n";
}

int WorkerApplication::PrintContractInfo(std::ostream& output)
{
    // 合同发现独占 stdout，确保调用方只需解析一个 JSON 对象。
    output
        << "{\"contract\":\"file_contract\","
        << "\"major\":1,\"minor\":0,"
        << "\"engineVersion\":\"" << EngineVersion << "\","
        << "\"produces\":[\"p0.rgbwsv.2\"],"
        << "\"capabilities\":["
        << "\"slice.rgbwsv\","
        << "\"geometry.preflight.full\","
        << "\"geometry.repair\"]}\n";
    return static_cast<int>(ExitCode::Success);
}

int WorkerApplication::PrintFailure(
    std::ostream& output,
    const ExitCode exitCode,
    const std::string_view errorCode,
    const std::string_view message)
{
    output
        << "SLICER_WORKER_ERROR code=" << errorCode
        << " message=\"" << message << "\"\n";
    return static_cast<int>(exitCode);
}

int WorkerApplication::HandleSpiRequest(
    const int argc,
    char* const argv[])
{
    if (argc != 3)
    {
        return PrintFailure(
            std::cerr,
            ExitCode::InvalidArguments,
            InvalidArgumentsCode,
            "--spi-request requires exactly one absolute request JSON path");
    }

    const std::filesystem::path requestPath{argv[2]};
    if (!requestPath.is_absolute())
    {
        return PrintFailure(
            std::cerr,
            ExitCode::InvalidArguments,
            InvalidArgumentsCode,
            "--spi-request requires an absolute request JSON path");
    }

    slicesoft::worker::WorkerJobDispatcher dispatcher;
    dispatcher.Register(
        "geometry.preflight.full",
        slicesoft::worker::CreateProductionWorkerPreflightExecutor());
    dispatcher.Register(
        "slice.rgbwsv",
        slicesoft::worker::CreateProductionWorkerSliceExecutor(std::cout));
    dispatcher.Register(
        "geometry.repair",
        slicesoft::worker::CreateProductionWorkerRepairExecutor());
    const slicesoft::worker::WorkerJobRuntimeResult result =
        slicesoft::worker::WorkerJobRuntime::Run(requestPath, dispatcher);
    if (result.processexitcode == 0)
    {
        return 0;
    }
    const std::string message = result.message.empty()
        ? "worker request failed without a diagnostic message"
        : result.message;
    return PrintFailure(
        std::cerr,
        static_cast<ExitCode>(result.processexitcode),
        result.stablecode,
        message);
}

}  // namespace slicer_worker
