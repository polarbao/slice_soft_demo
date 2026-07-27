#pragma once

#include <QObject>
#include <QString>

class SceneSelectionModel final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Create the scene selection model.
     * @param parent QObject owner.
     */
    explicit SceneSelectionModel(QObject* parent = nullptr);

    /**
     * @brief Select one scene instance.
     * @param instanceId Stable instance identity.
     */
    void SetSelectedInstance(const QString& instanceId);

    /**
     * @brief Clear the current selection.
     */
    void Clear();

    /**
     * @brief Return the selected instance identity.
     * @return Empty text when no instance is selected.
     */
    QString SelectedInstance() const;

signals:
    void SigSelectionChanged(const QString& instanceId);

private:
    QString m_selectedInstance;
};
