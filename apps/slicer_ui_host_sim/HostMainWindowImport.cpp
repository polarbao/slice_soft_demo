#include "HostMainWindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QTableWidget>
#include <QTableWidgetItem>

void HostMainWindow::OnImportModel()
{
    const QStringList modelPaths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择要批量导入的 OBJ、3MF 或 STL 模型"),
        QDir::homePath(),
        QStringLiteral(
            "支持的模型 (*.obj *.3mf *.stl);;OBJ 模型 (*.obj);;"
            "3MF 模型 (*.3mf);;STL 模型 (*.stl)"));
    if (modelPaths.isEmpty())
    {
        return;
    }

    QString contextError;
    if (!ApplyPendingSceneContext(&contextError))
    {
        ShowImportError(contextError);
        return;
    }

    SetSceneCommandsEnabled(false);
    m_importSummaryLabel->setText(
        QStringLiteral("正在导入 %1 个模型并执行快速预检…")
            .arg(modelPaths.size()));
    QCoreApplication::processEvents();

    QList<hostmodelimportresult> results;
    QString error;
    const bool imported = m_importWorkflow->ImportModels(
        modelPaths, &results, &error);
    SetSceneCommandsEnabled(m_client.IsOpen());
    if (!imported)
    {
        ShowImportError(error);
        return;
    }

    RefreshSliceSettings();
    for (const hostmodelimportresult& result : results)
    {
        ShowImportResult(result);
    }
    m_importSummaryLabel->setText(
        QStringLiteral(
            "已按选择顺序导入 %1 个模型；一次原子提交完成，场景 revision=%2。"
            "预检表显示最后一个模型。")
            .arg(results.size())
            .arg(m_importWorkflow->SceneRevision()));
    m_statusLabel->setText(
        QStringLiteral("批量模型已导入 · ABI 调用 %1 次")
            .arg(m_client.CallCount()));
    RefreshSceneViews();
}

void HostMainWindow::ShowImportResult(const hostmodelimportresult& result)
{
    const QFileInfo source(result.sourcepath);
    const QString admissionText =
        result.admission == QStringLiteral("passed")
        ? QStringLiteral("通过")
        : result.admission == QStringLiteral("manual_repair_required")
            ? QStringLiteral("需要人工修复")
            : QStringLiteral("阻断");
    m_modelListPanel->AddModel(result);
    m_transformLayoutPanel->SetSceneState(
        m_importWorkflow->InstanceCount(),
        m_importWorkflow->SceneRevision());
    m_importSummaryLabel->setText(
        QStringLiteral(
            "%1\nOBJ/3MF/STL 元数据：%2 三角形，%3 顶点，"
            "%4 × %5 × %6 mm，UV=%7，法线=%8\n"
            "快速预检：%9；场景 revision=%10")
            .arg(source.fileName())
            .arg(result.trianglecount)
            .arg(result.vertexcount)
            .arg(result.widthmm, 0, 'f', 2)
            .arg(result.heightmm, 0, 'f', 2)
            .arg(result.depthmm, 0, 'f', 2)
            .arg(result.hasuv ? QStringLiteral("有") : QStringLiteral("无"))
            .arg(result.hasnormals ? QStringLiteral("有") : QStringLiteral("无"))
            .arg(admissionText)
            .arg(m_importWorkflow->SceneRevision()));

    m_preflightTable->setRowCount(result.issues.size());
    for (int rowIndex = 0; rowIndex < result.issues.size(); ++rowIndex)
    {
        const hostpreflightissue& issue = result.issues.at(rowIndex);
        m_preflightTable->setItem(
            rowIndex, 0, new QTableWidgetItem(issue.severity));
        m_preflightTable->setItem(
            rowIndex,
            1,
            new QTableWidgetItem(QStringLiteral("%1 (%2)")
                .arg(issue.code)
                .arg(issue.count)));
        m_preflightTable->setItem(
            rowIndex, 2, new QTableWidgetItem(issue.detail));
    }
    m_preflightTable->resizeRowsToContents();
    m_statusLabel->setText(
        QStringLiteral("模型已导入 · %1 · ABI 调用 %2 次")
            .arg(admissionText)
            .arg(m_client.CallCount()));
}

void HostMainWindow::ShowImportError(const QString& error)
{
    const QString detail = error.isEmpty()
        ? QStringLiteral("模型导入流程失败，模块未返回详细原因。")
        : error;
    m_importSummaryLabel->setText(QStringLiteral("导入失败：%1").arg(detail));
    m_statusLabel->setText(QStringLiteral("模型导入失败"));
    QMessageBox::critical(this, QStringLiteral("模型导入失败"), detail);
}
