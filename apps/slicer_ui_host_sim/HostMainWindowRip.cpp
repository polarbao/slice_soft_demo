#include "HostMainWindow.h"

#include "HostWorkspaceState.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QSettings>
#include <QTabWidget>
#include <QUrl>

void HostMainWindow::OnRipSettingsChanged()
{
    QString error;
    const hostripsettings settings = m_ripSettingsPanel->Settings();
    if (!HostRipSettingsStore::Validate(settings, &error))
    {
        m_ripSettingsPanel->ShowJobState(
            QStringLiteral("配置无效"), error);
        return;
    }
    RefreshRipRequestStatus();
    if (!HostWorkspaceState::PersistenceEnabled())
    {
        return;
    }
    QSettings storage(
        HostWorkspaceState::OrganizationName(),
        HostWorkspaceState::ApplicationName());
    if (!HostRipSettingsStore::Save(storage, settings, &error))
    {
        m_ripSettingsPanel->ShowJobState(
            QStringLiteral("保存失败"), error);
    }
}

void HostMainWindow::OnRunRip()
{
    (void)StartRipForPackage(
        m_ripSettingsPanel->PackageDirectory(), false);
}

bool HostMainWindow::StartRipForPackage(
    const QString& packageDirectory,
    const bool automatic)
{
    QString error;
    if (packageDirectory.isEmpty()
        || packageDirectory
            != m_packageReviewController->Review().packagedirectory
        || !m_ripJobController->Start(
            packageDirectory,
            m_ripModuleDirectory,
            m_ripSettingsPanel->Settings(),
            &error))
    {
        const QString detail = error.isEmpty()
            ? QStringLiteral("当前没有通过严格加载的切片包。")
            : error;
        m_ripSettingsPanel->ShowCompletion(
            false, false, detail, QString{});
        m_statusLabel->setText(
            QStringLiteral("切片成功 · RIP 启动失败：%1").arg(detail));
        if (automatic)
        {
            m_inspectorTabs->setCurrentWidget(m_ripSettingsPanel);
        }
        RefreshSliceJobReadiness();
        return false;
    }
    m_ripSettingsPanel->SetJobActive(true);
    m_ripSettingsPanel->ShowJobState(
        automatic ? QStringLiteral("自动运行") : QStringLiteral("手动运行"),
        QStringLiteral("RIP 已提交"));
    SetWorkflowEditingEnabled(false);
    m_inspectorTabs->setCurrentWidget(m_ripSettingsPanel);
    m_statusLabel->setText(
        QStringLiteral("切片成功 · RIP 正在运行 · %1")
            .arg(packageDirectory));
    RefreshSliceJobReadiness();
    return true;
}

void HostMainWindow::OnCancelRip()
{
    QString error;
    if (!m_ripJobController->Cancel(&error))
    {
        m_ripSettingsPanel->ShowJobState(
            QStringLiteral("取消失败"), error);
    }
}

void HostMainWindow::OnRipStateChanged(
    const QString& state,
    const QString& message)
{
    m_ripSettingsPanel->ShowJobState(state, message);
}

void HostMainWindow::OnRipCompleted(
    const bool success,
    const bool cancelled,
    const QString& code,
    const QString& message,
    const QString& outputDirectory,
    const qint64 elapsedMs)
{
    m_ripSettingsPanel->ShowCompletion(
        success,
        cancelled,
        QStringLiteral("%1 · %2 · %3 ms")
            .arg(code, message)
            .arg(elapsedMs),
        outputDirectory);
    SetWorkflowEditingEnabled(m_client.IsOpen());
    m_statusLabel->setText(
        success
            ? QStringLiteral("切片成功 · RIP 本地候选完成 · 外部验收延期")
            : cancelled
                ? QStringLiteral("切片成功 · RIP 已取消")
                : QStringLiteral("切片成功 · RIP 失败 · %1 · %2")
                      .arg(code, message));
    RefreshSliceJobReadiness();
}

void HostMainWindow::OnOpenRipOutputRequested(
    const QString& outputDirectory)
{
    const QString packageDirectory =
        m_packageReviewController->Review().packagedirectory;
    const QString expected = QDir(packageDirectory).filePath(
        QStringLiteral("rip"));
    if (packageDirectory.isEmpty()
        || QDir::cleanPath(outputDirectory) != QDir::cleanPath(expected)
        || !QFileInfo(outputDirectory).isDir()
        || !QDesktopServices::openUrl(QUrl::fromLocalFile(outputDirectory)))
    {
        m_ripSettingsPanel->ShowJobState(
            QStringLiteral("打开失败"),
            QStringLiteral("RIP 输出目录身份无效或系统拒绝打开。"));
    }
}

void HostMainWindow::RefreshRipRuntimeStatus()
{
    QString error;
    const bool valid = m_ripJobController->CheckRuntime(
        m_ripModuleDirectory, &error);
    m_ripSettingsPanel->SetRuntimeStatus(
        valid,
        valid
            ? QStringLiteral(
                "RIP 运行时完整性通过；仅限本地工程候选，外部验收延期。")
            : QStringLiteral("RIP 运行时不可用：%1").arg(error));
    RefreshRipRequestStatus();
}

void HostMainWindow::RefreshRipRequestStatus()
{
    const QString packageDirectory = m_ripSettingsPanel->PackageDirectory();
    if (packageDirectory.isEmpty())
    {
        m_ripSettingsPanel->SetRequestStatus(
            false, QStringLiteral("RIP 未运行 · 尚无已校验切片包"));
        return;
    }
    QString error;
    const bool valid = m_ripJobController->CheckRequest(
        packageDirectory,
        m_ripModuleDirectory,
        m_ripSettingsPanel->Settings(),
        &error);
    m_ripSettingsPanel->SetRequestStatus(
        valid,
        valid
            ? QStringLiteral("RIP 已就绪 · 输出将发布到同级 rip 目录")
            : QStringLiteral("RIP 前置检查未通过 · %1").arg(error));
}
