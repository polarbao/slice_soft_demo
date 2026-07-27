#include "SceneDocument.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{

constexpr double kLayoutTolerance{1.0e-9};

bool LayoutsEquivalent(
    const slicer_core::SceneLayout& left,
    const slicer_core::SceneLayout& right)
{
    return left.policy == right.policy
        && left.maxcolumns == right.maxcolumns
        && left.maxrows == right.maxrows
        && std::abs(left.columngapmm - right.columngapmm)
            <= kLayoutTolerance
        && std::abs(left.rowgapmm - right.rowgapmm)
            <= kLayoutTolerance
        && left.spacingmode == right.spacingmode
        && left.order == right.order;
}

}  // namespace

bool SceneDocumentOperationResult::IsValid() const
{
    return !error.has_value();
}

std::string_view SceneDocumentOperationErrorCodeName(
    const SceneDocumentOperationErrorCode code)
{
    switch (code)
    {
    case SceneDocumentOperationErrorCode::None:
        return "NONE";
    case SceneDocumentOperationErrorCode::SceneRevisionStale:
        return "SCENE_REVISION_STALE";
    case SceneDocumentOperationErrorCode::InstanceLimitExceeded:
        return "SCENE_INSTANCE_LIMIT_EXCEEDED";
    case SceneDocumentOperationErrorCode::InstanceIdDuplicate:
        return "SCENE_INSTANCE_ID_DUPLICATE";
    case SceneDocumentOperationErrorCode::InstanceNotFound:
        return "SCENE_INSTANCE_NOT_FOUND";
    case SceneDocumentOperationErrorCode::InstanceLocked:
        return "SCENE_INSTANCE_LOCKED";
    case SceneDocumentOperationErrorCode::SceneIdentityMismatch:
        return "SCENE_IDENTITY_MISMATCH";
    case SceneDocumentOperationErrorCode::LayoutInvalid:
        return "SCENE_LAYOUT_INVALID";
    case SceneDocumentOperationErrorCode::LayoutRestoreUnavailable:
        return "SCENE_LAYOUT_RESTORE_UNAVAILABLE";
    }
    return "SCENE_DOCUMENT_UNKNOWN";
}

SceneDocument::SceneDocument(QObject* parent)
    : QObject(parent)
{
}

void SceneDocument::Reset()
{
    ++m_generation;
    m_modelPath.clear();
    m_pendingModelPath.clear();
    m_error.clear();
    m_geometry.reset();
    m_instance.reset();
    m_sceneId.clear();
    m_sceneRevision = 0U;
    m_sourceCacheKey.clear();
    m_sourceHash.clear();
    m_resourceHash.clear();
    m_dirty = false;
    m_geometryStale = false;
    m_sceneConfigPath.clear();
    m_effectiveConfigPath.clear();
    m_effectiveConfigHash.clear();
    m_items.clear();
    m_currentInstanceId.clear();
    m_layout = {};
    m_layoutSnapshot.reset();
    m_additionInProgress = false;
    m_transformedPreflightState =
        SceneTransformedPreflightState::NotRun;
    m_transformedPreflightGeneration = 0U;
    m_transformedPreflightSceneRevision = 0U;
    m_transformedPreflightTransformRevision = 0U;
    m_transformedPreflight.reset();
    m_transformedPreflightError.clear();
    PublishState(SceneDocumentState::Unloaded);
}

void SceneDocument::SetLoading(
    const quint64 generation,
    const QString& modelPath)
{
    m_generation = generation;
    m_modelPath = modelPath;
    m_pendingModelPath.clear();
    m_error.clear();
    m_geometry.reset();
    m_instance.reset();
    m_sceneId.clear();
    m_sceneRevision = 0U;
    m_sourceCacheKey.clear();
    m_sourceHash.clear();
    m_resourceHash.clear();
    m_dirty = false;
    m_geometryStale = false;
    m_sceneConfigPath.clear();
    m_effectiveConfigPath.clear();
    m_effectiveConfigHash.clear();
    m_items.clear();
    m_currentInstanceId.clear();
    m_layout = {};
    m_layoutSnapshot.reset();
    m_additionInProgress = false;
    m_transformedPreflightState =
        SceneTransformedPreflightState::NotRun;
    m_transformedPreflightGeneration = 0U;
    m_transformedPreflightSceneRevision = 0U;
    m_transformedPreflightTransformRevision = 0U;
    m_transformedPreflight.reset();
    m_transformedPreflightError.clear();
    PublishState(SceneDocumentState::Loading);
}

