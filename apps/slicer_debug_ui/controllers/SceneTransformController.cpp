#include "SceneTransformController.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <cmath>
#include <map>
#include <optional>
#include <utility>

namespace
{

QString StableCode(const SceneTransformErrorCode code)
{
    return QString::fromLatin1(SceneTransformErrorCodeName(code).data());
}

SceneTransformError MakeError(
    const SceneTransformErrorCode code,
    const QString& field,
    const QString& message)
{
    return {code, field, message};
}

bool WriteSceneDraft(
    const std::filesystem::path& path,
    const slicer_core::MultiModelScene& scene)
{
    QSaveFile output(QString::fromStdWString(path.wstring()));
    if (!output.open(QIODevice::WriteOnly))
    {
        return false;
    }
    const QByteArray payload = QByteArray::fromStdString(
        slicer_core::SerializeMultiModelScene(scene).dump(2));
    return output.write(payload) == payload.size() && output.commit();
}

struct FileSnapshot
{
    bool existed{false};
    QByteArray content;
};

FileSnapshot CaptureFile(const std::filesystem::path& path)
{
    QFile file(QString::fromStdWString(path.wstring()));
    if (!file.exists())
    {
        return {};
    }
    if (!file.open(QIODevice::ReadOnly))
    {
        return {true, {}};
    }
    return {true, file.readAll()};
}

bool RestoreFile(
    const std::filesystem::path& path,
    const FileSnapshot& snapshot)
{
    const QString qtPath = QString::fromStdWString(path.wstring());
    if (!snapshot.existed)
    {
        return !QFile::exists(qtPath) || QFile::remove(qtPath);
    }

    QSaveFile output(qtPath);
    if (!output.open(QIODevice::WriteOnly))
    {
        return false;
    }
    return output.write(snapshot.content) == snapshot.content.size()
        && output.commit();
}

void RestoreSavePair(
    const std::filesystem::path& scenePath,
    const FileSnapshot& sceneSnapshot,
    const std::filesystem::path& effectivePath,
    const FileSnapshot& effectiveSnapshot)
{
    RestoreFile(scenePath, sceneSnapshot);
    RestoreFile(effectivePath, effectiveSnapshot);
}

QString ExtensionLower(const QString& path)
{
    return QFileInfo(path).suffix().toLower();
}

}  // namespace

bool SceneTransformCommandResult::IsValid() const
{
    return !error.has_value();
}

bool SceneTransformSaveResult::IsValid() const
{
    return !error.has_value();
}

bool SceneTransformSnapshotResult::IsValid() const
{
    return !error.has_value();
}

std::string_view SceneTransformErrorCodeName(
    const SceneTransformErrorCode code)
{
    switch (code)
    {
    case SceneTransformErrorCode::None:
        return "NONE";
    case SceneTransformErrorCode::NoSelection:
        return "SCENE_TRANSFORM_NO_SELECTION";
    case SceneTransformErrorCode::InstanceLocked:
        return "SCENE_TRANSFORM_INSTANCE_LOCKED";
    case SceneTransformErrorCode::SceneRevisionStale:
        return "SCENE_TRANSFORM_SCENE_REVISION_STALE";
    case SceneTransformErrorCode::TransformRevisionStale:
        return "MODEL_TRANSFORM_REVISION_STALE";
    case SceneTransformErrorCode::NonFinite:
        return "MODEL_TRANSFORM_NON_FINITE";
    case SceneTransformErrorCode::ScaleNonPositive:
        return "MODEL_TRANSFORM_SCALE_NON_POSITIVE";
    case SceneTransformErrorCode::SourceCacheMissing:
        return "SCENE_TRANSFORM_SOURCE_CACHE_MISSING";
    case SceneTransformErrorCode::ProjectionUnavailable:
        return "SCENE_TRANSFORM_PROJECTION_UNAVAILABLE";
    case SceneTransformErrorCode::EffectiveConfigStale:
        return "SCENE_TRANSFORM_EFFECTIVE_CONFIG_STALE";
    case SceneTransformErrorCode::SaveCancelled:
        return "SCENE_TRANSFORM_SAVE_CANCELLED";
    case SceneTransformErrorCode::SaveFailed:
        return "SCENE_TRANSFORM_SAVE_FAILED";
    }
    return "SCENE_TRANSFORM_UNKNOWN";
}

