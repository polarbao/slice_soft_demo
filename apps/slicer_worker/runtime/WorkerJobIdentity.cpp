#include "slicer_worker/runtime/WorkerJobIdentity.h"

#include <utility>

namespace slicesoft::worker
{

WorkerJobIdentity::WorkerJobIdentity(
    std::string jobId,
    std::string correlationId,
    std::string capability,
    std::filesystem::path requestPath)
    : m_jobId(std::move(jobId)),
      m_correlationId(std::move(correlationId)),
      m_capability(std::move(capability)),
      m_requestPath(std::move(requestPath)),
      m_jobDirectory(m_requestPath.parent_path().lexically_normal()),
      m_resultPath(m_jobDirectory / "result.json"),
      m_resultTemporaryPath(m_jobDirectory / "result.json.tmp"),
      m_cancelPath(m_jobDirectory / "cancel.requested")
{
}

const std::string& WorkerJobIdentity::JobId() const noexcept
{
    return m_jobId;
}

const std::string& WorkerJobIdentity::CorrelationId() const noexcept
{
    return m_correlationId;
}

const std::string& WorkerJobIdentity::Capability() const noexcept
{
    return m_capability;
}

const std::filesystem::path& WorkerJobIdentity::RequestPath() const noexcept
{
    return m_requestPath;
}

const std::filesystem::path& WorkerJobIdentity::JobDirectory() const noexcept
{
    return m_jobDirectory;
}

const std::filesystem::path& WorkerJobIdentity::ResultPath() const noexcept
{
    return m_resultPath;
}

const std::filesystem::path& WorkerJobIdentity::ResultTemporaryPath() const noexcept
{
    return m_resultTemporaryPath;
}

const std::filesystem::path& WorkerJobIdentity::CancelPath() const noexcept
{
    return m_cancelPath;
}

}  // namespace slicesoft::worker
