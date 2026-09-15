#pragma once
#include "diagnostics/host/LogSession.h"
namespace slicesoft::diagnostics
{
HostLogSink CreateFileLogSink(const LogSessionOptions& options);
}
