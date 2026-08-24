#include "HostMatvolSettingsPanel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

HostMatvolSettingsPanel::HostMatvolSettingsPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildInterface();
    RefreshEnabledState();
}

void HostMatvolSettingsPanel::BuildInterface()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* header = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    m_enabledCheck = new QCheckBox(
        QStringLiteral("启用多材质纵深体积 RGB（候选）"), header);
    m_enabledCheck->setObjectName(QStringLiteral("hostMatvolEnabledCheck"));
    m_enabledCheck->setChecked(false);
    m_enabledCheck->setToolTip(QStringLiteral(
        "按 Z 解析同一列内的材质归属。开放材质与同级重叠会在构建期 fail closed，"
        "不会静默降级为顶面颜色沿列传播。"));
    m_expandButton = new QToolButton(header);
    m_expandButton->setObjectName(QStringLiteral("hostMatvolExpandButton"));
    m_expandButton->setText(QStringLiteral("收起"));
    m_expandButton->setCheckable(true);
    m_expandButton->setChecked(true);
    headerLayout->addWidget(m_enabledCheck);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_expandButton);
    layout->addWidget(header);

    m_content = new QWidget(this);
    m_content->setObjectName(QStringLiteral("hostMatvolContent"));
    auto* form = new QFormLayout(m_content);
    form->setContentsMargins(12, 0, 0, 0);

    m_primaryNameEdit = new QLineEdit(m_content);
    m_primaryNameEdit->setObjectName(
        QStringLiteral("hostMatvolPrimaryNameEdit"));
    m_primaryNameEdit->setPlaceholderText(QStringLiteral("如 01"));
    m_primaryPrioritySpin = new QSpinBox(m_content);
    m_primaryPrioritySpin->setObjectName(
        QStringLiteral("hostMatvolPrimaryPrioritySpin"));
    m_primaryPrioritySpin->setRange(0, 100000);
    m_primaryPrioritySpin->setValue(200);

    m_secondaryNameEdit = new QLineEdit(m_content);
    m_secondaryNameEdit->setObjectName(
        QStringLiteral("hostMatvolSecondaryNameEdit"));
    m_secondaryNameEdit->setPlaceholderText(QStringLiteral("如 02"));
    m_secondaryPrioritySpin = new QSpinBox(m_content);
    m_secondaryPrioritySpin->setObjectName(
        QStringLiteral("hostMatvolSecondaryPrioritySpin"));
    m_secondaryPrioritySpin->setRange(0, 100000);
    m_secondaryPrioritySpin->setValue(100);

    m_capabilityHint = new QLabel(m_content);
    m_capabilityHint->setObjectName(
        QStringLiteral("hostMatvolCapabilityHint"));
    m_capabilityHint->setWordWrap(true);
    m_capabilityHint->setText(QStringLiteral(
        "重叠必须由显式优先级裁决，数值大者胜；同级重叠一律阻断。"));

    form->addRow(QStringLiteral("高优先级材质名"), m_primaryNameEdit);
    form->addRow(QStringLiteral("高优先级数值"), m_primaryPrioritySpin);
    form->addRow(QStringLiteral("低优先级材质名"), m_secondaryNameEdit);
    form->addRow(QStringLiteral("低优先级数值"), m_secondaryPrioritySpin);
    form->addRow(m_capabilityHint);
    layout->addWidget(m_content);

    connect(
        m_enabledCheck,
        &QCheckBox::toggled,
        this,
        &HostMatvolSettingsPanel::OnEnabledChanged);
    connect(
        m_expandButton,
        &QToolButton::toggled,
        this,
        &HostMatvolSettingsPanel::OnExpandedChanged);
    for (QLineEdit* edit : {m_primaryNameEdit, m_secondaryNameEdit})
    {
        connect(
            edit,
            &QLineEdit::textChanged,
            this,
            &HostMatvolSettingsPanel::OnValueChanged);
    }
    for (QSpinBox* spin : {m_primaryPrioritySpin, m_secondaryPrioritySpin})
    {
        connect(
            spin,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            &HostMatvolSettingsPanel::OnValueChanged);
    }
}

