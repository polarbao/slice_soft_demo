#pragma once

#include "HostSliceSettings.h"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

/** @brief Host-owned slice settings editor and effective Profile preview. */
class HostSliceSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Creates an editor with production-oriented reference defaults.
     * @param parent Optional Qt parent widget.
     */
    explicit HostSliceSettingsPanel(QWidget* parent = nullptr);

    /**
     * @brief Updates the host Profile selected by H-B-04.
     * @param profileId Available host Profile identity.
     * @param supportsSlice True when the Profile requires slice.rgbwsv.
     */
    void SetSelectedProfileId(
        const QString& profileId,
        bool supportsSlice = true);

    /**
     * @brief Updates the model used by the effective Profile preview.
     * @param modelPath Existing imported OBJ or 3MF path, or empty.
     */
    void SetModelPath(const QString& modelPath);

    /**
     * @brief Records the authoritative context of an already-created scene.
     * @param bound True when scene context is immutable for this session.
     * @param profileId Profile identity committed with the scene.
     * @param buildVolume Device build volume committed with the scene.
     */
    void SetSceneAuthority(
        bool bound,
        const QString& profileId,
        const hostbuildvolume& buildVolume);

    /**
     * @brief Applies validated user preferences before a scene is created.
     * @param settings Persisted host-owned parameters; model identity is ignored.
     * @return This function does not return a value.
     */
    void SetPersistentSettings(const hostslicesettings& settings);

    /**
     * @brief Returns all host-owned values currently shown in the editor.
     * @return Settings snapshot independent from module state.
     */
    [[nodiscard]] hostslicesettings Settings() const;

    /**
     * @brief Returns whether an exact effective Profile is ready.
     * @return True only after model, path and scene-binding validation pass.
     */
    [[nodiscard]] bool IsReady() const;

    /**
     * @brief Returns the latest validated effective Profile.
     * @return Empty object when IsReady is false.
     */
    [[nodiscard]] hosteffectiveprofile EffectiveProfile() const;

signals:
    /** @brief Emitted after an operator changes a local slice setting. */
    void SigSettingsChanged();

private:
    void BuildInterface();
    void OnBrowseOutput();
    void OnSettingsEdited();
    void RefreshPreview();
    bool ValidateSceneBinding(QString* error) const;

    QLabel* m_profileLabel{nullptr};
    QSpinBox* m_dpiXSpin{nullptr};
    QSpinBox* m_dpiYSpin{nullptr};
    QDoubleSpinBox* m_layerThicknessSpin{nullptr};
    QLineEdit* m_outputEdit{nullptr};
    QPushButton* m_outputBrowseButton{nullptr};
    QComboBox* m_materialCombo{nullptr};
    QDoubleSpinBox* m_buildWidthSpin{nullptr};
    QDoubleSpinBox* m_buildHeightSpin{nullptr};
    QDoubleSpinBox* m_buildZSpin{nullptr};
    QLabel* m_validationLabel{nullptr};
    QPlainTextEdit* m_profilePreview{nullptr};
    QString m_profileId;
    bool m_profileSupportsSlice{true};
    QString m_modelPath;
    bool m_sceneBound{false};
    QString m_sceneProfileId;
    hostbuildvolume m_sceneBuildVolume;
    hosteffectiveprofile m_effectiveProfile;
};
