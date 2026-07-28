#include "SceneBatchImportController.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace
{

QString SanitizeIdentityPart(const QString& value)
{
    QString sanitized;
    sanitized.reserve(value.size());
    for (const QChar character : value)
    {
        sanitized.append(
            character.isLetterOrNumber()
                    || character == QLatin1Char('-')
                    || character == QLatin1Char('_')
                ? character
                : QLatin1Char('_'));
    }
    while (sanitized.contains(QStringLiteral("__")))
    {
        sanitized.replace(QStringLiteral("__"), QStringLiteral("_"));
    }
    sanitized = sanitized.trimmed();
    return sanitized.isEmpty()
        ? QStringLiteral("model")
        : sanitized;
}

SceneBatchImportStartResult StartFailure(
    const SceneBatchImportStartErrorCode code,
    const QString& path,
    const QString& message)
{
    SceneBatchImportStartResult result;
    result.error = SceneBatchImportStartError{
        code,
        path,
        message,
    };
    return result;
}

bool IsSupportedModelPath(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("obj")
        || suffix == QStringLiteral("stl")
        || suffix == QStringLiteral("3mf");
}

}  // namespace

bool SceneBatchImportStartResult::IsValid() const
{
    return started && !error.has_value();
}

std::string_view SceneBatchImportStartErrorCodeName(
    const SceneBatchImportStartErrorCode code)
{
    switch (code)
    {
    case SceneBatchImportStartErrorCode::None:
        return "NONE";
    case SceneBatchImportStartErrorCode::Busy:
        return "SCENE_BATCH_IMPORT_BUSY";
    case SceneBatchImportStartErrorCode::EmptySelection:
        return "SCENE_BATCH_IMPORT_EMPTY_SELECTION";
    case SceneBatchImportStartErrorCode::SceneNotReady:
        return "SCENE_BATCH_IMPORT_SCENE_NOT_READY";
    case SceneBatchImportStartErrorCode::CapacityExceeded:
        return "SCENE_BATCH_IMPORT_CAPACITY_EXCEEDED";
    case SceneBatchImportStartErrorCode::UnsupportedFile:
        return "SCENE_BATCH_IMPORT_FILE_UNSUPPORTED";
    case SceneBatchImportStartErrorCode::LoaderUnavailable:
        return "SCENE_BATCH_IMPORT_LOADER_UNAVAILABLE";
    }
    return "SCENE_BATCH_IMPORT_UNKNOWN";
}

SceneBatchImportController::SceneBatchImportController(
    SceneDocument* document,
    QObject* parent)
    : QObject(parent),
      m_document(document)
{
    if (m_document == nullptr)
    {
        throw std::invalid_argument(
            "SceneBatchImportController requires a scene document");
    }
}

void SceneBatchImportController::SetLoadHandlers(
    LoadRequestHandler loadHandler,
    CancelHandler cancelHandler)
{
    m_loadHandler = std::move(loadHandler);
    m_cancelHandler = std::move(cancelHandler);
}

SceneBatchImportStartResult SceneBatchImportController::Start(
    const SceneBatchImportRequest& request)
{
    SceneBatchImportRequest normalized;
    const SceneBatchImportStartResult validation =
        ValidateAndNormalize(request, &normalized);
    if (!validation.IsValid())
    {
        return validation;
    }

    m_request = std::move(normalized);
    m_summary = {};
    m_summary.batchid = m_request.batchid;
    m_summary.selected = m_request.files.size();
    m_summary.autolayoutrequested = m_request.autolayout;
    m_summary.items.reserve(
        static_cast<std::size_t>(m_summary.selected));
    m_nextFileIndex = 0U;
    m_expectedLoaderGeneration = 0U;
    m_currentPath.clear();
    m_currentModelId.clear();
    m_currentInstanceId.clear();
    m_cancelRequested = false;
    m_running = true;
    emit SigStateChanged();
    StartNext();
    return {true, std::nullopt};
}

void SceneBatchImportController::Cancel()
{
    if (!m_running)
    {
        return;
    }

    m_cancelRequested = true;
    if (m_cancelHandler)
    {
        m_cancelHandler();
    }

    if (!m_currentPath.isEmpty())
    {
        RecordCurrent(
            SceneBatchImportItemStatus::Cancelled,
            QStringLiteral("SCENE_BATCH_IMPORT_CANCELLED"),
            QStringLiteral("用户取消了当前模型导入。"));
        ++m_nextFileIndex;
    }
    while (m_nextFileIndex
           < static_cast<std::size_t>(m_request.files.size()))
    {
        m_currentPath =
            m_request.files.at(static_cast<int>(m_nextFileIndex));
        m_currentModelId.clear();
        m_currentInstanceId.clear();
        RecordCurrent(
            SceneBatchImportItemStatus::Cancelled,
            QStringLiteral("SCENE_BATCH_IMPORT_CANCELLED"),
            QStringLiteral("模型尚未开始加载，批次已取消。"));
        ++m_nextFileIndex;
    }
    Finish();
}