void SceneDocument::SetAdding(
    const quint64 generation,
    const QString& modelPath)
{
    m_generation = generation;
    m_pendingModelPath = modelPath;
    m_error.clear();
    m_additionInProgress = true;
    PublishState(SceneDocumentState::Loading);
}

bool SceneDocument::SetSceneContext(
    const quint64 generation,
    const QString& sceneId,
    const quint64 sceneRevision,
    const QString& sourceCacheKey,
    const QString& sourceHash,
    const QString& resourceHash,
    slicer_core::ModelInstance instance)
{
    if (generation != m_generation
        || sceneId.trimmed().isEmpty()
        || sourceCacheKey.trimmed().isEmpty())
    {
        return false;
    }
    m_sceneId = sceneId;
    m_sceneRevision = sceneRevision;
    m_sourceCacheKey = sourceCacheKey;
    m_sourceHash = sourceHash;
    m_resourceHash = resourceHash;
    m_instance = std::move(instance);
    SceneDocumentItem item;
    item.modelpath = m_modelPath;
    item.sourcecachekey = sourceCacheKey;
    item.sourcehash = sourceHash;
    item.resourcehash = resourceHash;
    item.instance = m_instance.value();
    item.requestedtransform = item.instance.transform;
    m_items.push_back(std::move(item));
    m_currentInstanceId =
        QString::fromStdString(m_instance->instanceid);
    m_additionInProgress = false;
    m_dirty = false;
    m_geometryStale = true;
    return true;
}

bool SceneDocument::AddSceneContext(
    const quint64 generation,
    const QString& sceneId,
    const quint64 sceneRevision,
    const QString& sourceCacheKey,
    const QString& sourceHash,
    const QString& resourceHash,
    slicer_core::ModelInstance instance)
{
    if (!m_additionInProgress
        || generation != m_generation
        || m_items.size() >= kMaximumInstanceCount
        || sceneId != m_sceneId
        || sceneRevision != m_sceneRevision + 1U
        || sourceCacheKey.trimmed().isEmpty()
        || FindItemIndex(
               QString::fromStdString(instance.instanceid))
               .has_value())
    {
        return false;
    }

    m_sceneRevision = sceneRevision;
    for (SceneDocumentItem& existing : m_items)
    {
        if (existing.geometry.has_value())
        {
            existing.geometry->scenerevision = m_sceneRevision;
            slicer_core::RefreshSceneViewGeometryHash(
                existing.geometry.value());
        }
    }
    SceneDocumentItem item;
    item.modelpath = m_pendingModelPath;
    item.sourcecachekey = sourceCacheKey;
    item.sourcehash = sourceHash;
    item.resourcehash = resourceHash;
    item.instance = std::move(instance);
    item.requestedtransform = item.instance.transform;
    m_items.push_back(std::move(item));
    LoadCurrentItem(m_items.size() - 1U);
    m_additionInProgress = false;
    m_pendingModelPath.clear();
    m_dirty = true;
    m_geometryStale = true;
    m_sceneConfigPath.clear();
    m_effectiveConfigPath.clear();
    m_effectiveConfigHash.clear();
    InvalidateLayoutRestore();
    return true;
}

void SceneDocument::SetProjectionLoading(const quint64 generation)
{
    m_generation = generation;
    m_error.clear();
    m_geometryStale = true;
    PublishState(SceneDocumentState::Loading);
}

bool SceneDocument::SetGeometry(
    const quint64 generation,
    slicer_core::SceneViewGeometry geometry)
{
    if (generation != m_generation)
    {
        return false;
    }
    if (m_instance.has_value()
        && (geometry.sceneid != m_sceneId.toStdString()
            || geometry.instanceid != m_instance->instanceid
            || geometry.scenerevision != m_sceneRevision
            || geometry.transformrevision
                != m_instance->transformrevision))
    {
        return false;
    }

    m_error.clear();
    if (m_instance.has_value())
    {
        m_instance->effectivebboxmm = geometry.effectivebboxmm;
    }
    m_geometry = std::move(geometry);
    SyncCurrentItem();
    m_geometryStale = false;
    PublishState(
        m_geometry->admissionstatus
                == slicer_core::SceneViewAdmissionStatus::Blocked
            ? SceneDocumentState::Blocked
            : SceneDocumentState::Ready);
    return true;
}