SceneTransformController::SceneTransformController(
    SceneDocument* document,
    SceneSelectionModel* selectionModel,
    SceneModelRepository* repository,
    QObject* parent)
    : QObject(parent),
      m_document(document),
      m_selectionModel(selectionModel),
      m_repository(repository)
{
    Q_ASSERT(m_document != nullptr);
    Q_ASSERT(m_selectionModel != nullptr);
    Q_ASSERT(m_repository != nullptr);
}

void SceneTransformController::SetProjectionRequester(
    ProjectionRequester requester)
{
    m_projectionRequester = std::move(requester);
}

SceneTransformCommandResult SceneTransformController::SetTransform(
    const slicer_core::ModelTransform& transform,
    const std::uint64_t expectedSceneRevision,
    const std::uint64_t expectedTransformRevision)
{
    return ApplyTransform(
        transform,
        expectedSceneRevision,
        expectedTransformRevision);
}

SceneTransformCommandResult SceneTransformController::CenterAtSceneOrigin(
    const std::uint64_t expectedSceneRevision,
    const std::uint64_t expectedTransformRevision)
{
    const SceneTransformCommandResult validation = ValidateCommand(
        expectedSceneRevision,
        expectedTransformRevision);
    if (!validation.IsValid())
    {
        return validation;
    }
    if (!m_document->Geometry().has_value()
        || m_document->IsGeometryStale())
    {
        return Failure(
            SceneTransformErrorCode::ProjectionUnavailable,
            QStringLiteral("geometry"),
            QStringLiteral("当前有效几何不可用于场景原点居中。"));
    }

    slicer_core::ModelTransform transform =
        m_document->Instance()->transform;
    const auto& bounds = m_document->Geometry()->worldboundsmm;
    transform.translatexmm -=
        (bounds.min.xmm + bounds.max.xmm) * 0.5;
    transform.translateymm -=
        (bounds.min.ymm + bounds.max.ymm) * 0.5;
    return ApplyTransform(
        transform,
        expectedSceneRevision,
        expectedTransformRevision);
}

SceneTransformCommandResult SceneTransformController::ResetTransform(
    const std::uint64_t expectedSceneRevision,
    const std::uint64_t expectedTransformRevision)
{
    return ApplyTransform(
        slicer_core::ModelTransform{},
        expectedSceneRevision,
        expectedTransformRevision);
}