void SceneBatchImportController::OnLoadFinished(
    const quint64 loaderGeneration)
{
    if (!m_running
        || m_cancelRequested
        || loaderGeneration != m_expectedLoaderGeneration)
    {
        return;
    }

    if (m_document->InstanceCount() > m_currentCountBefore)
    {
        RecordCurrent(
            SceneBatchImportItemStatus::Imported,
            {},
            QStringLiteral("模型已导入。"));
    }
    else if (m_document->State() == SceneDocumentState::Cancelled)
    {
        RecordCurrent(
            SceneBatchImportItemStatus::Cancelled,
            QStringLiteral("SCENE_BATCH_IMPORT_CANCELLED"),
            QStringLiteral("模型导入已取消。"));
    }
    else
    {
        RecordCurrent(
            SceneBatchImportItemStatus::Failed,
            QStringLiteral("SCENE_BATCH_IMPORT_ITEM_FAILED"),
            m_document->Error().isEmpty()
                ? QStringLiteral("模型导入失败，加载器未返回详细原因。")
                : m_document->Error());
    }

    ++m_nextFileIndex;
    m_currentPath.clear();
    m_currentModelId.clear();
    m_currentInstanceId.clear();
    StartNext();
}

bool SceneBatchImportController::IsRunning() const
{
    return m_running;
}

const SceneBatchImportSummary&
SceneBatchImportController::Summary() const
{
    return m_summary;
}

QString SceneBatchImportController::StatusText() const
{
    if (m_running)
    {
        return QStringLiteral("批量导入 %1/%2：%3")
            .arg(
                static_cast<int>(m_nextFileIndex) + 1,
                m_summary.selected)
            .arg(QFileInfo(m_currentPath).fileName());
    }
    if (m_summary.selected == 0)
    {
        return QStringLiteral("尚未开始批量导入。");
    }
    return QStringLiteral(
               "批量导入完成：选择 %1，成功 %2，失败 %3，取消 %4，"
               "自动排版 %5。")
        .arg(m_summary.selected)
        .arg(m_summary.imported)
        .arg(m_summary.failed)
        .arg(m_summary.cancelled)
        .arg(
            m_summary.autolayoutapplied
                ? QStringLiteral("已执行")
                : m_summary.layouterror.isEmpty()
                ? QStringLiteral("未执行")
                : QStringLiteral("失败"));
}

SceneBatchImportStartResult
SceneBatchImportController::ValidateAndNormalize(
    const SceneBatchImportRequest& request,
    SceneBatchImportRequest* normalized) const
{
    if (m_running)
    {
        return StartFailure(
            SceneBatchImportStartErrorCode::Busy,
            {},
            QStringLiteral("已有模型批量导入正在运行。"));
    }
    if (!m_loadHandler)
    {
        return StartFailure(
            SceneBatchImportStartErrorCode::LoaderUnavailable,
            {},
            QStringLiteral("模型加载器尚未绑定。"));
    }
    if (request.files.isEmpty())
    {
        return StartFailure(
            SceneBatchImportStartErrorCode::EmptySelection,
            {},
            QStringLiteral("没有选择模型文件。"));
    }
    if (m_document->State() == SceneDocumentState::Loading
        || (m_document->InstanceCount() > 0U
            && m_document->IsGeometryStale()))
    {
        return StartFailure(
            SceneBatchImportStartErrorCode::SceneNotReady,
            {},
            QStringLiteral(
                "场景正在加载或重投影，请等待俯视更新完成。"));
    }

    const std::size_t selected =
        static_cast<std::size_t>(request.files.size());
    if (selected
        > kMaximumInstanceCount - m_document->InstanceCount())
    {
        return StartFailure(
            SceneBatchImportStartErrorCode::CapacityExceeded,
            {},
            QStringLiteral(
                "场景剩余容量为 %1，本次选择 %2 个模型；"
                "超过 22 个实例上限，未导入任何文件。")
                .arg(
                    static_cast<qulonglong>(
                        kMaximumInstanceCount
                        - m_document->InstanceCount()))
                .arg(request.files.size()));
    }

    *normalized = request;
    normalized->batchid = SanitizeIdentityPart(
        request.batchid.trimmed().isEmpty()
            ? QStringLiteral("batch")
            : request.batchid);
    normalized->configpath =
        request.configpath.trimmed().isEmpty()
        ? QString{}
        : QFileInfo(request.configpath).absoluteFilePath();
    normalized->files.clear();

    for (const QString& selectedPath : request.files)
    {
        if (selectedPath.trimmed().isEmpty())
        {
            return StartFailure(
                SceneBatchImportStartErrorCode::UnsupportedFile,
                selectedPath,
                QStringLiteral("模型路径为空。"));
        }
        const QString absolutePath =
            QFileInfo(selectedPath).absoluteFilePath();
        if (!IsSupportedModelPath(absolutePath))
        {
            return StartFailure(
                SceneBatchImportStartErrorCode::UnsupportedFile,
                absolutePath,
                QStringLiteral(
                    "仅支持 OBJ、STL 和 3MF 模型文件。"));
        }
        normalized->files.push_back(
            QDir::fromNativeSeparators(absolutePath));
    }
    return {true, std::nullopt};
}