bool SceneDocument::SetFailure(
    const quint64 generation,
    const QString& error)
{
    if (generation != m_generation)
    {
        return false;
    }

    m_error = error;
    if (m_additionInProgress && !m_items.empty())
    {
        m_additionInProgress = false;
        m_pendingModelPath.clear();
        m_geometryStale = false;
        PublishState(
            m_geometry.has_value()
                    && m_geometry->admissionstatus
                        == slicer_core::SceneViewAdmissionStatus::Blocked
                ? SceneDocumentState::Blocked
                : SceneDocumentState::Ready);
    }
    else
    {
        m_geometryStale = true;
        PublishState(SceneDocumentState::Failed);
    }
    return true;
}

bool SceneDocument::SetCancelled(const quint64 generation)
{
    if (generation != m_generation)
    {
        return false;
    }

    m_error.clear();
    if (m_additionInProgress && !m_items.empty())
    {
        m_additionInProgress = false;
        m_pendingModelPath.clear();
        m_geometryStale = false;
        PublishState(
            m_geometry.has_value()
                    && m_geometry->admissionstatus
                        == slicer_core::SceneViewAdmissionStatus::Blocked
                ? SceneDocumentState::Blocked
                : SceneDocumentState::Ready);
    }
    else
    {
        m_geometryStale = true;
        PublishState(SceneDocumentState::Cancelled);
    }
    return true;
}

SceneDocumentState SceneDocument::State() const
{
    return m_state;
}

quint64 SceneDocument::Generation() const
{
    return m_generation;
}

QString SceneDocument::ModelPath() const
{
    return m_modelPath;
}

QString SceneDocument::Error() const
{
    return m_error;
}

const std::optional<slicer_core::SceneViewGeometry>&
SceneDocument::Geometry() const
{
    return m_geometry;
}

const std::optional<slicer_core::ModelInstance>&
SceneDocument::Instance() const
{
    return m_instance;
}

const std::vector<SceneDocumentItem>& SceneDocument::Items() const
{
    return m_items;
}

std::size_t SceneDocument::InstanceCount() const
{
    return m_items.size();
}

QString SceneDocument::CurrentInstanceId() const
{
    return m_currentInstanceId;
}

bool SceneDocument::SetCurrentInstance(const QString& instanceId)
{
    const std::optional<std::size_t> index =
        FindItemIndex(instanceId);
    if (!index.has_value())
    {
        return false;
    }
    if (m_currentInstanceId == instanceId)
    {
        return true;
    }
    SyncCurrentItem();
    LoadCurrentItem(index.value());
    m_transformedPreflightState =
        SceneTransformedPreflightState::NotRun;
    m_transformedPreflight.reset();
    m_transformedPreflightError.clear();
    PublishState(
        m_geometry.has_value()
                && m_geometry->admissionstatus
                    == slicer_core::SceneViewAdmissionStatus::Blocked
            ? SceneDocumentState::Blocked
            : SceneDocumentState::Ready);
    return true;
}

SceneDocumentOperationResult SceneDocument::DuplicateInstance(
    const QString& sourceInstanceId,
    const QString& newInstanceId,
    const quint64 expectedSceneRevision)
{
    const SceneDocumentOperationResult validation =
        ValidateOperation(sourceInstanceId, expectedSceneRevision);
    if (!validation.IsValid())
    {
        return validation;
    }
    if (m_items.size() >= kMaximumInstanceCount)
    {
        return OperationFailure(
            SceneDocumentOperationErrorCode::InstanceLimitExceeded,
            QStringLiteral("instances"),
            QStringLiteral("场景最多允许 22 个模型实例。"));
    }
    if (newInstanceId.trimmed().isEmpty()
        || FindItemIndex(newInstanceId).has_value())
    {
        return OperationFailure(
            SceneDocumentOperationErrorCode::InstanceIdDuplicate,
            QStringLiteral("instanceId"),
            QStringLiteral("新实例标识为空或已存在。"));
    }

    SceneDocumentItem duplicate =
        m_items.at(FindItemIndex(sourceInstanceId).value());
    duplicate.instance.instanceid =
        newInstanceId.toStdString();
    duplicate.instance.transformrevision = 0U;
    duplicate.instance.locked = false;
    duplicate.layoutrow = -1;
    duplicate.layoutcolumn = -1;
    if (duplicate.geometry.has_value())
    {
        duplicate.geometry->instanceid =
            duplicate.instance.instanceid;
        duplicate.geometry->transformrevision = 0U;
        duplicate.geometry->visible = duplicate.instance.visible;
        duplicate.geometry->locked = false;
    }
    m_items.push_back(std::move(duplicate));
    InvalidateLayoutRestore();
    AdvanceSceneRevision();
    LoadCurrentItem(m_items.size() - 1U);
    emit SigChanged();
    return {true, std::nullopt};
}

