#include "TransformCommitPolicy.h"

#include <QJsonArray>

#include <cmath>

TransformCommitPolicy::TransformCommitPolicy() = default;

bool TransformCommitPolicy::Begin(const QString& instanceId)
{
    if (instanceId.trimmed().isEmpty())
    {
        return false;
    }

    m_instanceId = instanceId;
    m_deltaXMm = 0.0;
    m_deltaYMm = 0.0;
    m_deltaZMm = 0.0;
    m_active = true;
    return true;
}

bool TransformCommitPolicy::UpdateTranslation(
    double deltaXMm,
    double deltaYMm,
    double deltaZMm)
{
    if (!m_active ||
        !std::isfinite(deltaXMm) ||
        !std::isfinite(deltaYMm) ||
        !std::isfinite(deltaZMm))
    {
        return false;
    }

    m_deltaXMm = deltaXMm;
    m_deltaYMm = deltaYMm;
    m_deltaZMm = deltaZMm;
    return true;
}

void TransformCommitPolicy::Reset()
{
    m_instanceId.clear();
    m_deltaXMm = 0.0;
    m_deltaYMm = 0.0;
    m_deltaZMm = 0.0;
    m_active = false;
}

bool TransformCommitPolicy::IsActive() const
{
    return m_active;
}

QJsonObject TransformCommitPolicy::BuildRequest(
    quint64 sceneHandle,
    quint64 sceneRevision,
    const QString& operationId) const
{
    if (!m_active || operationId.trimmed().isEmpty())
    {
        return {};
    }

    QJsonObject operation;
    operation.insert(QStringLiteral("type"), QStringLiteral("translate"));
    operation.insert(QStringLiteral("instanceId"), m_instanceId);
    operation.insert(
        QStringLiteral("deltaMm"),
        QJsonArray{m_deltaXMm, m_deltaYMm, m_deltaZMm});

    QJsonObject request;
    request.insert(
        QStringLiteral("capability"),
        QStringLiteral("scene.apply_operation"));
    request.insert(QStringLiteral("operationId"), operationId);
    request.insert(
        QStringLiteral("sceneHandle"),
        static_cast<qint64>(sceneHandle));
    request.insert(
        QStringLiteral("currentSceneRevision"),
        static_cast<qint64>(sceneRevision));
    request.insert(
        QStringLiteral("expectedSceneRevision"),
        static_cast<qint64>(sceneRevision));
    request.insert(QStringLiteral("operations"), QJsonArray{operation});
    return request;
}

bool TransformCommitPolicy::IsStale(const QJsonObject& response)
{
    return !response.value(QStringLiteral("ok")).toBool() &&
        response.value(QStringLiteral("code")).toString() ==
            QStringLiteral("PM-SLICER-LAYOUT-0022");
}