SceneTransformSnapshotResult
SceneTransformController::BuildCurrentScene(
    const std::string& sourceProfileId,
    const slicer_core::SceneBuildVolume& buildVolume) const
{
    SceneTransformSnapshotResult result;
    if (sourceProfileId.empty()
        || m_document->Items().empty())
    {
        result.error = MakeError(
            SceneTransformErrorCode::SaveFailed,
            QStringLiteral("scene"),
            QStringLiteral("场景快照缺少模型实例或 Profile。"));
        return result;
    }

    result.scene.sceneid =
        m_document->SceneId().toStdString();
    result.scene.scenerevision =
        m_document->SceneRevision();
    result.scene.resolvedprofileid = sourceProfileId;
    result.scene.buildvolume = buildVolume;
    result.scene.layout = m_document->Layout();
    std::map<std::string, QString> sourceCacheKeys;
    for (const SceneDocumentItem& item :
         m_document->Items())
    {
        const auto sourceEntry =
            m_repository->Find(item.sourcecachekey);
        if (!sourceEntry.has_value()
            || sourceEntry->model == nullptr)
        {
            result.error = MakeError(
                SceneTransformErrorCode::SourceCacheMissing,
                QStringLiteral("sourceCacheKey"),
                QStringLiteral(
                    "源模型缓存不存在，无法生成场景快照。"));
            return result;
        }

        const std::string modelId =
            item.instance.modelid;
        const auto existing =
            sourceCacheKeys.find(modelId);
        if (existing == sourceCacheKeys.end())
        {
            sourceCacheKeys.emplace(
                modelId,
                item.sourcecachekey);
            const QString extension =
                ExtensionLower(sourceEntry->modelpath);
            const std::filesystem::path sourcePath =
                sourceEntry->model->model_path.empty()
                ? std::filesystem::path(
                      sourceEntry->modelpath.toStdWString())
                : sourceEntry->model->model_path;

            slicer_core::ResourceScope scope;
            scope.resourcescopeid = modelId + "-scope";
            if (extension == QStringLiteral("3mf"))
            {
                scope.kind =
                    slicer_core::ResourceScopeKind::
                        ThreeMfPackage;
                scope.rootpath = sourcePath;
                scope.packagepath = sourcePath;
                scope.partidentity = "root-model";
            }
            else if (extension
                     == QStringLiteral("stl"))
            {
                scope.kind =
                    slicer_core::ResourceScopeKind::StlFile;
                scope.rootpath = sourcePath;
            }
            else
            {
                scope.kind =
                    slicer_core::ResourceScopeKind::
                        ObjDirectory;
                scope.rootpath = sourcePath.parent_path();
            }
            result.scene.resourcescopes.push_back(
                scope);

            slicer_core::ModelSource source;
            source.modelid = modelId;
            source.sourcepath = sourcePath;
            source.format = extension.toStdString();
            source.resourcescopeid =
                scope.resourcescopeid;
            source.sourcehash =
                sourceEntry->sourcehash.toStdString();
            source.resourcehash =
                sourceEntry->resourcehash.toStdString();
            source.displayname =
                QFileInfo(sourceEntry->modelpath)
                    .completeBaseName()
                    .toStdString();
            result.scene.models.push_back(
                std::move(source));
        }
        else if (existing->second
                 != item.sourcecachekey)
        {
            result.error = MakeError(
                SceneTransformErrorCode::SaveFailed,
                QStringLiteral("modelId"),
                QStringLiteral(
                    "同一 modelId 绑定了不同源模型缓存，"
                    "拒绝生成场景快照。"));
            return result;
        }

        slicer_core::SceneModelInstance sceneInstance;
        sceneInstance.instance = item.instance;
        sceneInstance.requestedtransform =
            item.requestedtransform;
        sceneInstance.derivedlayouttransform =
            item.derivedlayouttransform;
        sceneInstance.effectivetransform =
            item.instance.transform;
        sceneInstance.resolvedprofileid =
            sourceProfileId;
        if (item.geometry.has_value())
        {
            sceneInstance.admissionstatus =
                item.geometry->admissionstatus
                        == slicer_core::
                            SceneViewAdmissionStatus::Blocked
                    ? slicer_core::
                          SceneInstanceAdmissionStatus::Blocked
                    : item.geometry->admissionstatus
                              == slicer_core::
                                  SceneViewAdmissionStatus::
                                      Admitted
                        ? slicer_core::
                              SceneInstanceAdmissionStatus::
                                  Admitted
                        : slicer_core::
                              SceneInstanceAdmissionStatus::
                                  Unknown;
        }
        result.scene.instances.push_back(
            std::move(sceneInstance));
    }

    const slicer_core::SceneValidationResult validation =
        slicer_core::ValidateMultiModelScene(
            result.scene,
            slicer_core::SceneValidationPurpose::Draft);
    if (!validation.IsValid())
    {
        result.error = MakeError(
            SceneTransformErrorCode::SaveFailed,
            QStringLiteral("scene"),
            QString::fromStdString(
                validation.errors.front().message));
    }
    return result;
}

