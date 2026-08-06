#include "slicer_worker/runtime/WorkerRequestEnvelope.h"

#include <utility>

namespace slicesoft::worker
{

WorkerRequestEnvelope::WorkerRequestEnvelope(
    WorkerJobIdentity identity,
    const std::uint32_t major,
    const std::uint32_t minor,
    const std::chrono::milliseconds timeout,
    std::optional<std::string> sceneHash,
    slicer_core::Json scene,
    slicer_core::Json profile,
    slicer_core::Json input,
    slicer_core::Json output)
    : m_identity(std::move(identity)),
      m_major(major),
      m_minor(minor),
      m_timeout(timeout),
      m_sceneHash(std::move(sceneHash)),
      m_scene(std::move(scene)),
      m_profile(std::move(profile)),
      m_input(std::move(input)),
      m_output(std::move(output))
{
}

const WorkerJobIdentity& WorkerRequestEnvelope::Identity() const noexcept
{
    return m_identity;
}

std::uint32_t WorkerRequestEnvelope::Major() const noexcept
{
    return m_major;
}

std::uint32_t WorkerRequestEnvelope::Minor() const noexcept
{
    return m_minor;
}

std::chrono::milliseconds WorkerRequestEnvelope::Timeout() const noexcept
{
    return m_timeout;
}

const std::optional<std::string>& WorkerRequestEnvelope::SceneHash() const noexcept
{
    return m_sceneHash;
}

bool WorkerRequestEnvelope::HasScene() const noexcept
{
    return m_scene.is_object();
}

const slicer_core::Json& WorkerRequestEnvelope::Scene() const noexcept
{
    return m_scene;
}

bool WorkerRequestEnvelope::HasProfile() const noexcept
{
    return m_profile.is_object();
}

const slicer_core::Json& WorkerRequestEnvelope::Profile() const noexcept
{
    return m_profile;
}

bool WorkerRequestEnvelope::HasInput() const noexcept
{
    return m_input.is_object();
}

const slicer_core::Json& WorkerRequestEnvelope::Input() const noexcept
{
    return m_input;
}

bool WorkerRequestEnvelope::HasOutput() const noexcept
{
    return m_output.is_object();
}

const slicer_core::Json& WorkerRequestEnvelope::Output() const noexcept
{
    return m_output;
}

}  // namespace slicesoft::worker
