#pragma once

#include "HostRipSettingsStore.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

/** @brief RIP configuration and job controls for the reference host. */
class HostRipSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit HostRipSettingsPanel(QWidget* parent = nullptr);

    [[nodiscard]] hostripsettings Settings() const;
    void SetSettings(const hostripsettings& settings);
    void SetModuleDirectory(const QString& directory);
    void SetPackageDirectory(const QString& directory);
    [[nodiscard]] QString PackageDirectory() const;
    void SetRuntimeStatus(bool valid, const QString& message);
    void SetRequestStatus(bool valid, const QString& message);
    void SetJobActive(bool active);
    void ShowJobState(const QString& state, const QString& message);
    void ShowCompletion(
        bool success,
        bool cancelled,
        const QString& message,
        const QString& outputDirectory);

signals:
    void SigSettingsChanged();
    void SigRunRequested();
    void SigCancelRequested();
    void SigOpenOutputRequested(QString outputDirectory);

private:
    void BuildInterface();
    void UpdateOutputPath();
    void RefreshControls();

    QCheckBox* m_autoCheck{nullptr};
    QComboBox* m_intentCombo{nullptr};
    QComboBox* m_transparentCombo{nullptr};
    QComboBox* m_colorModeCombo{nullptr};
    QComboBox* m_inputIccCombo{nullptr};
    QComboBox* m_outputIccCombo{nullptr};
    QCheckBox* m_continueCheck{nullptr};
    QComboBox* m_grayBitsCombo{nullptr};
    QComboBox* m_outputValidationCombo{nullptr};
    QSpinBox* m_timeoutSpin{nullptr};
    QLineEdit* m_modulePathEdit{nullptr};
    QLineEdit* m_inputPathEdit{nullptr};
    QLineEdit* m_outputPathEdit{nullptr};
    QLabel* m_runtimeStatusLabel{nullptr};
    QLabel* m_jobStatusLabel{nullptr};
    QPushButton* m_runButton{nullptr};
    QPushButton* m_cancelButton{nullptr};
    QPushButton* m_openButton{nullptr};
    QString m_packageDirectory;
    QString m_outputDirectory;
    bool m_outputExists{false};
    bool m_runtimeValid{false};
    bool m_requestValid{false};
    bool m_jobActive{false};
};