SceneDocumentOperationResult SceneDocument::DeleteInstance(
    const QString& instanceId,
    const quint64 expectedSceneRevision)
{
    const SceneDocumentOperationResult validation =
        ValidateOperation(instanceId, expectedSceneRevision);
    if (!validation.IsValid())
    {
        return validation;
    }
    const std::size_t index =
        FindItemIndex(instanceId).value();
    if (m_items[index].instance.locked)
    {
        return OperationFailure(
            SceneDocumentOperationErrorCode::InstanceLocked,
            QStringLiteral("locked"),
            QStringLiteral("锁定的模型实例不能删除。"));
    }

    m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(index));
    InvalidateLayoutRestore();
    AdvanceSceneRevision();
    if (m_items.empty())
    {
        ClearCurrentInstance();
        PublishState(SceneDocumentState::Unloaded);
    }
    else
    {
        LoadCurrentItem(std::min(index, m_items.size() - 1U));
        PublishState(
            m_geometry.has_value()
                    && m_geometry->admissionstatus
                        == slicer_core::SceneViewAdmissionStatus::Blocked
                ? SceneDocumentState::Blocked
                : SceneDocumentState::Ready);
    }
    return {true, std::nullopt};
}

SceneDocumentOperationResult SceneDocument::SetInstanceVisible(
    const QString& instanceId,
    const bool visible,
    const quint64 expectedSceneRevision)
{
    const SceneDocumentOperationResult validation =
        ValidateOperation(instanceId, expectedSceneRevision);
    if (!validation.IsValid())
    {
        return validation;
    }
    SceneDocumentItem& item =
        m_items[FindItemIndex(instanceId).value()];
    if (item.instance.visible == visible)
    {
        return {false, std::nullopt};
    }
    item.instance.visible = visible;
    if (item.geometry.has_value())
    {
        item.geometry->visible = visible;
    }
    InvalidateLayoutRestore();
    AdvanceSceneRevision();
    if (instanceId == m_currentInstanceId)
    {
        LoadCurrentItem(FindItemIndex(instanceId).value());
    }
    emit SigChanged();
    return {true, std::nullopt};
}

SceneDocumentOperationResult SceneDocument::SetInstanceLocked(
    const QString& instanceId,
    const bool locked,
    const quint64 expectedSceneRevision)
{
    const SceneDocumentOperationResult validation =
        ValidateOperation(instanceId, expectedSceneRevision);
    if (!validation.IsValid())
    {
        return validation;
    }
    SceneDocumentItem& item =
        m_items[FindItemIndex(instanceId).value()];
    if (item.instance.locked == locked)
    {
        return {false, std::nullopt};
    }
    item.instance.locked = locked;
    if (item.geometry.has_value())
    {
        item.geometry->locked = locked;
    }
    InvalidateLayoutRestore();
    AdvanceSceneRevision();
    if (instanceId == m_currentInstanceId)
    {
        LoadCurrentItem(FindItemIndex(instanceId).value());
    }
    emit SigChanged();
    return {true, std::nullopt};
}

const slicer_core::SceneLayout& SceneDocument::Layout() const
{
    return m_layout;
}