SceneTransformSaveResult
SceneTransformController::SaveSceneEffectiveConfig(
    const SceneTransformSaveRequest& request)
{
    SceneTransformSaveResult result;
    if (request.cancelled)
    {
        result.error = MakeError(
            SceneTransformErrorCode::SaveCancelled,
            QStringLiteral("cancelled"),
            QStringLiteral("场景保存已取消。"));
        return result;
    }
    const SceneTransformCommandResult validation = ValidateCommand(
        request.expectedscenerevision,
        request.expectedtransformrevision);
    if (!validation.IsValid())
    {
        result.error = validation.error;
        return result;
    }
    if (request.sessiondirectory.empty()
        || request.sourceprofileid.empty()
        || request.generatedatutc.empty())
    {
        result.error = MakeError(
            SceneTransformErrorCode::SaveFailed,
            QStringLiteral("saveRequest"),
            QStringLiteral("场景保存参数不完整。"));
        return result;
    }

    const SceneTransformSnapshotResult snapshot =
        BuildCurrentScene(
            request.sourceprofileid,
            request.buildvolume);
    if (!snapshot.IsValid())
    {
        result.error = snapshot.error;
        return result;
    }
    result.scene = snapshot.scene;
    result.scenepath =
        request.sessiondirectory / "scene_config.draft.json";
    result.effectiveconfigpath =
        request.sessiondirectory / "scene_config.effective.json";
    std::error_code directoryError;
    std::filesystem::create_directories(
        request.sessiondirectory,
        directoryError);
    if (directoryError)
    {
        result.error = MakeError(
            SceneTransformErrorCode::SaveFailed,
            QStringLiteral("sessionDirectory"),
            QStringLiteral("场景会话目录创建失败。"));
        return result;
    }
    const FileSnapshot sceneSnapshot = CaptureFile(result.scenepath);
    const FileSnapshot effectiveSnapshot =
        CaptureFile(result.effectiveconfigpath);
    if (!WriteSceneDraft(result.scenepath, result.scene))
    {
        result.error = MakeError(
            SceneTransformErrorCode::SaveFailed,
            QStringLiteral("sceneConfig"),
            QStringLiteral("场景草稿写入失败。"));
        return result;
    }

    slicer_core::SceneEffectiveConfigRequest coreRequest;
    coreRequest.scene = result.scene;
    coreRequest.sourcescenepath = result.scenepath;
    coreRequest.generatedconfigpath = result.effectiveconfigpath;
    coreRequest.sourceprofileid = request.sourceprofileid;
    coreRequest.sourceprofileconfigpath =
        request.sourceprofileconfigpath;
    coreRequest.outputpackagedir = request.outputpackagedir;
    coreRequest.generatedatutc = request.generatedatutc;
    coreRequest.dpix = request.dpix;
    coreRequest.dpiy = request.dpiy;
    coreRequest.layerheightmm = request.layerheightmm;
    coreRequest.slicepipelinemode = request.slicepipelinemode;
    coreRequest.production = request.production;

    const slicer_core::SceneEffectiveConfigResult written =
        slicer_core::WriteSceneEffectiveConfig(coreRequest);
    const slicer_core::SceneEffectiveConfigResult readback =
        written.IsValid()
        ? slicer_core::ReadSceneEffectiveConfig(result.effectiveconfigpath)
        : slicer_core::SceneEffectiveConfigResult{};
    if (!written.IsValid() || !readback.IsValid())
    {
        RestoreSavePair(
            result.scenepath,
            sceneSnapshot,
            result.effectiveconfigpath,
            effectiveSnapshot);
        result.error = MakeError(
            SceneTransformErrorCode::SaveFailed,
            QStringLiteral("effectiveConfig"),
            QStringLiteral("场景生效配置写入或回读失败。"));
        return result;
    }
    if (slicer_core::IsSceneEffectiveConfigStale(
            readback.document,
            result.scene))
    {
        RestoreSavePair(
            result.scenepath,
            sceneSnapshot,
            result.effectiveconfigpath,
            effectiveSnapshot);
        result.error = MakeError(
            SceneTransformErrorCode::EffectiveConfigStale,
            QStringLiteral("identity"),
            QStringLiteral("场景生效配置身份或 revision 已过期。"));
        return result;
    }

    result.confighash = readback.confighash;
    if (!m_document->MarkSaved(
            QString::fromStdWString(result.scenepath.wstring()),
            QString::fromStdWString(
                result.effectiveconfigpath.wstring()),
            QString::fromStdString(result.confighash),
            request.expectedscenerevision,
            request.expectedtransformrevision))
    {
        RestoreSavePair(
            result.scenepath,
            sceneSnapshot,
            result.effectiveconfigpath,
            effectiveSnapshot);
        result.error = MakeError(
            SceneTransformErrorCode::EffectiveConfigStale,
            QStringLiteral("identity"),
            QStringLiteral(
                "保存完成前场景 revision 已变化，结果已回滚。"));
        return result;
    }
    emit SigSceneSaved(
        QString::fromStdWString(result.effectiveconfigpath.wstring()));
    return result;
}

