#include "slicer_worker/runtime/WorkerJobRuntime.h"

#include "slicer_worker/runtime/WorkerRequestParser.h"
#include "slicer_worker/runtime/WorkerResultWriter.h"
#include "diagnostics/transport/WorkerTelemetry.h"

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
        diagnostics::transport::EmitWorkerEvent(2, "validated", "", "Worker request parsed and validated");
        const WorkerResultEnvelope result = dispatcher.Dispatch(request);
        diagnostics::transport::EmitWorkerEvent(result.Ok() ? 2 : 4, "executed", result.Code(), "Worker capability execution finished");
        WorkerJobRuntimeResult outcome;
        outcome.trustedidentity = true;
        outcome.processexitcode = result.ProcessExitCode();
        outcome.stablecode = result.Code();
        if (!result.Ok())
        {
            outcome.message = result.ToJson().at("error").at("message").as_string();
        }
        WorkerResultWriter::WriteAtomically(result);
        diagnostics::transport::EmitWorkerEvent(2, "published", result.Code(), "Worker result published atomically");
        outcome.resultwritten = true;
        return outcome;
    }
    catch (const WorkerRequestParseError& error)
    {
        diagnostics::transport::EmitWorkerEvent(4, "rejected", "", error.what());
        return RejectUntrusted(error);
    }
    catch (const WorkerResultWriteError& error)
    {
        diagnostics::transport::EmitWorkerEvent(4, "write_failed", error.StableCode(), error.what());
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
        diagnostics::transport::EmitWorkerEvent(4, "failed", "PM-SLICER-INTERNAL-0099", error.what());
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
