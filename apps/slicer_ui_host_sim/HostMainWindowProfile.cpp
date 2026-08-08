#include "HostMainWindow.h"

#include <QLabel>
#include <QMessageBox>

void HostMainWindow::ConfigureProfiles()
{
    hostprofilecatalogresolution resolution;
    QString error;
    if (!HostProfileCapabilityResolver::Resolve(
            *m_profileCatalog,
            m_client.ModuleInfo(),
            &resolution,
            &error))
    {
        m_selectedProfileId.clear();
        m_profilePanel->ClearProfiles(error);
        m_statusLabel->setText(QStringLiteral("Profile 目录不可用"));
        QMessageBox::critical(
            this, QStringLiteral("Profile 目录错误"), error);
        return;
    }
    m_profilePanel->SetProfiles(resolution);
    m_selectedProfileId = m_profilePanel->SelectedProfileId();
}

void HostMainWindow::OnProfileChanged(const QString& profileId)
{
    m_selectedProfileId = profileId;
    m_statusLabel->setText(
        QStringLiteral("Profile 已选择：%1 · 仅更新宿主会话草稿")
            .arg(profileId));
}
