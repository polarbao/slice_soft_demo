#include "SceneInteractionController.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUuid>

namespace
{
QString NewOperationId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
}

SceneInteractionController::SceneInteractionController(ModuleClient& client)
    : m_client(client)
{
}

bool SceneInteractionController::Initialize(
    const QJsonObject& scene,
    QString* error)
{
    if (!m_client.IsOpen())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("切片能力模块尚未加载。");
        }
        return false;
    }

    m_externalSceneId = scene.value(QStringLiteral("sceneId")).toString();
    m_sceneRevision = static_cast<quint64>(
        scene.value(QStringLiteral("sceneRevision")).toDouble());
    if (m_externalSceneId.isEmpty() || m_sceneRevision == 0)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("场景缺少 sceneId 或有效 revision。");
        }
        return false;
    }

    QJsonObject bootstrapResult;
    if (!Bootstrap(scene, &bootstrapResult, error) ||
        !AdoptCommit(bootstrapResult, error))
    {
        return false;
    }

    // Scene bootstrap is followed by one explicit initial refresh to obtain
    // the documented sceneHandle used by all later Commit requests.
    return RefreshSnapshot(error);
}

bool SceneInteractionController::Attach(
    const quint64 sceneHandle,
    const quint64 sceneRevision)
{
    if (!m_client.IsOpen() || sceneHandle == 0U || sceneRevision == 0U)
    {
        return false;
    }
    m_transformPolicy.Reset();
    m_sceneHandle = sceneHandle;
    m_sceneRevision = sceneRevision;
    m_viewDataIdentity.clear();
    m_collisionCount = 0;
    m_outOfBoundsCount = 0;
    return true;
}

bool SceneInteractionController::BeginTransient(const QString& instanceId)
{
    return m_transformPolicy.Begin(instanceId);
}

bool SceneInteractionController::UpdateTransientTranslation(
    double deltaXMm,
    double deltaYMm,
    double deltaZMm)
{
    return m_transformPolicy.UpdateTranslation(
        deltaXMm,
        deltaYMm,
        deltaZMm);
}

CommitOutcome SceneInteractionController::CommitTransient(QString* error)
{
    const QJsonObject request = m_transformPolicy.BuildRequest(
        m_sceneHandle,
        m_sceneRevision,
        NewOperationId());
    if (request.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("没有可提交的瞬态变换。");
        }
        return CommitOutcome::Failed;
    }

    QJsonObject result;
    if (!ExecuteSync(request, &result, error))
    {
        m_transformPolicy.Reset();
        return CommitOutcome::Failed;
    }

    if (TransformCommitPolicy::IsStale(result))
    {
        m_transformPolicy.Reset();
        if (!RefreshSnapshot(error))
        {
            return CommitOutcome::Failed;
        }
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "场景版本已更新，已丢弃本地瞬态状态并恢复权威快照。");
        }
        return CommitOutcome::StaleRecovered;
    }

    if (!result.value(QStringLiteral("ok")).toBool() ||
        !AdoptCommit(result, error))
    {
        m_transformPolicy.Reset();
        if (error != nullptr && error->isEmpty())
        {
            *error = QString::fromUtf8(
                QJsonDocument(result).toJson(QJsonDocument::Compact));
        }
        return CommitOutcome::Failed;
    }

    m_transformPolicy.Reset();
    return CommitOutcome::Committed;
}

void SceneInteractionController::DiscardTransient()
{
    m_transformPolicy.Reset();
}

bool SceneInteractionController::HasTransient() const
{
    return m_transformPolicy.IsActive();
}

quint64 SceneInteractionController::SceneRevision() const
{
    return m_sceneRevision;
}

quint64 SceneInteractionController::SceneHandle() const
{
    return m_sceneHandle;
}

QString SceneInteractionController::SceneHash() const
{
    return m_sceneHash;
}

QString SceneInteractionController::ViewDataIdentity() const
{
    return m_viewDataIdentity;
}

quint64 SceneInteractionController::SnapshotReadCount() const
{
    return m_snapshotReadCount;
}

int SceneInteractionController::CollisionCount() const
{
    return m_collisionCount;
}

int SceneInteractionController::OutOfBoundsCount() const
{
    return m_outOfBoundsCount;
}

