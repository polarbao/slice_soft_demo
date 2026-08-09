#pragma once

#include "HostSliceSettings.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QToolButton;

/** @brief Collapsible host-owned support Profile editor. */
class HostSupportSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Creates the support editor with production lower-support defaults.
     * @param parent Optional Qt parent widget.
     */
    explicit HostSupportSettingsPanel(QWidget* parent = nullptr);

    /**
     * @brief Applies a validated host support draft to the controls.
     * @param settings Host-owned support parameters.
     */
    void SetSettings(const hostsupportsettings& settings);

    /**
     * @brief Returns the support draft represented by the controls.
     * @return Host-owned support parameters without module state.
     */
    [[nodiscard]] hostsupportsettings Settings() const;

signals:
    /** @brief Emitted after an operator changes a support setting. */
    void SigSettingsChanged();

private:
    void BuildInterface();
    void OnSupportEnabledChanged();
    void OnInternalVoidChanged();
    void OnBaseProjectionChanged();
    void OnExpandedChanged(bool expanded);
    void OnValueChanged();
    void RefreshEnabledState();

    QCheckBox* m_enabledCheck{nullptr};
    QToolButton* m_expandButton{nullptr};
    QWidget* m_content{nullptr};
    QComboBox* m_modeCombo{nullptr};
    QDoubleSpinBox* m_offsetSpin{nullptr};
    QSpinBox* m_minAreaSpin{nullptr};
    QCheckBox* m_internalVoidCheck{nullptr};
    QSpinBox* m_internalVoidMinAreaSpin{nullptr};
    QCheckBox* m_baseProjectionCheck{nullptr};
    QSpinBox* m_baseProjectionLayersSpin{nullptr};
};
