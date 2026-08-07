#pragma once

#include <QStringList>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

/** @brief Host-side controls for committed instance transforms and grid layout. */
class HostTransformLayoutPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Creates the transform and layout controls.
     * @param parent Optional Qt parent widget.
     */
    explicit HostTransformLayoutPanel(QWidget* parent = nullptr);

    /**
     * @brief Updates the host-local instance selection without a module call.
     * @param instanceIds Stable selected instance identities.
     */
    void SetSelectedInstances(const QStringList& instanceIds);

    /**
     * @brief Updates the authoritative scene summary shown by the panel.
     * @param instanceCount Number of host-tracked scene instances.
     * @param sceneRevision Latest committed scene revision.
     */
    void SetSceneState(int instanceCount, quint64 sceneRevision);

    /**
     * @brief Enables commands that cross the public module boundary.
     * @param enabled True when the module is ready and no command is active.
     */
    void SetCommandsEnabled(bool enabled);

    /** @brief Resets incremental transform inputs after a successful Commit. */
    void ResetTransformInputs();

signals:
    /** @brief Requests one atomic Commit for selected instance transforms. */
    void SigTransformRequested(
        const QStringList& instanceIds,
        double deltaXMm,
        double deltaYMm,
        double deltaZMm,
        double rotateZDegrees,
        double uniformScaleFactor,
        bool mirrorX,
        bool mirrorY);

    /** @brief Requests one authoritative applyGridLayout Commit. */
    void SigLayoutRequested(
        int maxColumns,
        int maxRows,
        double columnGapMm,
        double rowGapMm);

private slots:
    void OnApplyTransform();
    void OnApplyLayout();

private:
    void UpdateControls();

    QStringList m_selectedInstanceIds;
    QLabel* m_selectionLabel{nullptr};
    QLabel* m_sceneLabel{nullptr};
    QDoubleSpinBox* m_deltaXSpin{nullptr};
    QDoubleSpinBox* m_deltaYSpin{nullptr};
    QDoubleSpinBox* m_deltaZSpin{nullptr};
    QDoubleSpinBox* m_rotateZSpin{nullptr};
    QDoubleSpinBox* m_scaleSpin{nullptr};
    QCheckBox* m_mirrorXCheck{nullptr};
    QCheckBox* m_mirrorYCheck{nullptr};
    QPushButton* m_applyTransformButton{nullptr};
    QSpinBox* m_columnsSpin{nullptr};
    QSpinBox* m_rowsSpin{nullptr};
    QDoubleSpinBox* m_columnGapSpin{nullptr};
    QDoubleSpinBox* m_rowGapSpin{nullptr};
    QPushButton* m_applyLayoutButton{nullptr};
    int m_instanceCount{0};
    quint64 m_sceneRevision{0};
    bool m_commandsEnabled{false};
};
