#include "HostProfilePanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace
{
QString SafetyText(const QString& safety)
{
    if (safety == QStringLiteral("production"))
    {
        return QStringLiteral("生产可用");
    }
    if (safety == QStringLiteral("restricted"))
    {
        return QStringLiteral("受限候选");
    }
    return QStringLiteral("仅诊断");
}
}

HostProfilePanel::HostProfilePanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("hostProfilePanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* form = new QFormLayout();
    m_profileCombo = new QComboBox(this);
    m_profileCombo->setObjectName(QStringLiteral("hostProfileCombo"));
    m_profileCombo->setToolTip(QStringLiteral(
        "Profile 目录由打印宿主提供；可用性由模块 ABI 能力求交决定。"));
    form->addRow(QStringLiteral("工艺 Profile"), m_profileCombo);

    m_safetyLabel = new QLabel(QStringLiteral("未解析"), this);
    m_safetyLabel->setObjectName(QStringLiteral("hostProfileSafetyLabel"));
    form->addRow(QStringLiteral("生产安全级别"), m_safetyLabel);

    m_availabilityLabel = new QLabel(QStringLiteral("等待模块能力"), this);
    m_availabilityLabel->setObjectName(
        QStringLiteral("hostProfileAvailabilityLabel"));
    m_availabilityLabel->setWordWrap(true);
    form->addRow(QStringLiteral("能力状态"), m_availabilityLabel);
    layout->addLayout(form);

    m_descriptionLabel = new QLabel(this);
    m_descriptionLabel->setObjectName(
        QStringLiteral("hostProfileDescriptionLabel"));
    m_descriptionLabel->setWordWrap(true);
    layout->addWidget(m_descriptionLabel);

    m_capabilityView = new QPlainTextEdit(this);
    m_capabilityView->setObjectName(
        QStringLiteral("hostProfileCapabilityView"));
    m_capabilityView->setReadOnly(true);
    m_capabilityView->setMaximumBlockCount(20);
    m_capabilityView->setPlaceholderText(
        QStringLiteral("此处显示标签与所需模块能力。"));
    layout->addWidget(m_capabilityView, 1);

    connect(
        m_profileCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &HostProfilePanel::OnProfileChanged);
    setEnabled(false);
}

void HostProfilePanel::SetProfiles(
    const hostprofilecatalogresolution& resolution)
{
    const QSignalBlocker blocker(m_profileCombo);
    m_profiles = resolution.profiles;
    m_profileCombo->clear();
    int firstAvailableIndex = -1;
    auto* model = qobject_cast<QStandardItemModel*>(m_profileCombo->model());
    for (int index = 0; index < m_profiles.size(); ++index)
    {
        const hostprofileavailability& availability = m_profiles.at(index);
        const QString suffix = availability.available
            ? SafetyText(availability.profile.productionsafety)
            : QStringLiteral("能力不足");
        m_profileCombo->addItem(
            QStringLiteral("%1 [%2]")
                .arg(availability.profile.displayname, suffix),
            availability.profile.profileid);
        if (!availability.available && model != nullptr)
        {
            model->item(index)->setEnabled(false);
        }
        else if (firstAvailableIndex < 0)
        {
            firstAvailableIndex = index;
        }
    }
    m_profileCombo->setCurrentIndex(firstAvailableIndex);
    setEnabled(firstAvailableIndex >= 0);
    RefreshPresentation();
    if (firstAvailableIndex >= 0)
    {
        emit SigProfileChanged(SelectedProfileId());
    }
}

void HostProfilePanel::ClearProfiles(const QString& reason)
{
    const QSignalBlocker blocker(m_profileCombo);
    m_profiles.clear();
    m_profileCombo->clear();
    m_safetyLabel->setText(QStringLiteral("不可用"));
    m_availabilityLabel->setText(reason);
    m_descriptionLabel->clear();
    m_capabilityView->clear();
    setEnabled(false);
}

bool HostProfilePanel::SelectProfile(const QString& profileId)
{
    for (int index = 0; index < m_profiles.size(); ++index)
    {
        if (m_profiles.at(index).profile.profileid == profileId
            && m_profiles.at(index).available)
        {
            m_profileCombo->setCurrentIndex(index);
            return true;
        }
    }
    return false;
}

QString HostProfilePanel::SelectedProfileId() const
{
    const hostprofileavailability* profile = CurrentProfile();
    return profile != nullptr && profile->available
        ? profile->profile.profileid : QString{};
}

int HostProfilePanel::AvailableProfileCount() const
{
    int count = 0;
    for (const hostprofileavailability& profile : m_profiles)
    {
        count += profile.available ? 1 : 0;
    }
    return count;
}

void HostProfilePanel::OnProfileChanged(const int index)
{
    Q_UNUSED(index)
    RefreshPresentation();
    const QString profileId = SelectedProfileId();
    if (!profileId.isEmpty())
    {
        emit SigProfileChanged(profileId);
    }
}

void HostProfilePanel::RefreshPresentation()
{
    const hostprofileavailability* availability = CurrentProfile();
    if (availability == nullptr)
    {
        m_safetyLabel->setText(QStringLiteral("不可用"));
        m_availabilityLabel->setText(QStringLiteral("没有可选择的 Profile。"));
        m_descriptionLabel->clear();
        m_capabilityView->clear();
        return;
    }

    m_safetyLabel->setText(SafetyText(
        availability->profile.productionsafety));
    m_availabilityLabel->setText(
        availability->available
            ? QStringLiteral("模块能力满足全部要求")
            : QStringLiteral("缺少能力：%1").arg(
                  availability->missingcapabilities.join(", ")));
    m_descriptionLabel->setText(availability->profile.description);
    m_capabilityView->setPlainText(
        QStringLiteral("标签：%1\n\n所需模块能力：\n%2")
            .arg(
                availability->profile.tags.join(QStringLiteral(" · ")),
                availability->profile.requiredcapabilities.join(
                    QStringLiteral("\n"))));
}

const hostprofileavailability* HostProfilePanel::CurrentProfile() const
{
    const int index = m_profileCombo->currentIndex();
    return index >= 0 && index < m_profiles.size()
        ? &m_profiles.at(index) : nullptr;
}
