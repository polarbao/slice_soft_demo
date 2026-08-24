#pragma once

#include "HostSliceSettings.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QToolButton;

/** @brief 可折叠的宿主侧多材质纵深体积 Profile 编辑器。 */
class HostMatvolSettingsPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 使用默认关闭的多材质纵深参数创建编辑器。
     * @param parent 可选的 Qt 父控件。
     */
    explicit HostMatvolSettingsPanel(QWidget* parent = nullptr);

    /**
     * @brief 将已校验的宿主多材质纵深草稿应用到控件。
     * @param settings 由宿主持有的多材质纵深参数。
     */
    void SetSettings(const hostmaterialvolumesettings& settings);

    /**
     * @brief 返回控件当前表示的多材质纵深草稿。
     * @return 不含模块状态的宿主侧多材质纵深参数。
     */
    [[nodiscard]] hostmaterialvolumesettings Settings() const;

    /**
     * @brief 按资产能力禁用编辑并展示原因，不改写用户选择。
     *
     * 与既有单材料限制的静默回落不同：本面板在能力不足时保留用户输入、
     * 禁用控件并把原因上屏，由提交前校验 fail closed。
     * @param restricted 资产能力是否不足。
     * @param reason 面向操作员的中文原因，可为空。
     */
    void SetCapabilityRestriction(bool restricted, const QString& reason);

    /**
     * @brief 当前是否因资产能力不足而不可提交。
     * @return 启用了多材质纵深但资产能力不足时返回 true。
     */
    [[nodiscard]] bool IsBlockedByCapability() const;

    /**
     * @brief 面向操作员的不可提交原因。
     * @return 能力不足时的中文原因，否则为空串。
     */
    [[nodiscard]] QString CapabilityBlockReason() const;

signals:
    /** @brief 操作员更改多材质纵深设置后发出。 */
    void SigSettingsChanged();

private:
    void BuildInterface();
    void OnEnabledChanged();
    void OnExpandedChanged(bool expanded);
    void OnValueChanged();
    void RefreshEnabledState();

    QCheckBox* m_enabledCheck{nullptr};
    QToolButton* m_expandButton{nullptr};
    QWidget* m_content{nullptr};
    QLineEdit* m_primaryNameEdit{nullptr};
    QSpinBox* m_primaryPrioritySpin{nullptr};
    QLineEdit* m_secondaryNameEdit{nullptr};
    QSpinBox* m_secondaryPrioritySpin{nullptr};
    QLabel* m_capabilityHint{nullptr};
    bool m_capabilityRestricted{false};
    QString m_capabilityReason;
};
