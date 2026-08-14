#pragma once

#include "HostSliceSettings.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QToolButton;

/** @brief 可折叠的宿主侧材料工艺 Profile 编辑器。 */
class HostMaterialSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 使用 RGB 实体生产默认值创建编辑器。
     * @param parent 可选的 Qt 父控件。
     */
    explicit HostMaterialSettingsPanel(QWidget* parent = nullptr);

    /**
     * @brief 将已校验的材料工艺草稿应用到控件。
     * @param strategy 宿主材料输出策略。
     * @param settings 由宿主持有的角色映射与工艺参数。
     */
    void SetSettings(
        HostMaterialStrategy strategy,
        const hostmaterialprocesssettings& settings);

    /**
     * @brief 限定材料策略只允许单材料白墨或单材料光油。
     * @param enabled true 时禁用所有 RGB 相关策略。
     * @param reason 显示给操作员的外观资源不完整原因。
     */
    void SetSingleMaterialOnly(bool enabled, const QString& reason);

    /**
     * @brief 返回选中的材料输出策略。
     * @return 控件当前表示的宿主材料策略。
     */
    [[nodiscard]] HostMaterialStrategy Strategy() const;

    /**
     * @brief 返回控件当前表示的材料工艺草稿。
     * @return 不含模块状态的宿主侧材料工艺参数。
     */
    [[nodiscard]] hostmaterialprocesssettings Settings() const;

signals:
    /** @brief 操作员更改材料设置后发出。 */
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
    bool m_singleMaterialOnly{false};
    QString m_singleMaterialReason;
};
