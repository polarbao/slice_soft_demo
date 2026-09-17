#include "WorkerApplication.h"
#include "slicer_core/system/Utf8CommandLine.h"
#include "diagnostics/host/ProcessDiagnostics.h"
#include "SliceSoftBuildVersion.h"

static int Run(int argc, char* argv[])
{
    const slicer_worker::WorkerApplication application;
    if (argc < 2 || std::string_view(argv[1]) != "--spi-request") return application.Run(argc, argv);
    slicesoft::diagnostics::ProcessDiagnostics diagnostics("worker", SLICESOFT_SLICER_IMPLEMENTATION_VERSION);
    const int result = application.Run(argc, argv);
    slicesoft::diagnostics::WriteApplicationLog("worker", "exit", std::to_string(result), result == 0 ? 2 : 4);
    return result;
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) { return slicer_core::RunUtf8Main(argc, argv, Run); }
#else
int main(int argc, char** argv) { return Run(argc, argv); }
#endif