void SceneBatchImportController::StartNext()
{
    if (!m_running || m_cancelRequested)
    {
        return;
    }
    if (m_nextFileIndex
        >= static_cast<std::size_t>(m_request.files.size()))
    {
        Finish();
        return;
    }

    m_currentPath =
        m_request.files.at(static_cast<int>(m_nextFileIndex));
    const QString baseName = SanitizeIdentityPart(
        QFileInfo(m_currentPath).completeBaseName());
    const int identityNumber = NextIdentityNumber(baseName);
    const QString suffix = QString::number(identityNumber);
    m_currentModelId =
        QStringLiteral("model-") + baseName
        + QStringLiteral("-") + suffix;
    m_currentInstanceId =
        QStringLiteral("instance-") + baseName
        + QStringLiteral("-") + suffix;
    m_currentCountBefore = m_document->InstanceCount();

    ModelTopViewLoadRequest loadRequest;
    loadRequest.configpath = m_request.configpath;
    loadRequest.modelpath = m_currentPath;
    loadRequest.sceneid = m_currentCountBefore > 0U
        ? m_document->SceneId()
        : QStringLiteral("preview-scene-") + m_request.batchid;
    loadRequest.modelid = m_currentModelId;
    loadRequest.instanceid = m_currentInstanceId;
    loadRequest.scenerevision = m_currentCountBefore > 0U
        ? m_document->SceneRevision() + 1U
        : 1U;
    loadRequest.transformrevision = 0U;
    loadRequest.appendtoscene = m_currentCountBefore > 0U;
    loadRequest.autolayoutoncompletion = false;
    loadRequest.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Admitted;

    try
    {
        m_expectedLoaderGeneration = m_loadHandler(loadRequest);
        emit SigStateChanged();
    }
    catch (const std::exception& error)
    {
        RecordCurrent(
            SceneBatchImportItemStatus::Failed,
            QStringLiteral("SCENE_BATCH_IMPORT_ITEM_FAILED"),
            QString::fromUtf8(error.what()));
        ++m_nextFileIndex;
        StartNext();
    }
}

void SceneBatchImportController::RecordCurrent(
    const SceneBatchImportItemStatus status,
    const QString& errorCode,
    const QString& message)
{
    SceneBatchImportItemResult item;
    item.path = m_currentPath;
    item.modelid = m_currentModelId;
    item.instanceid = m_currentInstanceId;
    item.status = status;
    item.errorcode = errorCode;
    item.message = message;
    m_summary.items.push_back(std::move(item));

    switch (status)
    {
    case SceneBatchImportItemStatus::Imported:
        ++m_summary.imported;
        break;
    case SceneBatchImportItemStatus::Failed:
        ++m_summary.failed;
        break;
    case SceneBatchImportItemStatus::Cancelled:
        ++m_summary.cancelled;
        break;
    }
}

void SceneBatchImportController::Finish()
{
    if (!m_running)
    {
        return;
    }

    if (!m_cancelRequested
        && m_request.autolayout
        && m_summary.imported > 0
        && m_document->InstanceCount() > 1U)
    {
        const SceneDocumentOperationResult layoutResult =
            m_document->ApplyGridLayout(
                m_document->Layout(),
                m_document->SceneRevision());
        if (layoutResult.IsValid())
        {
            m_summary.autolayoutapplied = layoutResult.changed;
        }
        else
        {
            m_summary.layouterrorcode =
                QString::fromLatin1(
                    SceneDocumentOperationErrorCodeName(
                        layoutResult.error->code)
                        .data());
            m_summary.layouterror =
                layoutResult.error->message;
        }
    }

    m_summary.finalscenerevision =
        m_document->SceneRevision();
    m_running = false;
    m_expectedLoaderGeneration = 0U;
    m_currentPath.clear();
    m_currentModelId.clear();
    m_currentInstanceId.clear();
    emit SigStateChanged();
    emit SigFinished();
}

bool SceneBatchImportController::IdentityExists(
    const QString& modelId,
    const QString& instanceId) const
{
    return std::any_of(
        m_document->Items().begin(),
        m_document->Items().end(),
        [&modelId, &instanceId](const SceneDocumentItem& item)
        {
            return QString::fromStdString(item.instance.modelid)
                    == modelId
                || QString::fromStdString(item.instance.instanceid)
                    == instanceId;
        })
        || std::any_of(
            m_summary.items.begin(),
            m_summary.items.end(),
            [&modelId, &instanceId](
                const SceneBatchImportItemResult& item)
            {
                return item.modelid == modelId
                    || item.instanceid == instanceId;
            });
}

int SceneBatchImportController::NextIdentityNumber(
    const QString& baseName)
{
    int number =
        static_cast<int>(m_document->InstanceCount()) + 1;
    while (IdentityExists(
        QStringLiteral("model-") + baseName
            + QStringLiteral("-") + QString::number(number),
        QStringLiteral("instance-") + baseName
            + QStringLiteral("-") + QString::number(number)))
    {
        ++number;
    }
    return number;
}
