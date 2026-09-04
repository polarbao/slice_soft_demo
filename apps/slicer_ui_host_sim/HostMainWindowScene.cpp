#include "HostMainWindow.h"

#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

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

// 「新建场景」出口。场景在首次提交时钉死 Profile 与 buildVolume，此后切换工艺会被
// 「当前场景已绑定 Profile/buildVolume；请新建场景后修改」挡下——而该提示要求的操作
// 此前在 UI 上并不存在，且删光模型也不解绑（RemoveInstances 不清 sceneHandle）。
// 没有这个按钮，用户一旦先导入再改工艺就无路可退，只能重启软件。
void HostMainWindow::AttachSceneResetButton(QWidget* page, QVBoxLayout* layout)
{
    auto* resetSceneButton = new QPushButton(
        QStringLiteral("新建场景（解绑工艺后可重选）"), page);
    resetSceneButton->setObjectName(QStringLiteral("hostResetSceneButton"));
    resetSceneButton->setToolTip(QStringLiteral(
        "释放已导入模型并解绑当前场景的 Profile 与设备构建体积，"
        "之后可在工艺配置中改选工艺再重新导入模型。"));
    connect(resetSceneButton, &QPushButton::clicked,
            this, &HostMainWindow::OnResetScene);
    layout->addWidget(resetSceneButton);
}

void HostMainWindow::OnResetScene()
{
    SetSceneCommandsEnabled(false);
    m_importWorkflow->ResetScene();
    SetSceneCommandsEnabled(m_client.IsOpen());
    m_modelListPanel->Clear();
    m_preflightTable->setRowCount(0);
    m_transformLayoutPanel->SetSceneState(
        m_importWorkflow->InstanceCount(),
        m_importWorkflow->SceneRevision());
    m_importSummaryLabel->setText(
        QStringLiteral("场景已解绑；请在工艺配置中选定工艺后重新导入模型。"));
    m_statusLabel->setText(
        QStringLiteral("场景已解绑 · ABI 调用 %1 次").arg(m_client.CallCount()));
    // 先刷新切片设置再刷视图：前者会重算 Profile 可提交性，
    // 「配置不可提交」的提示正是由它产出。
    RefreshSliceSettings();
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
