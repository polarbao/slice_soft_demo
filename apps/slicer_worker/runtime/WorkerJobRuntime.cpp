#include "slicer_worker/runtime/WorkerJobRuntime.h"

#include "slicer_worker/runtime/WorkerRequestParser.h"
#include "slicer_worker/runtime/WorkerResultWriter.h"

#include <exception>
#include <utility>

namespace slicesoft::worker
{
namespace
{

WorkerJobRuntimeResult RejectUntrusted(const WorkerRequestParseError& error)
{
    WorkerJobRuntimeResult result;
    result.trustedidentity = false;
    result.resultwritten = false;
    result.message = error.what();
    if (error.Code() == WorkerRequestParseErrorCode::ContractViolation)
    {
        result.processexitcode = 7;
        result.stablecode = "PM-SLICER-CONTRACT-0060";
    }
    else
    {
        result.processexitcode = 2;
        result.stablecode = "PM-SLICER-INPUT-0002";
    }
    return result;
}

}  // namespace

WorkerJobRuntimeResult WorkerJobRuntime::Run(
    const std::filesystem::path& requestPath,
    const WorkerJobDispatcher& dispatcher) noexcept
{
    try
    {
        const WorkerRequestEnvelope request = WorkerRequestParser::Parse(requestPath);
        const WorkerResultEnvelope result = dispatcher.Dispatch(request);
        WorkerJobRuntimeResult outcome;
        outcome.trustedidentity = true;
        outcome.processexitcode = result.ProcessExitCode();
        outcome.stablecode = result.Code();
        if (!result.Ok())
        {
            outcome.message = result.ToJson().at("error").at("message").as_string();
        }
        WorkerResultWriter::WriteAtomically(result);
        outcome.resultwritten = true;
        return outcome;
    }
    catch (const WorkerRequestParseError& error)
    {
        return RejectUntrusted(error);
    }
    catch (const WorkerResultWriteError& error)
    {
        WorkerJobRuntimeResult result;
        result.processexitcode = error.ProcessExitCode();
        result.stablecode = error.StableCode();
        result.message = error.what();
        result.trustedidentity = true;
        result.resultwritten = false;
        return result;
    }
    catch (const std::exception& error)
    {
        WorkerJobRuntimeResult result;
        result.message = error.what();
        return result;
    }
    catch (...)
    {
        WorkerJobRuntimeResult result;
        result.message = "worker runtime failed with an unknown exception";
        return result;
    }
}

}  // namespace slicesoft::worker
