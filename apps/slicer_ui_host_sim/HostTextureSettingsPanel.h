#pragma once

#include "HostSliceSettings.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;
class QToolButton;

/** @brief 由宿主持有的生产纹理与白色载体字段编辑器。 */
class HostTextureSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 创建独立纹理 Profile 编辑器。
     * @param parent 可选的 Qt 父控件。
     */
    explicit HostTextureSettingsPanel(QWidget* parent = nullptr);

    /**
     * @brief 应用已校验的宿主纹理草稿。
     * @param settings 待显示的纹理设置。
     */
    void SetSettings(const hosttexturesettings& settings);

    /**
     * @brief 返回当前编辑的纹理设置。
     * @return 不含模块状态的宿主侧设置。
     */
    [[nodiscard]] hosttexturesettings Settings() const;

signals:
    /** @brief 本地纹理设置变化后发出。 */
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
