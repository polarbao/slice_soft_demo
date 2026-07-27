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

    m_error.clear();
    m_geometry = std::move(geometry);
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
    m_geometry.reset();
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
    m_geometry.reset();
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

void SceneDocument::PublishState(const SceneDocumentState state)
{
    m_state = state;
    emit SigChanged();
}
