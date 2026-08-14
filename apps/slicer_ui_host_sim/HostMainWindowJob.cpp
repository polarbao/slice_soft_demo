#include "HostMainWindow.h"

#include <QLabel>
#include <QTabWidget>

void HostMainWindow::RefreshSliceJobReadiness()
{
    if (m_resultLoadActive)
    {
        m_sliceJobPanel->SetReady(
            false, QStringLiteral("正在校验并加载本次切片结果。"));
        return;
    }
    if (m_sliceJobController->IsActive())
    {
        m_sliceJobPanel->SetReady(false, QStringLiteral("切片作业正在运行。"));
        return;
    }
    if (!m_client.IsOpen())
    {
        m_sliceJobPanel->SetReady(false, QStringLiteral("切片模块尚未加载。"));
        return;
    }
    if (m_importWorkflow->SceneHandle() == 0U
        || m_importWorkflow->InstanceCount() == 0)
    {
        m_sliceJobPanel->SetReady(false, QStringLiteral("请先导入模型。"));
        return;
    }
    if (!m_sliceSettingsPanel->IsReady())
    {
        m_sliceJobPanel->SetReady(
            false,
            QStringLiteral("请先修正切片设置和有效 Profile。"));
        return;
    }
    m_sliceJobPanel->SetReady(true, QStringLiteral("切片作业已就绪。"));
}

void HostMainWindow::OnStartSlice()
{
    QString error;
    if (!m_sliceSettingsPanel->IsReady()
        || !m_sliceJobController->Start(
            m_importWorkflow->SceneHandle(),
            m_sliceSettingsPanel->EffectiveProfile(),
            &error))
    {
        const QString detail = error.isEmpty()
            ? QStringLiteral("场景或有效 Profile 尚未准备完成。")
            : error;
        m_statusLabel->setText(QStringLiteral("切片提交失败：%1").arg(detail));
        m_sliceJobPanel->ShowCompletion(
            false,
            false,
            QStringLiteral("HOST-SLICE-SUBMIT-FAILED"),
            detail,
            QString{},
            QString{},
            QJsonObject{},
            0,
            -1);
        m_inspectorTabs->setCurrentWidget(m_sliceJobPanel);
        RefreshSliceJobReadiness();
        return;
    }
    SetWorkflowEditingEnabled(false);
    m_sliceJobPanel->SetActive();
    m_inspectorTabs->setCurrentWidget(m_sliceJobPanel);
    m_statusLabel->setText(
        m_textureWhiteWarning
            ? QStringLiteral(
                "切片作业已提交；纹理白区预检存在非阻断保守告警。")
            : QStringLiteral("切片作业已提交到 Worker。"));
}

void HostMainWindow::OnCancelSlice()
{
    QString error;
    if (!m_sliceJobController->Cancel(&error))
    {
        m_statusLabel->setText(QStringLiteral("取消失败：%1").arg(error));
        return;
    }
    m_statusLabel->setText(QStringLiteral("正在取消并清理临时产物…"));
}

void HostMainWindow::OnSliceJobProgress(
    const QString& state,
    const QString& phase,
    const int current,
    const int total,
    const int percent,
    const qint64 elapsedMs)
{
    m_sliceJobPanel->UpdateProgress(
        state, phase, current, total, percent, elapsedMs);
}

void HostMainWindow::OnSliceJobCompleted(
    const bool success,
    const bool cancelled,
    const QString& code,
    const QString& message,
    const QString& detail,
    const QString& packageDirectory,
    const QJsonObject& timing,
    const qint64 elapsedMs,
    const qint64 cancelLatencyMs)
{
    m_sliceJobPanel->ShowCompletion(
        success,
        cancelled,
        code,
        message,
        detail,
        packageDirectory,
        timing,
        elapsedMs,
        cancelLatencyMs);
    if (success)
    {
        m_resultLoadActive = true;
        SetWorkflowEditingEnabled(false);
        const hostslicesettings settings = m_sliceSettingsPanel->Settings();
        m_packageReviewPanel->SetStage16Context(
            HostEffectiveProfileBuilder::GeometrySamplingStrategyId(
                settings.geometrysamplingstrategy),
            timing);
        m_statusLabel->setText(
            QStringLiteral("切片完成 · %1 ms · 正在后台校验并加载结果 · %2")
                .arg(elapsedMs)
                .arg(packageDirectory));
        LoadSliceResult(packageDirectory);
    }
    else if (cancelled)
    {
        SetWorkflowEditingEnabled(m_client.IsOpen());
        m_inspectorTabs->setCurrentWidget(m_sliceJobPanel);
        m_statusLabel->setText(
            QStringLiteral("切片已取消 · 清理耗时 %1 ms")
                .arg(cancelLatencyMs));
    }
    else
    {
        SetWorkflowEditingEnabled(m_client.IsOpen());
        m_inspectorTabs->setCurrentWidget(m_sliceJobPanel);
        m_statusLabel->setText(
            QStringLiteral("切片失败 · %1 · %2").arg(code, message));
    }
    RefreshSliceJobReadiness();
}
