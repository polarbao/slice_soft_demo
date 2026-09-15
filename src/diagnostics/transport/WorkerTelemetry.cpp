#include "WorkerTelemetry.h"
#include <utility>

namespace slicesoft::diagnostics::transport
{
namespace { thread_local WorkerTelemetryScope* currentTelemetry{nullptr}; }

WorkerTelemetryScope::WorkerTelemetryScope() noexcept
    : m_previous{currentTelemetry}
{
    (void)ConnectFromWorkerEnvironment(m_client);
    currentTelemetry = this;
}
WorkerTelemetryScope::~WorkerTelemetryScope() { currentTelemetry = m_previous; }

void WorkerTelemetryScope::Emit(LogEvent event) noexcept
{
    event.sequence = ++m_sequence;
    (void)m_client.Send(EncodeEvent(event));
}

void EmitWorkerEvent(const int level, const std::string_view phase,
    const std::string_view code, const std::string_view message, const std::string_view jobId) noexcept
{
    if (currentTelemetry == nullptr) return;
    try
    {
        LogEvent event;
        event.domain = "slicer.module";
        event.module = "slicer_worker";
        event.sourceProcessRole = "worker";
        event.level = level;
        event.action = "request";
        event.phase = phase.substr(0, 128);
        event.code = code.substr(0, 128);
        event.message = message.substr(0, 2048);
        event.jobId = jobId.substr(0, 128);
        currentTelemetry->Emit(std::move(event));
    }
    catch (...) {}
}
}
