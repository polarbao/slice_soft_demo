#pragma once

#include "EventPipe.h"
#include "diagnostics/LogEvent.h"

namespace slicesoft::diagnostics::transport
{
// Scoped on the request thread; contract queries create no endpoint or files.
class WorkerTelemetryScope final
{
public:
    WorkerTelemetryScope() noexcept;
    ~WorkerTelemetryScope();
    WorkerTelemetryScope(const WorkerTelemetryScope&) = delete;
    WorkerTelemetryScope& operator=(const WorkerTelemetryScope&) = delete;
    void Emit(LogEvent event) noexcept;
private:
    EventPipeClient m_client;
    WorkerTelemetryScope* m_previous{nullptr};
    std::uint64_t m_sequence{0};
};
void EmitWorkerEvent(int level, std::string_view phase, std::string_view code,
    std::string_view message, std::string_view jobId = {}) noexcept;
}
