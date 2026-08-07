#pragma once

#include "HostModelImportWorkflow.h"

#include <QStringList>
#include <QWidget>

class QLabel;
class QListWidget;
class QToolButton;

/** @brief Host-owned model instance list with local multi-selection commands. */
class HostModelListPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Creates the model list used by the public-SPI reference host.
     * @param parent Optional Qt parent widget.
     */
    explicit HostModelListPanel(QWidget* parent = nullptr);

    /**
     * @brief Appends one successfully imported model instance.
     * @param result Import metadata returned by HostModelImportWorkflow.
     */
    void AddModel(const hostmodelimportresult& result);

    /**
     * @brief Removes committed instances from the local presentation list.
     * @param instanceIds Stable instance identities removed by the module.
     */
    void RemoveInstances(const QStringList& instanceIds);

    /**
     * @brief Enables or disables commands that cross the public module boundary.
     * @param enabled True when the module is ready and no command is active.
     */
    void SetCommandsEnabled(bool enabled);

    /** @brief Returns the selected stable instance identities. */
    [[nodiscard]] QStringList SelectedInstanceIds() const;

    /** @brief Returns the number of imported instances displayed by the host. */
    [[nodiscard]] int ModelCount() const;

signals:
    /** @brief Requests opening the model import workflow. */
    void SigAddRequested();

    /** @brief Requests atomically removing selected module instances. */
    void SigRemoveRequested(const QStringList& instanceIds);

    /** @brief Publishes host-local selection for view highlighting. */
    void SigSelectionChanged(const QStringList& instanceIds);

private slots:
    void OnSelectAllRequested();
    void OnRemoveRequested();
    void OnSelectionChanged();

private:
    void UpdateControls();

    QLabel* m_summaryLabel{nullptr};
    QListWidget* m_modelList{nullptr};
    QToolButton* m_addButton{nullptr};
    QToolButton* m_selectAllButton{nullptr};
    QToolButton* m_removeButton{nullptr};
    bool m_commandsEnabled{false};
};