bool SceneInteractionController::Bootstrap(
    const QJsonObject& scene,
    QJsonObject* result,
    QString* error)
{
    const QJsonArray instances = scene.value(QStringLiteral("instances")).toArray();
    if (instances.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("场景至少需要一个实例。");
        }
        return false;
    }

    const QString instanceId = instances.first()
                                   .toObject()
                                   .value(QStringLiteral("instanceId"))
                                   .toString();
    QJsonObject operation;
    operation.insert(QStringLiteral("type"), QStringLiteral("translate"));
    operation.insert(QStringLiteral("instanceId"), instanceId);
    operation.insert(QStringLiteral("deltaMm"), QJsonArray{0.0, 0.0, 0.0});

    QJsonObject request;
    request.insert(
        QStringLiteral("capability"),
        QStringLiteral("scene.apply_operation"));
    request.insert(QStringLiteral("operationId"), NewOperationId());
    request.insert(QStringLiteral("scene"), scene);
    request.insert(
        QStringLiteral("currentSceneRevision"),
        static_cast<qint64>(m_sceneRevision));
    request.insert(
        QStringLiteral("expectedSceneRevision"),
        static_cast<qint64>(m_sceneRevision));
    request.insert(QStringLiteral("operations"), QJsonArray{operation});
    return ExecuteSync(request, result, error);
}

bool SceneInteractionController::RefreshSnapshot(QString* error)
{
    QJsonObject request;
    request.insert(
        QStringLiteral("capability"),
        QStringLiteral("scene.get_snapshot"));
    if (m_sceneHandle != 0)
    {
        request.insert(
            QStringLiteral("sceneHandle"),
            static_cast<qint64>(m_sceneHandle));
    }
    else
    {
        request.insert(QStringLiteral("sceneId"), m_externalSceneId);
    }

    QJsonObject result;
    if (!ExecuteSync(request, &result, error) ||
        !result.value(QStringLiteral("ok")).toBool() ||
        !AdoptSnapshot(result, error))
    {
        return false;
    }
    ++m_snapshotReadCount;
    return true;
}

bool SceneInteractionController::ExecuteSync(
    const QJsonObject& request,
    QJsonObject* result,
    QString* error)
{
    if (result == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("同步结果目标不能为空。");
        }
        return false;
    }

    const QByteArray requestBytes = QJsonDocument(request).toJson(
        QJsonDocument::Compact);
    pm_job_t* job = m_client.Submit(requestBytes, error);
    if (job == nullptr)
    {
        return false;
    }

    QByteArray progress;
    QByteArray resultBytes;
    const bool polled = m_client.Poll(job, &progress, error);
    const bool read = polled && m_client.Result(job, &resultBytes, error);
    m_client.Release(job);
    if (!read)
    {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument resultDocument = QJsonDocument::fromJson(
        resultBytes,
        &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !resultDocument.isObject())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模块返回了无效 JSON：%1")
                         .arg(parseError.errorString());
        }
        return false;
    }

    *result = resultDocument.object();
    return true;
}

bool SceneInteractionController::AdoptCommit(
    const QJsonObject& result,
    QString* error)
{
    const qint64 revision = static_cast<qint64>(result.value(
        QStringLiteral("newSceneRevision")).toDouble());
    const QString sceneHash = result.value(
        QStringLiteral("sceneHash")).toString();
    const QString viewDataIdentity = result.value(
        QStringLiteral("viewdataIdentity")).toString();
    if (revision <= 0 || sceneHash.isEmpty() || viewDataIdentity.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Commit 响应缺少 revision 或 sceneHash：%1")
                         .arg(QString::fromUtf8(
                             QJsonDocument(result).toJson(
                                 QJsonDocument::Compact)));
        }
        return false;
    }

    m_sceneRevision = static_cast<quint64>(revision);
    m_sceneHash = sceneHash;
    m_viewDataIdentity = viewDataIdentity;
    m_collisionCount = result.value(
        QStringLiteral("collisions")).toArray().size();
    m_outOfBoundsCount = result.value(
        QStringLiteral("outOfBoundsInstances")).toArray().size();
    const quint64 responseHandle = static_cast<quint64>(result.value(
        QStringLiteral("sceneHandle")).toDouble());
    if (responseHandle != 0U)
    {
        m_sceneHandle = responseHandle;
    }
    return true;
}

bool SceneInteractionController::AdoptSnapshot(
    const QJsonObject& result,
    QString* error)
{
    const quint64 responseHandle = static_cast<quint64>(result.value(
        QStringLiteral("sceneHandle")).toDouble());
    if (responseHandle != 0U)
    {
        m_sceneHandle = responseHandle;
    }
    const qint64 revision = static_cast<qint64>(result.value(
        QStringLiteral("sceneRevision")).toDouble());
    const QString sceneHash = result.value(
        QStringLiteral("sceneHash")).toString();
    if (revision <= 0 || sceneHash.isEmpty() || m_sceneHandle == 0U)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Snapshot 响应缺少权威场景身份。");
        }
        return false;
    }

    m_sceneRevision = static_cast<quint64>(revision);
    m_sceneHash = sceneHash;
    m_viewDataIdentity.clear();
    return true;
}