SceneDocumentOperationResult SceneDocument::ApplyGridLayout(
    const slicer_core::SceneLayout& layout,
    const quint64 expectedSceneRevision)
{
    if (expectedSceneRevision != m_sceneRevision)
    {
        return OperationFailure(
            SceneDocumentOperationErrorCode::SceneRevisionStale,
            QStringLiteral("sceneRevision"),
            QStringLiteral("场景 revision 已变化，请刷新后重试。"));
    }
    for (const SceneDocumentItem& item : m_items)
    {
        if (!item.geometry.has_value()
            || item.geometry->scenerevision != m_sceneRevision
            || item.geometry->transformrevision
                != item.instance.transformrevision)
        {
            return OperationFailure(
                SceneDocumentOperationErrorCode::LayoutInvalid,
                QStringLiteral("scene.items.geometry"),
                QStringLiteral(
                    "场景包含尚未完成重投影的模型，"
                    "请等待俯视更新后再排版。"));
        }
    }

    slicer_core::GridLayoutRequest request;
    request.layout = layout;
    request.currentscenerevision = m_sceneRevision;
    request.expectedscenerevision = expectedSceneRevision;
    request.items.reserve(m_items.size());
    for (const SceneDocumentItem& item : m_items)
    {
        slicer_core::GridLayoutItem layoutItem;
        layoutItem.instance = item.instance;
        layoutItem.requestedtransform = item.requestedtransform;
        layoutItem.currentderivedlayouttransform =
            item.derivedlayouttransform;
        request.items.push_back(std::move(layoutItem));
    }

    const slicer_core::GridLayoutResult layoutResult =
        slicer_core::ComputeGridLayout(request);
    if (!layoutResult.IsValid())
    {
        return OperationFailure(
            SceneDocumentOperationErrorCode::LayoutInvalid,
            QString::fromStdString(layoutResult.error->field),
            QString::fromStdString(layoutResult.error->message));
    }

    const bool layoutChanged = !LayoutsEquivalent(m_layout, layout);
    if (!layoutResult.changed && !layoutChanged)
    {
        return {false, std::nullopt};
    }

    SceneLayoutSnapshot snapshot;
    snapshot.layout = m_layout;
    snapshot.items.reserve(m_items.size());
    for (const SceneDocumentItem& item : m_items)
    {
        SceneLayoutSnapshotItem snapshotItem;
        snapshotItem.instanceid =
            QString::fromStdString(item.instance.instanceid);
        snapshotItem.requestedtransform = item.requestedtransform;
        snapshotItem.derivedlayouttransform =
            item.derivedlayouttransform;
        snapshotItem.effectivetransform = item.instance.transform;
        snapshotItem.effectivebboxmm =
            item.instance.effectivebboxmm;
        snapshotItem.layoutrow = item.layoutrow;
        snapshotItem.layoutcolumn = item.layoutcolumn;
        snapshot.items.push_back(std::move(snapshotItem));
    }

    std::vector<std::string> transformHashes;
    transformHashes.reserve(layoutResult.placements.size());
    for (std::size_t index = 0U;
         index < layoutResult.placements.size();
         ++index)
    {
        const slicer_core::ModelTransformHashResult transformHash =
            slicer_core::ComputeModelTransformHash(
                layoutResult.placements[index].effectivetransform,
                m_items[index].instance.sourcetransformidentity,
                m_items[index].instance.instanceid,
                m_items[index].instance.modelid);
        if (!transformHash.IsValid())
        {
            return OperationFailure(
                SceneDocumentOperationErrorCode::LayoutInvalid,
                QString::fromStdString(transformHash.error->field),
                QString::fromStdString(transformHash.error->message));
        }
        transformHashes.push_back(transformHash.hash);
    }

    for (std::size_t index = 0U;
         index < m_items.size();
         ++index)
    {
        SceneDocumentItem& item = m_items[index];
        const slicer_core::GridLayoutPlacement& placement =
            layoutResult.placements[index];
        const double translateX =
            placement.effectivebboxmm.min.x
            - item.instance.effectivebboxmm.min.x;
        const double translateY =
            placement.effectivebboxmm.min.y
            - item.instance.effectivebboxmm.min.y;
        const bool transformChanged =
            !slicer_core::ModelTransformsEquivalent(
                item.instance.transform,
                placement.effectivetransform);

        item.requestedtransform = placement.requestedtransform;
        item.derivedlayouttransform =
            placement.derivedlayouttransform;
        item.instance.transform = placement.effectivetransform;
        item.instance.effectivebboxmm =
            placement.effectivebboxmm;
        item.layoutrow = placement.row;
        item.layoutcolumn = placement.column;
        if (transformChanged)
        {
            ++item.instance.transformrevision;
        }
        TranslateGeometry(
            item,
            translateX,
            translateY,
            transformHashes[index]);
    }
    m_layout = layout;
    m_layoutSnapshot = std::move(snapshot);
    AdvanceSceneRevision();
    if (!m_currentInstanceId.isEmpty())
    {
        LoadCurrentItem(
            FindItemIndex(m_currentInstanceId).value());
    }
    emit SigChanged();
    return {true, std::nullopt};
}

bool SceneDocument::CanRestoreGridLayout() const
{
    return m_layoutSnapshot.has_value()
        && SnapshotMatchesCurrentScene();
}