SceneTransformCommandResult SceneTransformController::ValidateCommand(
    const std::uint64_t expectedSceneRevision,
    const std::uint64_t expectedTransformRevision)
{
    if (!m_document->Instance().has_value()
        || m_selectionModel->SelectedInstance().isEmpty()
        || m_selectionModel->SelectedInstance()
            != QString::fromStdString(
                m_document->Instance()->instanceid))
    {
        return Failure(
            SceneTransformErrorCode::NoSelection,
            QStringLiteral("selection"),
            QStringLiteral("请先选择当前模型。"));
    }
    if (m_document->Instance()->locked)
    {
        return Failure(
            SceneTransformErrorCode::InstanceLocked,
            QStringLiteral("locked"),
            QStringLiteral("当前模型已锁定，不能修改变换。"));
    }
    if (expectedSceneRevision != m_document->SceneRevision())
    {
        return Failure(
            SceneTransformErrorCode::SceneRevisionStale,
            QStringLiteral("sceneRevision"),
            QStringLiteral("场景 revision 已变化，请刷新后重试。"));
    }
    if (expectedTransformRevision
        != m_document->Instance()->transformrevision)
    {
        return Failure(
            SceneTransformErrorCode::TransformRevisionStale,
            QStringLiteral("transformRevision"),
            QStringLiteral("模型变换 revision 已变化，请刷新后重试。"));
    }
    if (!m_repository->Find(m_document->SourceCacheKey()).has_value())
    {
        return Failure(
            SceneTransformErrorCode::SourceCacheMissing,
            QStringLiteral("sourceCacheKey"),
            QStringLiteral("源模型缓存不存在，请重新导入模型。"));
    }
    return {};
}

SceneTransformCommandResult SceneTransformController::ApplyTransform(
    const slicer_core::ModelTransform& transform,
    const std::uint64_t expectedSceneRevision,
    const std::uint64_t expectedTransformRevision)
{
    const SceneTransformCommandResult commandValidation = ValidateCommand(
        expectedSceneRevision,
        expectedTransformRevision);
    if (!commandValidation.IsValid())
    {
        return commandValidation;
    }
    slicer_core::ModelInstance updated =
        m_document->Instance().value();
    const slicer_core::ModelInstanceTransformUpdateResult update =
        slicer_core::UpdateModelInstanceTransform(
            updated,
            transform,
            expectedTransformRevision);
    if (!update.IsValid())
    {
        const SceneTransformErrorCode code =
            update.error->code
                == slicer_core::ModelTransformErrorCode::ScaleNonPositive
            ? SceneTransformErrorCode::ScaleNonPositive
            : update.error->code
                    == slicer_core::ModelTransformErrorCode::RevisionStale
                ? SceneTransformErrorCode::TransformRevisionStale
                : SceneTransformErrorCode::NonFinite;
        return Failure(
            code,
            QString::fromStdString(update.error->field),
            QString::fromStdString(update.error->message));
    }
    if (!update.changed)
    {
        return {};
    }
    if (!m_projectionRequester)
    {
        return Failure(
            SceneTransformErrorCode::ProjectionUnavailable,
            QStringLiteral("projectionRequester"),
            QStringLiteral("异步重投影服务不可用。"));
    }
    if (!m_document->CommitInstance(
            updated,
            expectedSceneRevision))
    {
        return Failure(
            SceneTransformErrorCode::SceneRevisionStale,
            QStringLiteral("sceneRevision"),
            QStringLiteral("场景 revision 已变化，变换未提交。"));
    }

    SceneProjectionRequest projection;
    projection.cachekey = m_document->SourceCacheKey();
    projection.sceneid = m_document->SceneId();
    projection.scenerevision = m_document->SceneRevision();
    projection.instance = m_document->Instance().value();
    projection.admissionstatus =
        m_document->Geometry().has_value()
        ? m_document->Geometry()->admissionstatus
        : slicer_core::SceneViewAdmissionStatus::Unknown;
    m_projectionRequester(projection);
    emit SigTransformChanged();
    return {true, std::nullopt};
}

SceneTransformCommandResult SceneTransformController::Failure(
    const SceneTransformErrorCode code,
    const QString& field,
    const QString& message)
{
    emit SigCommandFailed(StableCode(code), message);
    return {false, MakeError(code, field, message)};
}
