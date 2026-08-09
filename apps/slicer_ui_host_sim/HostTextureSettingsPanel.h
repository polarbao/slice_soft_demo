#pragma once

#include "HostSliceSettings.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;
class QToolButton;

/** @brief Host-owned editor for production texture and white-carrier fields. */
class HostTextureSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Creates an independent texture Profile editor.
     * @param parent Optional Qt parent widget.
     */
    explicit HostTextureSettingsPanel(QWidget* parent = nullptr);

    /**
     * @brief Applies a validated host texture draft.
     * @param settings Texture settings to display.
     */
    void SetSettings(const hosttexturesettings& settings);

    /**
     * @brief Returns the currently edited texture settings.
     * @return Host-owned settings without module state.
     */
    [[nodiscard]] hosttexturesettings Settings() const;

signals:
    /** @brief Emitted after a local texture setting changes. */
    void SigSettingsChanged();

private:
    void BuildInterface();
    void OnExpandedChanged(bool expanded);
    void OnSettingsEdited();
    void RefreshEnabledState();

    QCheckBox* m_enabledCheck{nullptr};
    QToolButton* m_expandButton{nullptr};
    QWidget* m_content{nullptr};
    QComboBox* m_applyModeCombo{nullptr};
    QSpinBox* m_topLayersSpin{nullptr};
    QComboBox* m_samplerCombo{nullptr};
    QComboBox* m_uvAddressCombo{nullptr};
    QCheckBox* m_flipVCheck{nullptr};
    QComboBox* m_missingPolicyCombo{nullptr};
    QComboBox* m_nonSurfacePolicyCombo{nullptr};
    QSpinBox* m_fallbackRedSpin{nullptr};
    QSpinBox* m_fallbackGreenSpin{nullptr};
    QSpinBox* m_fallbackBlueSpin{nullptr};
    QComboBox* m_whitePolicyCombo{nullptr};
    QSpinBox* m_whiteThresholdSpin{nullptr};
    QSpinBox* m_whiteValueSpin{nullptr};
    QLabel* m_hintLabel{nullptr};
};
