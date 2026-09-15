#include "CliProgress.h"
#include "diagnostics/host/ProcessDiagnostics.h"
#include "slicer_core/SliceRunTelemetry.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace slicer_cli
{
namespace
{
std::string FormatMilliseconds(const double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

void RecordProgress(const slicer_core::SliceRunProgress& progress) noexcept
{
    try
    {
        const auto session = slicesoft::diagnostics::ApplicationLog();
        if (!session || session->MinimumLevel() < 0 || session->MinimumLevel() > 1) return;
        std::ostringstream message;
        message << "current=" << progress.current << " total=" << progress.total
            << " percent=" << progress.percent << " elapsedMs=" << FormatMilliseconds(progress.elapsed_ms);
        slicesoft::diagnostics::WriteApplicationLog("cli_slice", progress.phase, message.str(), 1);
    }
    catch (...) {}
}
}

void PrintSliceProgress(const slicer_core::SliceRunProgress& progress)
{
    std::cout
        << "SLICE_PROGRESS"
        << " phase=" << progress.phase
        << " current=" << progress.current
        << " total=" << progress.total
        << " percent=" << progress.percent
        << " elapsedMs=" << FormatMilliseconds(progress.elapsed_ms)
        << '\n';
    std::cout.flush();
    RecordProgress(progress);
}
}
