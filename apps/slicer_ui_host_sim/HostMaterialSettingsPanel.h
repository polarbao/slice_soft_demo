#pragma once

#include "HostSliceSettings.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QToolButton;

/** @brief Collapsible host-owned material process Profile editor. */
class HostMaterialSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Creates the editor with RGB solid production defaults.
     * @param parent Optional Qt parent widget.
     */
    explicit HostMaterialSettingsPanel(QWidget* parent = nullptr);

    /**
     * @brief Applies a validated material process draft to the controls.
     * @param strategy Host material output strategy.
     * @param settings Host-owned role mapping and process parameters.
     */
    void SetSettings(
        HostMaterialStrategy strategy,
        const hostmaterialprocesssettings& settings);

    /**
     * @brief Returns the selected material output strategy.
     * @return Host material strategy represented by the controls.
     */
    [[nodiscard]] HostMaterialStrategy Strategy() const;

    /**
     * @brief Returns the material process draft represented by the controls.
     * @return Host-owned material process parameters without module state.
     */
    [[nodiscard]] hostmaterialprocesssettings Settings() const;

signals:
    /** @brief Emitted after an operator changes a material setting. */
    void SigSettingsChanged();

private:
    void BuildInterface();
    void OnRoleMappingChanged();
    void OnExpandedChanged(bool expanded);
    void OnValueChanged();
    void RefreshEnabledState();

    QToolButton* m_expandButton{nullptr};
    QWidget* m_content{nullptr};
    QComboBox* m_strategyCombo{nullptr};
    QCheckBox* m_roleMappingCheck{nullptr};
    QComboBox* m_defaultRoleCombo{nullptr};
    QCheckBox* m_mapWhiteNamesCheck{nullptr};
    QCheckBox* m_mapVarnishNamesCheck{nullptr};
    QCheckBox* m_allowInputSupportCheck{nullptr};
    QSpinBox* m_whiteExpandSpin{nullptr};
    QSpinBox* m_whiteShrinkSpin{nullptr};
    QSpinBox* m_varnishTopLayersSpin{nullptr};
    QSpinBox* m_maxOverlapSpin{nullptr};
};
