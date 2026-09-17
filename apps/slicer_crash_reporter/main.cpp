#include "diagnostics/windows/CrashCapture.h"

int wmain(int argc, wchar_t** argv) {
    return slicesoft::diagnostics::RunCrashReporter(argc, argv);
}
