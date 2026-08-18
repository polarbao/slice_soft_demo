#include "HostMainWindow.h"

#include <QLabel>
#include <QMessageBox>

void HostMainWindow::OnTransformRequested(
    const QStringList& instanceIds,
    const double deltaXMm,
    const double deltaYMm,
    const double deltaZMm,
    const double rotateXDegrees,
    const double rotateYDegrees,
    const double rotateZDegrees,
    const double uniformScaleFactor,
    const bool mirrorX,
    const bool mirrorY,
    const bool landOnBuildPlate)
{
    SetSceneCommandsEnabled(false);
    hostsceneeditresult result;
    QString error;
    const bool committed = m_importWorkflow->ApplyTransforms(
        instanceIds,
        hosttransformrequest{
            deltaXMm,
            deltaYMm,
            deltaZMm,
            rotateXDegrees,
            rotateYDegrees,
            rotateZDegrees,
            uniformScaleFactor,
            mirrorX,
            mirrorY,
            landOnBuildPlate},
        &result,
        &error);
    SetSceneCommandsEnabled(m_client.IsOpen());
    if (!committed)
    {
        ShowSceneEditError(QStringLiteral("实例变换"), error);
        return;
    }
    m_transformLayoutPanel->ResetTransformInputs();
    ShowSceneEditResult(QStringLiteral("实例变换"), result);
    RefreshSceneViews();
}

void HostMainWindow::OnLandOnBuildPlateRequested(
    const QStringList& instanceIds)
{
    SetSceneCommandsEnabled(false);
    hostsceneeditresult result;
    QString error;
    const bool committed = m_importWorkflow->LandOnBuildPlate(
        instanceIds, &result, &error);
    SetSceneCommandsEnabled(m_client.IsOpen());
    if (!committed)
    {
        ShowSceneEditError(QStringLiteral("实例触底"), error);
        return;
    }
    ShowSceneEditResult(QStringLiteral("实例触底"), result);
    RefreshSceneViews();
}

void HostMainWindow::OnLayoutRequested(
    const int maxColumns,
    const int maxRows,
    const double columnGapMm,
    const double rowGapMm)
{
    SetSceneCommandsEnabled(false);
    hostsceneeditresult result;
    QString error;
    const bool committed = m_importWorkflow->ApplyGridLayout(
        hostgridlayoutrequest{
            maxColumns,
            maxRows,
            columnGapMm,
            rowGapMm},
        &result,
        &error);
    SetSceneCommandsEnabled(m_client.IsOpen());
    if (!committed)
    {
        ShowSceneEditError(QStringLiteral("规则排版"), error);
        return;
    }
    ShowSceneEditResult(QStringLiteral("规则排版"), result);
    RefreshSceneViews();
}

void HostMainWindow::SetSceneCommandsEnabled(const bool enabled)
{
    m_modelListPanel->SetCommandsEnabled(enabled);
    m_transformLayoutPanel->SetCommandsEnabled(enabled);
}

void HostMainWindow::SetWorkflowEditingEnabled(const bool enabled)
{
    SetSceneCommandsEnabled(enabled);
    m_profilePanel->setEnabled(enabled);
    m_sliceSettingsPanel->setEnabled(enabled);
}

void HostMainWindow::ShowSceneEditResult(
    const QString& action,
    const hostsceneeditresult& result)
{
    m_transformLayoutPanel->SetSceneState(
        m_importWorkflow->InstanceCount(), result.scenerevision);
    m_statusLabel->setText(
        QStringLiteral(
            "%1已提交 · revision=%2 · 碰撞=%3 · 越界=%4 · ABI 调用 %5 次")
            .arg(action)
            .arg(result.scenerevision)
            .arg(result.collisioncount)
            .arg(result.outofboundscount)
            .arg(m_client.CallCount()));
}

void HostMainWindow::ShowSceneEditError(
    const QString& action,
    const QString& error)
{
    const QString detail = error.isEmpty()
        ? QStringLiteral("场景编辑失败，模块未返回详细原因。")
        : error;
    m_statusLabel->setText(QStringLiteral("%1失败").arg(action));
    QMessageBox::critical(
        this,
        QStringLiteral("%1失败").arg(action),
        detail);
}
