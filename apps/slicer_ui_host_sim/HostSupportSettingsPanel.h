#pragma once

#include "HostSliceSettings.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QToolButton;

/** @brief 可折叠的宿主侧支撑 Profile 编辑器。 */
class HostSupportSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 使用生产下方支撑默认值创建支撑编辑器。
     * @param parent 可选的 Qt 父控件。
     */
    explicit HostSupportSettingsPanel(QWidget* parent = nullptr);

    /**
     * @brief 将已校验的宿主支撑草稿应用到控件。
     * @param settings 由宿主持有的支撑参数。
     */
    void SetSettings(const hostsupportsettings& settings);

    /**
     * @brief 返回控件当前表示的支撑草稿。
     * @return 不含模块状态的宿主侧支撑参数。
     */
    [[nodiscard]] hostsupportsettings Settings() const;

signals:
    /** @brief 操作员更改支撑设置后发出。 */
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