SceneDocumentOperationResult SceneDocument::RestoreGridLayout(
    const quint64 expectedSceneRevision)
{
    if (expectedSceneRevision != m_sceneRevision)
    {
        return OperationFailure(
            SceneDocumentOperationErrorCode::SceneRevisionStale,
            QStringLiteral("sceneRevision"),
            QStringLiteral("场景 revision 已变化，请刷新后重试。"));
    }
    if (!CanRestoreGridLayout())
    {
        return OperationFailure(
            SceneDocumentOperationErrorCode::
                LayoutRestoreUnavailable,
            QStringLiteral("layout"),
            QStringLiteral("没有可恢复的排版快照。"));
    }

    const SceneLayoutSnapshot snapshot =
        m_layoutSnapshot.value();
    std::vector<std::string> transformHashes;
    transformHashes.reserve(snapshot.items.size());
    for (std::size_t index = 0U;
         index < snapshot.items.size();
         ++index)
    {
        const slicer_core::ModelTransformHashResult transformHash =
            slicer_core::ComputeModelTransformHash(
                snapshot.items[index].effectivetransform,
                m_items[index].instance.sourcetransformidentity,
                m_items[index].instance.instanceid,
                m_items[index].instance.modelid);
        if (!transformHash.IsValid())
        {
            return OperationFailure(
                SceneDocumentOperationErrorCode::LayoutInvalid,
                QString::fromStdString(transformHash.error->field),
                QString::fromStdString(transformHash.error->message));
        }
        transformHashes.push_back(transformHash.hash);
    }

    for (std::size_t index = 0U;
         index < m_items.size();
         ++index)
    {
        SceneDocumentItem& item = m_items[index];
        const SceneLayoutSnapshotItem& snapshotItem =
            snapshot.items[index];
        const double translateX =
            snapshotItem.effectivebboxmm.min.x
            - item.instance.effectivebboxmm.min.x;
        const double translateY =
            snapshotItem.effectivebboxmm.min.y
            - item.instance.effectivebboxmm.min.y;
        const bool transformChanged =
            !slicer_core::ModelTransformsEquivalent(
                item.instance.transform,
                snapshotItem.effectivetransform);

        item.requestedtransform =
            snapshotItem.requestedtransform;
        item.derivedlayouttransform =
            snapshotItem.derivedlayouttransform;
        item.instance.transform =
            snapshotItem.effectivetransform;
        item.instance.effectivebboxmm =
            snapshotItem.effectivebboxmm;
        item.layoutrow = snapshotItem.layoutrow;
        item.layoutcolumn = snapshotItem.layoutcolumn;
        if (transformChanged)
        {
            ++item.instance.transformrevision;
        }
        TranslateGeometry(
            item,
            translateX,
            translateY,
            transformHashes[index]);
    }
    m_layout = snapshot.layout;
    m_layoutSnapshot.reset();
    AdvanceSceneRevision();
    if (!m_currentInstanceId.isEmpty())
    {
        LoadCurrentItem(
            FindItemIndex(m_currentInstanceId).value());
    }
    emit SigChanged();
    return {true, std::nullopt};
}

QString SceneDocument::SceneId() const
{
    return m_sceneId;
}

quint64 SceneDocument::SceneRevision() const
{
    return m_sceneRevision;
}

QString SceneDocument::SourceCacheKey() const
{
    return m_sourceCacheKey;
}

QString SceneDocument::SourceHash() const
{
    return m_sourceHash;
}

QString SceneDocument::ResourceHash() const
{
    return m_resourceHash;
}

bool SceneDocument::IsDirty() const
{
    return m_dirty;
}

bool SceneDocument::IsGeometryStale() const
{
    return m_geometryStale;
}

QString SceneDocument::SceneConfigPath() const
{
    return m_sceneConfigPath;
}

QString SceneDocument::EffectiveConfigPath() const
{
    return m_effectiveConfigPath;
}

bool SceneDocument::SetTransformedPreflightRunning(
    const quint64 generation,
    const quint64 sceneRevision,
    const quint64 transformRevision)
{
    if (!m_instance.has_value()
        || sceneRevision != m_sceneRevision
        || transformRevision != m_instance->transformrevision)
    {
        return false;
    }
    m_transformedPreflightGeneration = generation;
    m_transformedPreflightSceneRevision = sceneRevision;
    m_transformedPreflightTransformRevision = transformRevision;
    m_transformedPreflightState =
        SceneTransformedPreflightState::Running;
    m_transformedPreflightError.clear();
    emit SigChanged();
    return true;
}

bool SceneDocument::SetTransformedPreflightResult(
    const quint64 generation,
    slicer_core::TransformedModelPreflightExecution execution)
{
    if (!m_instance.has_value()
        || generation != m_transformedPreflightGeneration
        || execution.sceneid != m_sceneId.toStdString()
        || execution.instanceid != m_instance->instanceid
        || execution.scenerevision != m_sceneRevision
        || execution.transformrevision
            != m_instance->transformrevision
        || execution.scenerevision
            != m_transformedPreflightSceneRevision
        || execution.transformrevision
            != m_transformedPreflightTransformRevision)
    {
        return false;
    }
    m_transformedPreflight = std::move(execution);
    m_transformedPreflightState =
        SceneTransformedPreflightState::Ready;
    m_transformedPreflightError.clear();
    emit SigChanged();
    return true;
}

