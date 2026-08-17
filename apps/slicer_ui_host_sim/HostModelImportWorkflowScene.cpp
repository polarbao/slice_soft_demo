#include "HostModelImportWorkflow.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

#include <cmath>

namespace
{
bool IsZero(const double value)
{
    return std::abs(value) <= 1.0e-12;
}

bool IsFinitePositive(const double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool BuildVolumesEqual(
    const hostbuildvolume& left,
    const hostbuildvolume& right)
{
    constexpr double epsilon = 1.0e-9;
    return std::abs(left.widthmm - right.widthmm) <= epsilon
        && std::abs(left.heightmm - right.heightmm) <= epsilon
        && std::abs(left.zlimitmm - right.zlimitmm) <= epsilon
        && left.origin == right.origin
        && left.xdirection == right.xdirection
        && left.ydirection == right.ydirection;
}

void AppendInstanceOperations(
    const QString& instanceId,
    const hosttransformrequest& request,
    QJsonArray* operations)
{
    if (!IsZero(request.deltaxmm)
        || !IsZero(request.deltaymm))
    {
        operations->append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("translate")},
            {QStringLiteral("instanceId"), instanceId},
            {QStringLiteral("deltaMm"), QJsonArray{
                 request.deltaxmm,
                 request.deltaymm,
                 0.0}}});
    }
    if (!IsZero(request.rotatexdegrees))
    {
        operations->append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("rotateX")},
            {QStringLiteral("instanceId"), instanceId},
            {QStringLiteral("degrees"), request.rotatexdegrees}});
    }
    if (!IsZero(request.rotateydegrees))
    {
        operations->append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("rotateY")},
            {QStringLiteral("instanceId"), instanceId},
            {QStringLiteral("degrees"), request.rotateydegrees}});
    }
    if (!IsZero(request.rotatezdegrees))
    {
        operations->append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("rotateZ")},
            {QStringLiteral("instanceId"), instanceId},
            {QStringLiteral("degrees"), request.rotatezdegrees}});
    }
    if (!IsZero(request.uniformscalefactor - 1.0))
    {
        operations->append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("uniformScale")},
            {QStringLiteral("instanceId"), instanceId},
            {QStringLiteral("factor"), request.uniformscalefactor}});
    }
    if (request.mirrorx)
    {
        operations->append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("mirror")},
            {QStringLiteral("instanceId"), instanceId},
            {QStringLiteral("axis"), QStringLiteral("x")}});
    }
    if (request.mirrory)
    {
        operations->append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("mirror")},
            {QStringLiteral("instanceId"), instanceId},
            {QStringLiteral("axis"), QStringLiteral("y")}});
    }
    if (request.landonbuildplate)
    {
        operations->append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("landOnBuildPlate")},
            {QStringLiteral("instanceId"), instanceId}});
    }
}
}

bool HostModelImportWorkflow::SetPendingSceneContext(
    const QString& profileId,
    const hostbuildvolume& buildVolume,
    QString* error)
{
    const QString normalizedProfile = profileId.trimmed();
    if (normalizedProfile.isEmpty()
        || !IsFinitePositive(buildVolume.widthmm)
        || !IsFinitePositive(buildVolume.heightmm)
        || !IsFinitePositive(buildVolume.zlimitmm)
        || buildVolume.origin != QStringLiteral("lower_left")
        || buildVolume.xdirection != QStringLiteral("positive")
        || buildVolume.ydirection != QStringLiteral("positive"))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "场景 Profile 和宿主设备 buildVolume 必须完整有效。");
        }
        return false;
    }
    if (m_sceneHandle != 0U
        && (normalizedProfile != m_sceneProfileId
            || !BuildVolumesEqual(buildVolume, m_sceneBuildVolume)))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "当前场景已绑定 Profile/buildVolume；请新建场景后修改。");
        }
        return false;
    }
    m_pendingProfileId = normalizedProfile;
    m_pendingBuildVolume = buildVolume;
    return true;
}

QString HostModelImportWorkflow::SceneProfileId() const
{
    return m_sceneHandle == 0U ? m_pendingProfileId : m_sceneProfileId;
}

hostbuildvolume HostModelImportWorkflow::SceneBuildVolume() const
{
    return m_sceneHandle == 0U
        ? m_pendingBuildVolume : m_sceneBuildVolume;
}

QString HostModelImportWorkflow::ReferenceModelPath() const
{
    QStringList paths = m_instanceSources.values();
    paths.removeDuplicates();
    paths.sort();
    return paths.isEmpty() ? QString{} : paths.first();
}

QStringList HostModelImportWorkflow::TexturePaths() const
{
    QStringList paths;
    for (const QStringList& instancePaths : m_instanceTexturePaths)
    {
        for (const QString& path : instancePaths)
        {
            if (!path.isEmpty() && !paths.contains(path))
            {
                paths.append(path);
            }
        }
    }
    paths.sort();
    return paths;
}

bool HostModelImportWorkflow::RequiresSingleMaterialProcess() const
{
    return !m_instanceMaterialRestrictions.isEmpty();
}

QString HostModelImportWorkflow::SingleMaterialRestrictionSummary() const
{
    QStringList reasons = m_instanceMaterialRestrictions.values();
    reasons.removeAll(QString{});
    reasons.removeDuplicates();
    reasons.sort();
    return reasons.join(QStringLiteral("；"));
}

bool HostModelImportWorkflow::AdoptSceneState(
    const quint64 sceneHandle,
    const quint64 sceneRevision,
    QString* error)
{
    if (sceneHandle == 0U || sceneHandle != m_sceneHandle
        || sceneRevision < m_sceneRevision)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "拖拽 Commit 返回了不匹配或倒退的权威场景身份。");
        }
        return false;
    }
    m_sceneRevision = sceneRevision;
    return true;
}

