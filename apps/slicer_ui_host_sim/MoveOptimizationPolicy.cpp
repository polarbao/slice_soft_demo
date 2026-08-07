#include "MoveOptimizationPolicy.h"

#include <cmath>

bool MoveOptimizationPolicy::Begin(
    const TopViewFrame& frame,
    const QString& instanceId)
{
    if (instanceId.isEmpty() || frame.instances.isEmpty())
    {
        return false;
    }
    for (int index = 0; index < frame.instances.size(); ++index)
    {
        if (frame.instances.at(index).instanceId == instanceId)
        {
            m_baseline = frame;
            m_preview = frame;
            m_instanceId = instanceId;
            m_instanceIndex = index;
            m_baselineMatrix = frame.instances.at(index).worldMatrix;
            m_active = true;
            return true;
        }
    }
    return false;
}

bool MoveOptimizationPolicy::UpdateTranslation(
    double deltaXMm,
    double deltaYMm,
    double deltaZMm)
{
    if (!m_active
        || !std::isfinite(deltaXMm)
        || !std::isfinite(deltaYMm)
        || !std::isfinite(deltaZMm))
    {
        return false;
    }
    auto& matrix = m_preview.instances[m_instanceIndex].worldMatrix;
    matrix = m_baselineMatrix;
    matrix.at(3) += deltaXMm;
    matrix.at(7) += deltaYMm;
    matrix.at(11) += deltaZMm;
    ++m_preview.localRevision;
    return true;
}

bool MoveOptimizationPolicy::AcceptCommit(
    quint64 sceneRevision,
    const QString& viewDataIdentity)
{
    if (!m_active || sceneRevision == 0 || viewDataIdentity.isEmpty())
    {
        return false;
    }
    m_preview.sceneRevision = sceneRevision;
    m_preview.viewDataIdentity = viewDataIdentity;
    m_preview.localRevision = 0;
    m_baseline = m_preview;
    m_active = false;
    m_instanceId.clear();
    m_instanceIndex = -1;
    return true;
}

void MoveOptimizationPolicy::Rollback()
{
    m_preview = m_baseline;
    m_active = false;
    m_instanceId.clear();
    m_instanceIndex = -1;
}

bool MoveOptimizationPolicy::IsActive() const
{
    return m_active;
}

const TopViewFrame& MoveOptimizationPolicy::Frame() const
{
    return m_preview;
}
