#include "SceneSelectionModel.h"

SceneSelectionModel::SceneSelectionModel(QObject* parent)
    : QObject(parent)
{
}

void SceneSelectionModel::SetSelectedInstance(const QString& instanceId)
{
    if (m_selectedInstance == instanceId)
    {
        return;
    }
    m_selectedInstance = instanceId;
    emit SigSelectionChanged(m_selectedInstance);
}

void SceneSelectionModel::Clear()
{
    SetSelectedInstance({});
}

QString SceneSelectionModel::SelectedInstance() const
{
    return m_selectedInstance;
}