bool HostModelImportWorkflow::ApplyTransforms(
    const QStringList& instanceIds,
    const hosttransformrequest& request,
    hostsceneeditresult* result,
    QString* error)
{
    if (!std::isfinite(request.deltaxmm)
        || !std::isfinite(request.deltaymm)
        || !std::isfinite(request.rotatexdegrees)
        || !std::isfinite(request.rotateydegrees)
        || !std::isfinite(request.rotatezdegrees)
        || !std::isfinite(request.uniformscalefactor)
        || request.uniformscalefactor <= 0.0)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("实例变换参数必须为有限值且缩放因子大于零。");
        }
        return false;
    }

    QStringList uniqueIds;
    for (const QString& instanceId : instanceIds)
    {
        if (!instanceId.isEmpty() && !uniqueIds.contains(instanceId))
        {
            uniqueIds.append(instanceId);
        }
    }
    for (const QString& instanceId : uniqueIds)
    {
        if (!m_instanceModels.contains(instanceId))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("当前场景不存在实例：%1")
                             .arg(instanceId);
            }
            return false;
        }
    }

    QJsonArray operations;
    for (const QString& instanceId : uniqueIds)
    {
        AppendInstanceOperations(instanceId, request, &operations);
    }
    if (operations.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("实例变换没有产生可提交的变化。");
        }
        return false;
    }
    return CommitSceneOperations(operations, result, error);
}

bool HostModelImportWorkflow::LandOnBuildPlate(
    const QStringList& instanceIds,
    hostsceneeditresult* result,
    QString* error)
{
    QStringList uniqueIds;
    for (const QString& instanceId : instanceIds)
    {
        if (!instanceId.isEmpty() && !uniqueIds.contains(instanceId))
        {
            uniqueIds.append(instanceId);
        }
    }
    QJsonArray operations;
    for (const QString& instanceId : uniqueIds)
    {
        if (!m_instanceModels.contains(instanceId))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("当前场景不存在实例：%1")
                             .arg(instanceId);
            }
            return false;
        }
        operations.append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("landOnBuildPlate")},
            {QStringLiteral("instanceId"), instanceId}});
    }
    if (operations.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("请先选择需要触底的模型实例。");
        }
        return false;
    }
    return CommitSceneOperations(operations, result, error);
}

bool HostModelImportWorkflow::ApplyGridLayout(
    const hostgridlayoutrequest& request,
    hostsceneeditresult* result,
    QString* error)
{
    if (m_instanceModels.isEmpty()
        || request.maxcolumns < 1 || request.maxcolumns > 11
        || request.maxrows < 1 || request.maxrows > 2
        || !std::isfinite(request.columngapmm)
        || !std::isfinite(request.rowgapmm)
        || request.columngapmm < 0.0 || request.rowgapmm < 0.0)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("规则排版需要有效场景、1..11 列、1..2 行和非负净距。");
        }
        return false;
    }
    if (m_instanceModels.size() > request.maxcolumns * request.maxrows)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("规则排版容量不足：当前 %1 个实例，容量 %2。")
                         .arg(m_instanceModels.size())
                         .arg(request.maxcolumns * request.maxrows);
        }
        return false;
    }

    const QJsonObject layout{
        {QStringLiteral("policy"), QStringLiteral("grid")},
        {QStringLiteral("maxColumns"), request.maxcolumns},
        {QStringLiteral("maxRows"), request.maxrows},
        {QStringLiteral("columnGapMm"), request.columngapmm},
        {QStringLiteral("rowGapMm"), request.rowgapmm},
        {QStringLiteral("spacingMode"), QStringLiteral("edge_clearance")},
        {QStringLiteral("order"), QStringLiteral("row_major")}};
    return CommitSceneOperations(
        QJsonArray{QJsonObject{
            {QStringLiteral("type"), QStringLiteral("applyGridLayout")},
            {QStringLiteral("layout"), layout}}},
        result,
        error);
}

bool HostModelImportWorkflow::CommitSceneOperations(
    const QJsonArray& operations,
    hostsceneeditresult* result,
    QString* error)
{
    if (result == nullptr || m_sceneHandle == 0U || operations.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("场景 Commit 缺少结果目标、场景会话或操作。");
        }
        return false;
    }
    *result = {};
    const QJsonObject request{
        {QStringLiteral("capability"),
         QStringLiteral("scene.apply_operation")},
        {QStringLiteral("operationId"),
         QStringLiteral("operation-edit-%1").arg(
             QUuid::createUuid().toString(QUuid::WithoutBraces))},
        {QStringLiteral("sceneHandle"), static_cast<qint64>(m_sceneHandle)},
        {QStringLiteral("currentSceneRevision"),
         static_cast<qint64>(m_sceneRevision)},
        {QStringLiteral("expectedSceneRevision"),
         static_cast<qint64>(m_sceneRevision)},
        {QStringLiteral("operations"), operations}};
    QJsonObject response;
    if (!ExecuteObject(request, &response, error))
    {
        return false;
    }

    const quint64 newRevision = static_cast<quint64>(response.value(
        QStringLiteral("newSceneRevision")).toDouble());
    if (newRevision != m_sceneRevision + 1U)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("场景 Commit 未返回连续 sceneRevision。");
        }
        return false;
    }
    m_sceneRevision = newRevision;
    result->scenerevision = newRevision;
    result->scenehash = response.value(QStringLiteral("sceneHash")).toString();
    result->viewdataidentity = response.value(
        QStringLiteral("viewdataIdentity")).toString();
    result->collisioncount = response.value(
        QStringLiteral("collisions")).toArray().size();
    result->outofboundscount = response.value(
        QStringLiteral("outOfBoundsInstances")).toArray().size();
    return true;
}
