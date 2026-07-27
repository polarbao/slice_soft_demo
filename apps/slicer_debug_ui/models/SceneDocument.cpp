#include "SceneDocument.h"

#include <utility>

SceneDocument::SceneDocument(QObject* parent)
    : QObject(parent)
{
}

void SceneDocument::Reset()
{
    ++m_generation;
    m_modelPath.clear();
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
    PublishState(SceneDocumentState::Unloaded);
}

void SceneDocument::SetLoading(
    const quint64 generation,
    const QString& modelPath)
{
    m_generation = generation;
    m_modelPath = modelPath;
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
    m_dirty = false;
    m_geometryStale = true;
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
    m_geometryStale = true;
    PublishState(SceneDocumentState::Failed);
    return true;
}

bool SceneDocument::SetCancelled(const quint64 generation)
{
    if (generation != m_generation)
    {
        return false;
    }

    m_error.clear();
    m_geometryStale = true;
    PublishState(SceneDocumentState::Cancelled);
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
    ++m_sceneRevision;
    m_dirty = true;
    m_geometryStale = true;
    m_effectiveConfigPath.clear();
    m_effectiveConfigHash.clear();
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
