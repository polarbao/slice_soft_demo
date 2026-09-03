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
    /**
     * @brief 按材质命名自动推导优先级；勾选后 primary/secondary 两个名字槽停用。
     *
     * 多图层资产的材质数超过两个名字槽，手填不可行，故自动模式是其唯一可用路径。
     */
    QCheckBox* m_autoByNameCheck{nullptr};
    /**
     * @brief 以下三项由工艺预设携带，面板不提供控件但必须【透传】。
     *
     * MO-11 新增这些字段时只接了 ABI 与预设，未在本面板存取，
     * 导致预设设定的值在 Settings() 处被静默替换为默认值：
     * 自动模式退化为手填模式（进而因名字为空而校验失败），
     * 光油映射与退化面阈值一并丢失。UI 路径的多图层能力因此完全不可用。
     */
    bool m_opacityVarnishEnabled{false};
    double m_opacityVarnishMax{0.001};
    double m_degenerateAreaEpsilonMm2{0.0};
    QLabel* m_capabilityHint{nullptr};
    bool m_capabilityRestricted{false};
    QString m_capabilityReason;
};
