#pragma once

namespace slicer_core
{
struct SliceRunProgress;
}

namespace slicer_cli
{
// Preserves the CLI progress stream; structured logging is optional.
void PrintSliceProgress(const slicer_core::SliceRunProgress& progress);
}
