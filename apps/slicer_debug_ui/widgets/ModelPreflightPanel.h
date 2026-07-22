#pragma once

#include "../services/ModelPreflightPresenter.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;

class ModelPreflightPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the read-only Chinese model preflight panel.
     * @param parent Parent widget.
     */
    explicit ModelPreflightPanel(QWidget* parent = nullptr);

    /**
     * @brief Replace the visible state and issue rows.
     * @param presentation Presenter output for the selected mode.
     */
    void ShowPresentation(const ModelPreflightPresentation& presentation);

signals:
    void SigRecheckRequested();
    void SigCancelRequested();

private:
    QLabel* m_stateLabel{nullptr};
    QLabel* m_modeLabel{nullptr};
    QLabel* m_admissionLabel{nullptr};
    QLabel* m_detailLabel{nullptr};
    QTableWidget* m_issueTable{nullptr};
    QPushButton* m_recheckButton{nullptr};
    QPushButton* m_cancelButton{nullptr};
};
