#pragma once

#include "../models/SceneDocument.h"
#include "../models/SceneSelectionModel.h"

#include <QWidget>

class QLabel;
class QListWidget;
class QToolButton;

/**
 * @brief Compact list and command surface for the editable model scene.
 */
class ModelListPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the multi-model list panel.
     * @param document Editable scene document.
     * @param selectionModel Shared list/canvas selection.
     * @param parent QWidget owner.
     */
    explicit ModelListPanel(
        SceneDocument* document,
        SceneSelectionModel* selectionModel,
        QWidget* parent = nullptr);

signals:
    /**
     * @brief Request importing another model into the current scene.
     */
    void SigAddRequested();

    /**
     * @brief Publish a user-facing scene command result.
     * @param message Localized status text.
     */
    void SigStatusMessage(const QString& message);

private slots:
    void OnDocumentChanged();
    void OnSelectionChanged(const QString& instanceId);
    void OnCurrentRowChanged(int row);
    void OnDuplicateRequested();
    void OnDeleteRequested();
    void OnVisibilityRequested();
    void OnLockRequested();

private:
    QString SelectedInstanceId() const;
    QString BuildDuplicateInstanceId(
        const QString& sourceInstanceId) const;
    const SceneDocumentItem* FindItem(
        const QString& instanceId) const;
    void ShowResult(
        const SceneDocumentOperationResult& result,
        const QString& successMessage);
    void UpdateButtons();

    SceneDocument* m_document{nullptr};
    SceneSelectionModel* m_selectionModel{nullptr};
    QLabel* m_summaryLabel{nullptr};
    QListWidget* m_modelList{nullptr};
    QToolButton* m_addButton{nullptr};
    QToolButton* m_duplicateButton{nullptr};
    QToolButton* m_deleteButton{nullptr};
    QToolButton* m_visibilityButton{nullptr};
    QToolButton* m_lockButton{nullptr};
    bool m_refreshing{false};
};
