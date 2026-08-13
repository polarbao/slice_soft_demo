#include "HostMainWindow.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QLabel>
#include <QTabWidget>
#include <QUrl>

void HostMainWindow::LoadSliceResult(const QString& packageDirectory)
{
    QString error;
    if (!m_packageReviewController->Load(packageDirectory, &error))
    {
        m_packageReviewPanel->ShowError(error);
        m_statusLabel->setText(
            QStringLiteral("切片已完成，但结果查看失败：%1").arg(error));
        return;
    }
    m_packageReviewPanel->SetPackage(m_packageReviewController->Review());
    OnResultLayerRequested(0, m_packageReviewPanel->SelectedChannels());
    OnResultReportRequested(QStringLiteral("slice"));
    m_workspaceTabs->setCurrentWidget(m_packageReviewPanel);
}

void HostMainWindow::OnResultLayerRequested(
    const int layerIndex,
    const QStringList& channels)
{
    QString previewPath;
    QString error;
    if (!m_packageReviewController->RenderPreview(
            layerIndex, channels, &previewPath, &error))
    {
        m_packageReviewPanel->ShowError(error);
        return;
    }
    QString referencePreviewPath;
    if (layerIndex == 0)
    {
        referencePreviewPath = previewPath;
    }
    else if (!m_packageReviewController->RenderPreview(
                 0, channels, &referencePreviewPath, &error))
    {
        m_packageReviewPanel->ShowError(
            QStringLiteral("首层 A 对照预览失败：%1").arg(error));
        return;
    }
    const hostpackagereview& review = m_packageReviewController->Review();
    if (layerIndex < 0 || layerIndex >= review.layers.size())
    {
        m_packageReviewPanel->ShowError(
            QStringLiteral("结果层索引超出已校验范围。"));
        return;
    }
    m_packageReviewPanel->ShowPreview(
        previewPath, review.layers.at(layerIndex));
    m_packageReviewPanel->ShowReferencePreview(
        referencePreviewPath, review.layers.first());
}

void HostMainWindow::OnResultReportRequested(const QString& reportName)
{
    hostpackagereport report;
    QString error;
    if (!m_packageReviewController->ReadReport(reportName, &report, &error))
    {
        m_packageReviewPanel->ShowError(error);
        return;
    }
    m_packageReviewPanel->ShowReport(report);
}

void HostMainWindow::OnOpenPackageDirectoryRequested(
    const QString& packageDirectory)
{
    const QString verifiedDirectory =
        m_packageReviewController->Review().packagedirectory;
    if (packageDirectory.isEmpty() || packageDirectory != verifiedDirectory
        || !QFileInfo(packageDirectory).isDir())
    {
        m_packageReviewPanel->ShowError(
            QStringLiteral("生产包目录身份不一致或目录已不存在，已停止打开。"));
        m_statusLabel->setText(QStringLiteral("生产包目录打开失败"));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(packageDirectory)))
    {
        m_packageReviewPanel->ShowError(
            QStringLiteral("系统文件管理器拒绝打开生产包目录。"));
        m_statusLabel->setText(QStringLiteral("生产包目录打开失败"));
        return;
    }
    m_statusLabel->setText(QStringLiteral("已打开本次切片生产包目录"));
}