void HostMatvolSettingsPanel::SetSettings(
    const hostmaterialvolumesettings& settings)
{
    const QSignalBlocker enabledBlocker(m_enabledCheck);
    const QSignalBlocker primaryNameBlocker(m_primaryNameEdit);
    const QSignalBlocker primaryPriorityBlocker(m_primaryPrioritySpin);
    const QSignalBlocker secondaryNameBlocker(m_secondaryNameEdit);
    const QSignalBlocker secondaryPriorityBlocker(m_secondaryPrioritySpin);
    m_enabledCheck->setChecked(settings.enabled);
    m_primaryNameEdit->setText(settings.primarymaterialname);
    m_primaryPrioritySpin->setValue(settings.primarypriority);
    m_secondaryNameEdit->setText(settings.secondarymaterialname);
    m_secondaryPrioritySpin->setValue(settings.secondarypriority);
    RefreshEnabledState();
}

hostmaterialvolumesettings HostMatvolSettingsPanel::Settings() const
{
    hostmaterialvolumesettings settings;
    settings.enabled = m_enabledCheck->isChecked();
    settings.primarymaterialname = m_primaryNameEdit->text().trimmed();
    settings.primarypriority = m_primaryPrioritySpin->value();
    settings.secondarymaterialname = m_secondaryNameEdit->text().trimmed();
    settings.secondarypriority = m_secondaryPrioritySpin->value();
    return settings;
}

void HostMatvolSettingsPanel::SetCapabilityRestriction(
    const bool restricted,
    const QString& reason)
{
    m_capabilityRestricted = restricted;
    m_capabilityReason = reason;
    RefreshEnabledState();
}

bool HostMatvolSettingsPanel::IsBlockedByCapability() const
{
    return m_capabilityRestricted && m_enabledCheck->isChecked();
}

QString HostMatvolSettingsPanel::CapabilityBlockReason() const
{
    if (!IsBlockedByCapability())
    {
        return {};
    }
    return m_capabilityReason.isEmpty()
        ? QStringLiteral("当前模型外观资源不足，无法解析逐层材质归属。")
        : m_capabilityReason;
}

void HostMatvolSettingsPanel::OnEnabledChanged()
{
    RefreshEnabledState();
    emit SigSettingsChanged();
}

void HostMatvolSettingsPanel::OnExpandedChanged(const bool expanded)
{
    m_content->setVisible(expanded);
    m_expandButton->setText(
        expanded ? QStringLiteral("收起") : QStringLiteral("展开"));
}

void HostMatvolSettingsPanel::OnValueChanged()
{
    emit SigSettingsChanged();
}

void HostMatvolSettingsPanel::RefreshEnabledState()
{
    /* 能力不足时禁用编辑并把原因上屏，但【不】改写用户已做的选择，
       也不 emit 任何变更信号 —— 回退由操作员决定，不由面板代劳。 */
    const bool editable = m_enabledCheck->isChecked()
        && !m_capabilityRestricted;
    m_enabledCheck->setEnabled(!m_capabilityRestricted);
    m_primaryNameEdit->setEnabled(editable);
    m_primaryPrioritySpin->setEnabled(editable);
    m_secondaryNameEdit->setEnabled(editable);
    m_secondaryPrioritySpin->setEnabled(editable);
    if (m_capabilityRestricted)
    {
        m_capabilityHint->setText(QStringLiteral(
            "多材质纵深不可用：%1 已禁用编辑，请改选单材料工艺后再提交。")
                                      .arg(CapabilityBlockReason().isEmpty()
                                               ? QStringLiteral(
                                                   "当前模型外观资源不足。")
                                               : CapabilityBlockReason()));
        return;
    }
    m_capabilityHint->setText(QStringLiteral(
        "重叠必须由显式优先级裁决，数值大者胜；同级重叠一律阻断。"));
}
