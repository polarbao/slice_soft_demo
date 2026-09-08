#include "WorkerApplication.h"
#include "slicer_core/system/Utf8CommandLine.h"

static int Run(int argc, char* argv[])
{
    const slicer_worker::WorkerApplication application;
    return application.Run(argc, argv);
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) { return slicer_core::RunUtf8Main(argc, argv, Run); }
#else
int main(int argc, char** argv) { return Run(argc, argv); }
#endif