bool SceneDocument::SetTransformedPreflightFailure(
    const quint64 generation,
    const QString& error)
{
    if (generation != m_transformedPreflightGeneration)
    {
        return false;
    }
    m_transformedPreflightState =
        SceneTransformedPreflightState::Failed;
    m_transformedPreflightError = error;
    emit SigChanged();
    return true;
}

bool SceneDocument::SetTransformedPreflightCancelled(
    const quint64 generation)
{
    if (generation != m_transformedPreflightGeneration)
    {
        return false;
    }
    m_transformedPreflightState =
        SceneTransformedPreflightState::Cancelled;
    m_transformedPreflightError.clear();
    emit SigChanged();
    return true;
}

SceneTransformedPreflightState
SceneDocument::TransformedPreflightState() const
{
    return m_transformedPreflightState;
}

const std::optional<
    slicer_core::TransformedModelPreflightExecution>&
SceneDocument::TransformedPreflight() const
{
    return m_transformedPreflight;
}

QString SceneDocument::TransformedPreflightError() const
{
    return m_transformedPreflightError;
}

bool SceneDocument::CommitInstance(
    const slicer_core::ModelInstance& instance,
    const quint64 expectedSceneRevision)
{
    if (!m_instance.has_value()
        || expectedSceneRevision != m_sceneRevision
        || instance.instanceid != m_instance->instanceid
        || instance.modelid != m_instance->modelid)
    {
        return false;
    }
    m_instance = instance;
    const std::size_t currentIndex =
        FindItemIndex(m_currentInstanceId).value();
    SceneDocumentItem& currentItem = m_items[currentIndex];
    currentItem.instance = instance;
    currentItem.requestedtransform = instance.transform;
    currentItem.derivedlayouttransform = {};
    currentItem.layoutrow = -1;
    currentItem.layoutcolumn = -1;
    m_geometry = currentItem.geometry;
    InvalidateLayoutRestore();
    ++m_sceneRevision;
    for (SceneDocumentItem& item : m_items)
    {
        if (item.geometry.has_value())
        {
            item.geometry->scenerevision = m_sceneRevision;
            slicer_core::RefreshSceneViewGeometryHash(
                item.geometry.value());
        }
    }
    if (m_geometry.has_value())
    {
        m_geometry->scenerevision = m_sceneRevision;
        slicer_core::RefreshSceneViewGeometryHash(
            m_geometry.value());
    }
    m_dirty = true;
    m_geometryStale = true;
    m_effectiveConfigPath.clear();
    m_effectiveConfigHash.clear();
    m_transformedPreflightState =
        SceneTransformedPreflightState::Stale;
    m_transformedPreflightError.clear();
    emit SigChanged();
    return true;
}

bool SceneDocument::MarkSaved(
    const QString& sceneConfigPath,
    const QString& effectiveConfigPath,
    const QString& configHash,
    const quint64 sceneRevision,
    const quint64 transformRevision)
{
    if (!m_instance.has_value()
        || sceneRevision != m_sceneRevision
        || transformRevision != m_instance->transformrevision
        || effectiveConfigPath.trimmed().isEmpty()
        || configHash.trimmed().isEmpty())
    {
        return false;
    }
    m_sceneConfigPath = sceneConfigPath;
    m_effectiveConfigPath = effectiveConfigPath;
    m_effectiveConfigHash = configHash;
    m_dirty = false;
    emit SigChanged();
    return true;
}

void SceneDocument::PublishState(const SceneDocumentState state)
{
    m_state = state;
    emit SigChanged();
}

void SceneDocument::ClearCurrentInstance()
{
    m_modelPath.clear();
    m_geometry.reset();
    m_instance.reset();
    m_sourceCacheKey.clear();
    m_sourceHash.clear();
    m_resourceHash.clear();
    m_currentInstanceId.clear();
    m_geometryStale = false;
    m_transformedPreflightState =
        SceneTransformedPreflightState::NotRun;
    m_transformedPreflight.reset();
    m_transformedPreflightError.clear();
}

void SceneDocument::LoadCurrentItem(const std::size_t index)
{
    const SceneDocumentItem& item = m_items.at(index);
    m_modelPath = item.modelpath;
    m_sourceCacheKey = item.sourcecachekey;
    m_sourceHash = item.sourcehash;
    m_resourceHash = item.resourcehash;
    m_instance = item.instance;
    m_geometry = item.geometry;
    m_currentInstanceId =
        QString::fromStdString(item.instance.instanceid);
    m_geometryStale = !m_geometry.has_value();
}

