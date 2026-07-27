#pragma once

#include "../models/SceneDocument.h"

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

/**
 * @brief Compact editor for deterministic 11x2 scene layout.
 */
class SceneLayoutPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the scene layout panel.
     * @param document Editable scene document.
     * @param parent QWidget owner.
     */
    explicit SceneLayoutPanel(
        SceneDocument* document,
        QWidget* parent = nullptr);

signals:
    /**
     * @brief Publish a localized layout command result.
     * @param message User-facing status text.
     */
    void SigStatusMessage(const QString& message);

private slots:
    void OnDocumentChanged();
    void OnApplyRequested();
    void OnRestoreRequested();

private:
    void ShowResult(
        const SceneDocumentOperationResult& result,
        const QString& successMessage);

    SceneDocument* m_document{nullptr};
    QLabel* m_summaryLabel{nullptr};
    QSpinBox* m_columnCountSpin{nullptr};
    QSpinBox* m_rowCountSpin{nullptr};
    QDoubleSpinBox* m_columnGapSpin{nullptr};
    QDoubleSpinBox* m_rowGapSpin{nullptr};
    QPushButton* m_applyButton{nullptr};
    QPushButton* m_restoreButton{nullptr};
};
