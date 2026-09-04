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

    // 自动模式：由材质命名推导逐材质优先级，primary/secondary 随之停用。
    // 多图层资产的材质数超过这两个名字槽，手填不可行，故自动模式是其唯一路径。
    m_autoByNameCheck = new QCheckBox(
        QStringLiteral("按材质命名自动推导优先级（多图层必选）"), m_content);
    m_autoByNameCheck->setObjectName(
        QStringLiteral("hostMatvolAutoByNameCheck"));
    m_autoByNameCheck->setChecked(false);
    m_autoByNameCheck->setToolTip(QStringLiteral(
        "材质名须形如 <素材名>-L<层号>，优先级按层序主导自动生成，"
        "无需填写下方两个材质名。命名违规或撞号会 fail closed。"));

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

    form->addRow(m_autoByNameCheck);
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
        m_autoByNameCheck,
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
    const QSignalBlocker autoByNameBlocker(m_autoByNameCheck);
    const QSignalBlocker primaryNameBlocker(m_primaryNameEdit);
    const QSignalBlocker primaryPriorityBlocker(m_primaryPrioritySpin);
    const QSignalBlocker secondaryNameBlocker(m_secondaryNameEdit);
    const QSignalBlocker secondaryPriorityBlocker(m_secondaryPrioritySpin);
    m_enabledCheck->setChecked(settings.enabled);
    m_autoByNameCheck->setChecked(settings.overlapautobyname);
    m_primaryNameEdit->setText(settings.primarymaterialname);
    m_primaryPrioritySpin->setValue(settings.primarypriority);
    m_secondaryNameEdit->setText(settings.secondarymaterialname);
    m_secondaryPrioritySpin->setValue(settings.secondarypriority);
    // 无控件的三项按原值透传，不得在此丢弃——见头文件的说明。
    m_opacityVarnishEnabled = settings.opacityvarnishenabled;
    m_opacityVarnishMax = settings.opacityvarnishmax;
    m_degenerateAreaEpsilonMm2 = settings.degenerateareaepsilonmm2;
    RefreshEnabledState();
}

hostmaterialvolumesettings HostMatvolSettingsPanel::Settings() const
{
    hostmaterialvolumesettings settings;
    settings.enabled = m_enabledCheck->isChecked();
    settings.overlapautobyname = m_autoByNameCheck->isChecked();
    settings.primarymaterialname = m_primaryNameEdit->text().trimmed();
    settings.primarypriority = m_primaryPrioritySpin->value();
    settings.secondarymaterialname = m_secondaryNameEdit->text().trimmed();
    settings.secondarypriority = m_secondaryPrioritySpin->value();
    settings.opacityvarnishenabled = m_opacityVarnishEnabled;
    settings.opacityvarnishmax = m_opacityVarnishMax;
    settings.degenerateareaepsilonmm2 = m_degenerateAreaEpsilonMm2;
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
    // 自动模式下 primary/secondary 不参与裁决，停用以免误导操作员填写。
    const bool manualEditable = editable && !m_autoByNameCheck->isChecked();
    m_enabledCheck->setEnabled(!m_capabilityRestricted);
    m_autoByNameCheck->setEnabled(editable);
    m_primaryNameEdit->setEnabled(manualEditable);
    m_primaryPrioritySpin->setEnabled(manualEditable);
    m_secondaryNameEdit->setEnabled(manualEditable);
    m_secondaryPrioritySpin->setEnabled(manualEditable);
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