void SceneDocument::SyncCurrentItem()
{
    const std::optional<std::size_t> index =
        FindItemIndex(m_currentInstanceId);
    if (!index.has_value() || !m_instance.has_value())
    {
        return;
    }
    SceneDocumentItem& item = m_items[index.value()];
    item.modelpath = m_modelPath;
    item.sourcecachekey = m_sourceCacheKey;
    item.sourcehash = m_sourceHash;
    item.resourcehash = m_resourceHash;
    item.instance = m_instance.value();
    item.geometry = m_geometry;
}

void SceneDocument::AdvanceSceneRevision()
{
    ++m_sceneRevision;
    for (SceneDocumentItem& item : m_items)
    {
        if (item.geometry.has_value())
        {
            item.geometry->scenerevision = m_sceneRevision;
            slicer_core::RefreshSceneViewGeometryHash(
                item.geometry.value());
        }
    }
    if (m_geometry.has_value())
    {
        m_geometry->scenerevision = m_sceneRevision;
        slicer_core::RefreshSceneViewGeometryHash(
            m_geometry.value());
    }
    m_dirty = true;
    m_sceneConfigPath.clear();
    m_effectiveConfigPath.clear();
    m_effectiveConfigHash.clear();
    m_transformedPreflightState =
        SceneTransformedPreflightState::Stale;
    m_transformedPreflightError.clear();
}

void SceneDocument::InvalidateLayoutRestore()
{
    m_layoutSnapshot.reset();
}

void SceneDocument::TranslateGeometry(
    SceneDocumentItem& item,
    const double translateX,
    const double translateY,
    const std::string& transformHash)
{
    if (!item.geometry.has_value())
    {
        return;
    }
    slicer_core::SceneViewGeometry& geometry =
        item.geometry.value();
    for (slicer_core::SceneViewTriangle& triangle :
         geometry.triangles)
    {
        triangle.a.xmm += translateX;
        triangle.a.ymm += translateY;
        triangle.b.xmm += translateX;
        triangle.b.ymm += translateY;
        triangle.c.xmm += translateX;
        triangle.c.ymm += translateY;
    }
    geometry.worldboundsmm.min.xmm += translateX;
    geometry.worldboundsmm.min.ymm += translateY;
    geometry.worldboundsmm.max.xmm += translateX;
    geometry.worldboundsmm.max.ymm += translateY;
    geometry.effectivebboxmm = item.instance.effectivebboxmm;
    geometry.transformrevision =
        item.instance.transformrevision;
    geometry.transformhash = transformHash;
}

bool SceneDocument::SnapshotMatchesCurrentScene() const
{
    if (!m_layoutSnapshot.has_value()
        || m_layoutSnapshot->items.size() != m_items.size())
    {
        return false;
    }
    for (std::size_t index = 0U;
         index < m_items.size();
         ++index)
    {
        if (m_layoutSnapshot->items[index].instanceid
            != QString::fromStdString(
                m_items[index].instance.instanceid))
        {
            return false;
        }
    }
    return true;
}

std::optional<std::size_t> SceneDocument::FindItemIndex(
    const QString& instanceId) const
{
    const auto found = std::find_if(
        m_items.begin(),
        m_items.end(),
        [&instanceId](const SceneDocumentItem& item)
        {
            return QString::fromStdString(item.instance.instanceid)
                == instanceId;
        });
    if (found == m_items.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(
        std::distance(m_items.begin(), found));
}

SceneDocumentOperationResult SceneDocument::ValidateOperation(
    const QString& instanceId,
    const quint64 expectedSceneRevision) const
{
    if (expectedSceneRevision != m_sceneRevision)
    {
        return OperationFailure(
            SceneDocumentOperationErrorCode::SceneRevisionStale,
            QStringLiteral("sceneRevision"),
            QStringLiteral("场景 revision 已变化，请刷新后重试。"));
    }
    if (!FindItemIndex(instanceId).has_value())
    {
        return OperationFailure(
            SceneDocumentOperationErrorCode::InstanceNotFound,
            QStringLiteral("instanceId"),
            QStringLiteral("模型实例不存在。"));
    }
    return {};
}

SceneDocumentOperationResult SceneDocument::OperationFailure(
    const SceneDocumentOperationErrorCode code,
    const QString& field,
    const QString& message) const
{
    return {
        false,
        SceneDocumentOperationError{code, field, message}};
}
