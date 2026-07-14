#include "SettingHelpPanel.h"

#include "../services/HelpTextProvider.h"

#include <QComboBox>
#include <QFormLayout>
#include <QPlainTextEdit>
#include <QVBoxLayout>

SettingHelpPanel::SettingHelpPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("settingHelpPanel"));

    auto* layout = new QVBoxLayout(this);
    auto* selectorForm = new QFormLayout();
    m_settingSelector = new QComboBox(this);
    m_settingSelector->setObjectName(QStringLiteral("settingHelpSelector"));
    m_settingSelector->setMinimumContentsLength(24);

    const QVector<SettingHelpMetadata>& entries = HelpTextProvider::All();
    for (const SettingHelpMetadata& entry : entries)
    {
        m_settingSelector->addItem(entry.title, entry.key);
        m_settingSelector->setItemData(
            m_settingSelector->count() - 1,
            entry.ToolTipText(),
            Qt::ToolTipRole);
    }
    selectorForm->addRow(QStringLiteral("设置项"), m_settingSelector);
    layout->addLayout(selectorForm);

    m_detailView = new QPlainTextEdit(this);
    m_detailView->setObjectName(QStringLiteral("settingHelpDetail"));
    m_detailView->setReadOnly(true);
    m_detailView->setMinimumHeight(240);
    layout->addWidget(m_detailView, 1);

    connect(
        m_settingSelector,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &SettingHelpPanel::OnSettingChanged);

    OnSettingChanged(m_settingSelector->currentIndex());
}

bool SettingHelpPanel::SelectKey(const QString& key)
{
    const int index = m_settingSelector->findData(key);
    if (index < 0)
    {
        return false;
    }
    m_settingSelector->setCurrentIndex(index);
    return true;
}

QString SettingHelpPanel::CurrentText() const
{
    return m_detailView->toPlainText();
}

void SettingHelpPanel::OnSettingChanged(const int index)
{
    const QString key = m_settingSelector->itemData(index).toString();
    const SettingHelpMetadata* metadata = HelpTextProvider::Find(key);
    m_detailView->setPlainText(
        metadata == nullptr
            ? QStringLiteral("未找到该设置项的说明。")
            : metadata->DetailText());
}
