#include "CameraController.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr float kPi{3.14159265358979323846F};
constexpr float kNearMm{0.01F};
constexpr float kFarMm{10000.0F};

struct Vector3 final
{
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

float Radians(float degrees)
{
    return degrees * kPi / 180.0F;
}

Vector3 Normalize(const Vector3& value)
{
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
    return length > 1.0e-6F
        ? Vector3{value.x / length, value.y / length, value.z / length}
        : Vector3{};
}

Vector3 Cross(const Vector3& a, const Vector3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

float Dot(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

void CopyMatrix(
    const std::array<float, 16>& source,
    float destination[16])
{
    std::copy(source.begin(), source.end(), destination);
}

std::array<float, 16> LookAt(
    const Vector3& eye,
    const Vector3& target)
{
    const Vector3 forward = Normalize({
        target.x - eye.x,
        target.y - eye.y,
        target.z - eye.z});
    const Vector3 preferredUp = std::abs(forward.z) > 0.98F
        ? Vector3{0.0F, 1.0F, 0.0F}
        : Vector3{0.0F, 0.0F, 1.0F};
    const Vector3 right = Normalize(Cross(forward, preferredUp));
    const Vector3 up = Cross(right, forward);
    return {
        right.x, right.y, right.z, -Dot(right, eye),
        up.x, up.y, up.z, -Dot(up, eye),
        -forward.x, -forward.y, -forward.z, Dot(forward, eye),
        0.0F, 0.0F, 0.0F, 1.0F};
}

std::array<float, 16> Orthographic(float width, float height)
{
    return {
        2.0F / width, 0.0F, 0.0F, 0.0F,
        0.0F, 2.0F / height, 0.0F, 0.0F,
        0.0F, 0.0F, -2.0F / (kFarMm - kNearMm),
        -(kFarMm + kNearMm) / (kFarMm - kNearMm),
        0.0F, 0.0F, 0.0F, 1.0F};
}

std::array<float, 16> Perspective(float aspect)
{
    const float scale = 1.0F / std::tan(Radians(45.0F) * 0.5F);
    return {
        scale / aspect, 0.0F, 0.0F, 0.0F,
        0.0F, scale, 0.0F, 0.0F,
        0.0F, 0.0F, -(kFarMm + kNearMm) / (kFarMm - kNearMm),
        -(2.0F * kFarMm * kNearMm) / (kFarMm - kNearMm),
        0.0F, 0.0F, -1.0F, 0.0F};
}
}

CameraController::CameraController() = default;

void CameraController::Fit(
    const CameraBounds& bounds,
    const std::uint32_t viewportWidthPx,
    const std::uint32_t viewportHeightPx)
{
    m_targetX = (bounds.minX + bounds.maxX) * 0.5F;
    m_targetY = (bounds.minY + bounds.maxY) * 0.5F;
    m_targetZ = (bounds.minZ + bounds.maxZ) * 0.5F;
    const float extentX = (std::max)(bounds.maxX - bounds.minX, 1.0F);
    const float extentY = (std::max)(bounds.maxY - bounds.minY, 1.0F);
    const float extentZ = (std::max)(bounds.maxZ - bounds.minZ, 1.0F);
    m_viewportWidthPx = (std::max)(viewportWidthPx, 1U);
    m_viewportHeightPx = (std::max)(viewportHeightPx, 1U);
    const float aspect = static_cast<float>(m_viewportWidthPx)
        / static_cast<float>(m_viewportHeightPx);
    m_orthographicHeightMm = 1.25F * (std::max)(
        extentY,
        extentX / (std::max)(aspect, 0.01F));
    m_distanceMm = 2.5F * std::sqrt(
        extentX * extentX + extentY * extentY + extentZ * extentZ);
}

void CameraController::Orbit(
    const float yawDeltaDeg,
    const float pitchDeltaDeg)
{
    m_yawDeg = std::fmod(m_yawDeg + yawDeltaDeg, 360.0F);
    m_pitchDeg = std::clamp(
        m_pitchDeg + pitchDeltaDeg,
        -89.0F,
        89.0F);
}

void CameraController::Pan(const float rightMm, const float upMm)
{
    const float yaw = Radians(m_yawDeg);
    const float pitch = Radians(m_pitchDeg);
    const Vector3 fromTarget{
        std::cos(pitch) * std::cos(yaw),
        std::cos(pitch) * std::sin(yaw),
        std::sin(pitch)};
    const Vector3 forward{-fromTarget.x, -fromTarget.y, -fromTarget.z};
    const Vector3 preferredUp = std::abs(forward.z) > 0.98F
        ? Vector3{0.0F, 1.0F, 0.0F}
        : Vector3{0.0F, 0.0F, 1.0F};
    const Vector3 right = Normalize(Cross(forward, preferredUp));
    const Vector3 up = Cross(right, forward);
    m_targetX += right.x * rightMm + up.x * upMm;
    m_targetY += right.y * rightMm + up.y * upMm;
    m_targetZ += right.z * rightMm + up.z * upMm;
}

void CameraController::ZoomAtCursor(
    const float wheelSteps,
    const float normalizedX,
    const float normalizedY)
{
    const float factor = std::pow(0.90F, wheelSteps);
    const float oldHeight = m_orthographicHeightMm;
    m_orthographicHeightMm = std::clamp(
        m_orthographicHeightMm * factor,
        0.01F,
        10000.0F);
    m_distanceMm = std::clamp(m_distanceMm * factor, 0.01F, 10000.0F);
    if (m_projection == slicer::render::Projection::Orthographic)
    {
        const float aspect = static_cast<float>(m_viewportWidthPx)
            / static_cast<float>(m_viewportHeightPx);
        Pan(
            normalizedX * (oldHeight - m_orthographicHeightMm) * aspect * 0.5F,
            normalizedY * (oldHeight - m_orthographicHeightMm) * 0.5F);
    }
}

void CameraController::SetPreset(const CameraPreset preset)
{
    switch (preset)
    {
    case CameraPreset::Top:
        m_yawDeg = -90.0F;
        m_pitchDeg = 89.0F;
        break;
    case CameraPreset::Bottom:
        m_yawDeg = -90.0F;
        m_pitchDeg = -89.0F;
        break;
    case CameraPreset::Front:
        m_yawDeg = -90.0F;
        m_pitchDeg = 0.0F;
        break;
    case CameraPreset::Back:
        m_yawDeg = 90.0F;
        m_pitchDeg = 0.0F;
        break;
    case CameraPreset::Left:
        m_yawDeg = 180.0F;
        m_pitchDeg = 0.0F;
        break;
    case CameraPreset::Right:
        m_yawDeg = 0.0F;
        m_pitchDeg = 0.0F;
        break;
    case CameraPreset::Isometric:
        m_yawDeg = -45.0F;
        m_pitchDeg = 35.264F;
        break;
    }
}

void CameraController::SetProjection(
    const slicer::render::Projection projection)
{
    m_projection = projection;
}

slicer::render::Projection CameraController::ProjectionMode() const
{
    return m_projection;
}

slicer::render::CameraDesc CameraController::BuildCamera() const
{
    const float yaw = Radians(m_yawDeg);
    const float pitch = Radians(m_pitchDeg);
    const Vector3 target{m_targetX, m_targetY, m_targetZ};
    const Vector3 eye{
        target.x + m_distanceMm * std::cos(pitch) * std::cos(yaw),
        target.y + m_distanceMm * std::cos(pitch) * std::sin(yaw),
        target.z + m_distanceMm * std::sin(pitch)};
    const float aspect = static_cast<float>(m_viewportWidthPx)
        / static_cast<float>(m_viewportHeightPx);
    slicer::render::CameraDesc result;
    result.projection = m_projection;
    CopyMatrix(LookAt(eye, target), result.viewMatrix);
    const std::array<float, 16> projection =
        m_projection == slicer::render::Projection::Orthographic
        ? Orthographic(m_orthographicHeightMm * aspect, m_orthographicHeightMm)
        : Perspective(aspect);
    CopyMatrix(projection, result.projMatrix);
    return result;
}
