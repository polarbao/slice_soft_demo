#include "HostModelImportWorkflow.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>
#include <QUuid>

namespace
{
QByteArray Compact(const QJsonObject& value)
{
    return QJsonDocument(value).toJson(QJsonDocument::Compact);
}

QString ResultError(const QJsonObject& value, const QString& fallback)
{
    const QString code = value.value(QStringLiteral("code")).toString();
    const QString message = value.value(QStringLiteral("message")).toString();
    const QString detail = value.value(QStringLiteral("detail")).toString();
    QStringList parts;
    if (!code.isEmpty())
    {
        parts.append(code);
    }
    if (!message.isEmpty())
    {
        parts.append(message);
    }
    if (!detail.isEmpty())
    {
        parts.append(detail);
    }
    return parts.isEmpty() ? fallback : parts.join(QStringLiteral("："));
}

double ArrayValue(const QJsonArray& values, const int index)
{
    return index >= 0 && index < values.size()
        ? values.at(index).toDouble()
        : 0.0;
}

QJsonObject ReferenceSceneContext()
{
    return QJsonObject{
        {QStringLiteral("resolvedProfileId"),
         QStringLiteral("host-reference-default")},
        {QStringLiteral("buildVolume"), QJsonObject{
             {QStringLiteral("source"), QStringLiteral("device_profile")},
             {QStringLiteral("widthMm"), 230.0},
             {QStringLiteral("heightMm"), 100.0},
             {QStringLiteral("zLimitMm"), 60.0},
             {QStringLiteral("origin"), QStringLiteral("lower_left")},
             {QStringLiteral("xDirection"), QStringLiteral("positive")},
             {QStringLiteral("yDirection"), QStringLiteral("positive")},
             {QStringLiteral("isFixture"), false}}}};
}
}

HostModelImportWorkflow::HostModelImportWorkflow(ModuleClient& client)
    : m_client(client)
{
}

bool HostModelImportWorkflow::ImportModel(
    const QString& modelPath,
    hostmodelimportresult* result,
    QString* error)
{
    if (result == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模型导入结果目标不能为空。");
        }
        return false;
    }
    *result = {};

    const QFileInfo modelFile(modelPath);
    const QString suffix = modelFile.suffix().toLower();
    if (!modelFile.isFile()
        || (suffix != QStringLiteral("obj")
            && suffix != QStringLiteral("3mf")))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "请选择存在的 OBJ 或 3MF 模型：%1").arg(modelPath);
        }
        return false;
    }

    QJsonObject imported;
    const QString normalizedPath = QDir::fromNativeSeparators(
        modelFile.absoluteFilePath());
    const QJsonObject importRequest{
        {QStringLiteral("capability"), QStringLiteral("model.import")},
        {QStringLiteral("modelPath"), normalizedPath},
        {QStringLiteral("options"), QJsonObject{
             {QStringLiteral("computeBBox"), true},
             {QStringLiteral("extractMaterials"), true}}}};
    if (!ExecuteObject(importRequest, &imported, error))
    {
        return false;
    }

    result->sourcepath = normalizedPath;
    result->modelid = imported.value(QStringLiteral("modelId")).toString();
    result->trianglecount = static_cast<quint64>(imported.value(
        QStringLiteral("triangleCount")).toDouble());
    result->vertexcount = static_cast<quint64>(imported.value(
        QStringLiteral("vertexCount")).toDouble());
    result->hasuv = imported.value(QStringLiteral("hasUV")).toBool();
    result->hasnormals = imported.value(QStringLiteral("hasNormals")).toBool();
    const QJsonObject bounds = imported.value(QStringLiteral("bboxMm")).toObject();
    const QJsonArray minimum = bounds.value(QStringLiteral("min")).toArray();
    const QJsonArray maximum = bounds.value(QStringLiteral("max")).toArray();
    result->widthmm = ArrayValue(maximum, 0) - ArrayValue(minimum, 0);
    result->heightmm = ArrayValue(maximum, 1) - ArrayValue(minimum, 1);
    result->depthmm = ArrayValue(maximum, 2) - ArrayValue(minimum, 2);
    if (result->modelid.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("model.import 未返回 modelId。");
        }
        return false;
    }

    if (!AddInstance(result->modelid, &result->instanceid, error))
    {
        RollbackImport(result->modelid, QString{});
        return false;
    }
    if (!RunFastPreflight(result->modelid, result, error))
    {
        RollbackImport(result->modelid, result->instanceid);
        return false;
    }
    return true;
}

quint64 HostModelImportWorkflow::SceneHandle() const
{
    return m_sceneHandle;
}

quint64 HostModelImportWorkflow::SceneRevision() const
{
    return m_sceneRevision;
}

