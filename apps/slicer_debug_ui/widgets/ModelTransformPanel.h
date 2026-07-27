#pragma once

#include "../controllers/SceneTransformController.h"
#include "../models/SceneDocument.h"
#include "../models/SceneSelectionModel.h"

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QPushButton;

/**
 * @brief Precise single-instance transform editor for the +Z workspace.
 */
class ModelTransformPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the transform panel and bind shared scene state.
     * @param document Editable scene document.
     * @param selectionModel Shared scene selection.
     * @param controller Validated transform command controller.
     * @param parent QWidget owner.
     */
    explicit ModelTransformPanel(
        SceneDocument* document,
        SceneSelectionModel* selectionModel,
        SceneTransformController* controller,
        QWidget* parent = nullptr);

signals:
    void SigSaveRequested();
    void SigStatusMessage(const QString& message);

private slots:
    void OnApply();
    void OnCenter();
    void OnReset();
    void OnMirrorX();
    void OnMirrorY();
    void OnDocumentChanged();
    void OnSelectionChanged(const QString& instanceId);
    void OnCommandFailed(const QString& code, const QString& message);

private:
    void SyncFields();
    void UpdateAvailability();
    void ShowCommandResult(
        const SceneTransformCommandResult& result,
        const QString& successMessage);

    SceneDocument* m_document{nullptr};
    SceneSelectionModel* m_selectionModel{nullptr};
    SceneTransformController* m_controller{nullptr};
    QDoubleSpinBox* m_translateXSpin{nullptr};
    QDoubleSpinBox* m_translateYSpin{nullptr};
    QDoubleSpinBox* m_rotateZSpin{nullptr};
    QDoubleSpinBox* m_uniformScaleSpin{nullptr};
    QLabel* m_identityLabel{nullptr};
    QLabel* m_revisionLabel{nullptr};
    QLabel* m_sourcePreflightLabel{nullptr};
    QLabel* m_transformedPreflightLabel{nullptr};
    QLabel* m_stateLabel{nullptr};
    QPushButton* m_applyButton{nullptr};
    QPushButton* m_centerButton{nullptr};
    QPushButton* m_resetButton{nullptr};
    QPushButton* m_mirrorXButton{nullptr};
    QPushButton* m_mirrorYButton{nullptr};
    QPushButton* m_saveButton{nullptr};
};