bool HostModelImportWorkflow::ExecuteObject(
    const QJsonObject& request,
    QJsonObject* response,
    QString* error)
{
    if (response == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("能力响应目标不能为空。");
        }
        return false;
    }
    QByteArray responseBytes;
    if (!m_client.Execute(Compact(request), &responseBytes, error))
    {
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        responseBytes, &parseError);
    if (!document.isObject())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("能力响应不是 JSON 对象：%1")
                         .arg(parseError.errorString());
        }
        return false;
    }
    *response = document.object();
    if (!response->value(QStringLiteral("ok")).toBool())
    {
        if (error != nullptr)
        {
            *error = ResultError(
                *response,
                QStringLiteral("切片能力模块拒绝模型导入流程。"));
        }
        return false;
    }
    return true;
}

bool HostModelImportWorkflow::AddInstance(
    const QString& modelId,
    QString* instanceId,
    QString* error)
{
    if (instanceId == nullptr)
    {
        return false;
    }
    *instanceId = QStringLiteral("instance-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    QJsonObject request{
        {QStringLiteral("capability"),
         QStringLiteral("scene.apply_operation")},
        {QStringLiteral("operationId"),
         QStringLiteral("operation-import-%1").arg(
             QUuid::createUuid().toString(QUuid::WithoutBraces))},
        {QStringLiteral("currentSceneRevision"),
         static_cast<qint64>(m_sceneRevision)},
        {QStringLiteral("expectedSceneRevision"),
         static_cast<qint64>(m_sceneRevision)},
        {QStringLiteral("operations"), QJsonArray{QJsonObject{
             {QStringLiteral("type"), QStringLiteral("addInstance")},
             {QStringLiteral("modelId"), modelId},
             {QStringLiteral("assignInstanceId"), *instanceId}}}}};
    if (m_sceneHandle == 0U)
    {
        request.insert(QStringLiteral("sceneContext"), ReferenceSceneContext());
    }
    else
    {
        request.insert(
            QStringLiteral("sceneHandle"),
            static_cast<qint64>(m_sceneHandle));
    }

    QJsonObject response;
    if (!ExecuteObject(request, &response, error))
    {
        return false;
    }
    const quint64 responseHandle = static_cast<quint64>(response.value(
        QStringLiteral("sceneHandle")).toDouble());
    if (m_sceneHandle == 0U)
    {
        m_sceneHandle = responseHandle;
    }
    const quint64 newRevision = static_cast<quint64>(response.value(
        QStringLiteral("newSceneRevision")).toDouble());
    if (m_sceneHandle == 0U || newRevision != m_sceneRevision + 1U)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "addInstance 未返回有效 sceneHandle/sceneRevision。");
        }
        return false;
    }
    m_sceneRevision = newRevision;
    return true;
}

bool HostModelImportWorkflow::RunFastPreflight(
    const QString& modelId,
    hostmodelimportresult* result,
    QString* error)
{
    QJsonObject response;
    if (!ExecuteObject(
            QJsonObject{
                {QStringLiteral("capability"),
                 QStringLiteral("geometry.preflight")},
                {QStringLiteral("mode"), QStringLiteral("fast")},
                {QStringLiteral("modelId"), modelId}},
            &response,
            error))
    {
        return false;
    }
    result->admission = response.value(QStringLiteral("admission")).toString();
    const QJsonArray issues = response.value(QStringLiteral("issues")).toArray();
    for (const QJsonValue& value : issues)
    {
        const QJsonObject issue = value.toObject();
        result->issues.append(hostpreflightissue{
            issue.value(QStringLiteral("code")).toString(),
            issue.value(QStringLiteral("severity")).toString(),
            static_cast<quint64>(issue.value(
                QStringLiteral("count")).toDouble()),
            issue.value(QStringLiteral("detail")).toString()});
    }
    if (result->admission.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("快速预检未返回 admission。");
        }
        return false;
    }
    return true;
}

void HostModelImportWorkflow::RollbackImport(
    const QString& modelId,
    const QString& instanceId)
{
    QString ignoredError;
    if (!instanceId.isEmpty() && m_sceneHandle != 0U)
    {
        QJsonObject response;
        const QJsonObject request{
            {QStringLiteral("capability"),
             QStringLiteral("scene.apply_operation")},
            {QStringLiteral("operationId"),
             QStringLiteral("operation-import-rollback-%1").arg(
                 QUuid::createUuid().toString(QUuid::WithoutBraces))},
            {QStringLiteral("sceneHandle"),
             static_cast<qint64>(m_sceneHandle)},
            {QStringLiteral("currentSceneRevision"),
             static_cast<qint64>(m_sceneRevision)},
            {QStringLiteral("expectedSceneRevision"),
             static_cast<qint64>(m_sceneRevision)},
            {QStringLiteral("operations"), QJsonArray{QJsonObject{
                 {QStringLiteral("type"), QStringLiteral("removeInstance")},
                 {QStringLiteral("instanceId"), instanceId}}}}};
        if (ExecuteObject(request, &response, &ignoredError))
        {
            m_sceneRevision = static_cast<quint64>(response.value(
                QStringLiteral("newSceneRevision")).toDouble());
        }
    }
    QJsonObject ignoredResponse;
    (void)ExecuteObject(
        QJsonObject{
            {QStringLiteral("capability"), QStringLiteral("model.release")},
            {QStringLiteral("modelId"), modelId}},
        &ignoredResponse,
        &ignoredError);
}
