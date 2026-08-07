#include "apps/slicer_ui_host_sim/HostModelListPanel.h"
#include "apps/slicer_ui_host_sim/ViewWorkspaceWidget.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QStringList>
#include <QTextStream>
#include <QToolButton>

namespace
{
bool Check(const bool condition, const QString& message, QTextStream& errors)
{
    if (!condition)
    {
        errors << message << Qt::endl;
    }
    return condition;
}

QString ArgumentValue(const QStringList& arguments, const QString& name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments.at(index + 1)
        : QString{};
}
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QTextStream errors(stderr);
    const QString modulePath = ArgumentValue(
        application.arguments(), QStringLiteral("--module"));
    const QString repositoryRoot = ArgumentValue(
        application.arguments(), QStringLiteral("--repo-root"));
    if (!Check(QFileInfo(modulePath).isFile(),
               QStringLiteral("slicer_module.dll 不存在。"),
               errors)
        || !Check(QDir(repositoryRoot).exists(),
                  QStringLiteral("仓库根目录不存在。"),
                  errors))
    {
        return 2;
    }

    ModuleClient client;
    QString error;
    if (!client.Open(modulePath, QByteArrayLiteral("{}"), &error))
    {
        errors << "模块加载失败：" << error << Qt::endl;
        return 3;
    }
    HostModelImportWorkflow workflow(client);
    hostmodelimportresult objResult;
    hostmodelimportresult threeMfResult;
    if (!workflow.ImportModel(
            QDir(repositoryRoot).filePath(QStringLiteral(
                "samples/models/openvdb/surface_shell_cube_no_uv.obj")),
            &objResult,
            &error)
        || !workflow.ImportModel(
            QDir(repositoryRoot).filePath(QStringLiteral(
                "samples/models/3mf/single_rgb_cube_stored.3mf")),
            &threeMfResult,
            &error))
    {
        errors << "H-B-02 模型准备失败：" << error << Qt::endl;
        return 4;
    }

    HostModelListPanel panel;
    ViewWorkspaceWidget workspace;
    QObject::connect(
        &panel,
        &HostModelListPanel::SigSelectionChanged,
        &workspace,
        &ViewWorkspaceWidget::SetSelectedInstances);
    panel.SetCommandsEnabled(true);
    panel.AddModel(objResult);
    panel.AddModel(threeMfResult);

    auto* selectAll = panel.findChild<QToolButton*>(
        QStringLiteral("hostModelListSelectAllButton"));
    auto* remove = panel.findChild<QToolButton*>(
        QStringLiteral("hostModelListRemoveButton"));
    auto* list = panel.findChild<QListWidget*>(
        QStringLiteral("hostImportedModelList"));
    auto* selectionLabel = workspace.findChild<QLabel*>(
        QStringLiteral("viewSelectionLabel"));
    if (!Check(selectAll != nullptr && remove != nullptr && list != nullptr,
               QStringLiteral("模型列表控件不完整。"),
               errors)
        || !Check(selectionLabel != nullptr,
                  QStringLiteral("工作区选择联动控件缺失。"),
                  errors))
    {
        return 5;
    }

    selectAll->click();
    if (!Check(panel.SelectedInstanceIds().size() == 2,
               QStringLiteral("全选未覆盖两个模型实例。"),
               errors)
        || !Check(selectionLabel->text().contains(QStringLiteral("2 个模型")),
                  QStringLiteral("多选结果未同步到中央工作区。"),
                  errors))
    {
        return 6;
    }

    QStringList removeRequest;
    QObject::connect(
        &panel,
        &HostModelListPanel::SigRemoveRequested,
        [&removeRequest](const QStringList& instanceIds)
        {
            removeRequest = instanceIds;
        });
    remove->click();
    if (!Check(removeRequest.size() == 2,
               QStringLiteral("删除命令未携带全部选中实例。"),
               errors))
    {
        return 7;
    }
    if (!workflow.RemoveInstances(removeRequest, &error)
        || !Check(workflow.SceneRevision() == 3U,
                  QStringLiteral("批量删除只应提交一次 revision。"),
                  errors)
        || !Check(workflow.InstanceCount() == 0,
                  QStringLiteral("模块实例删除后宿主跟踪未清空。"),
                  errors))
    {
        errors << "H-B-02 批量删除失败：" << error << Qt::endl;
        return 8;
    }
    panel.RemoveInstances(removeRequest);
    if (!Check(panel.ModelCount() == 0,
               QStringLiteral("删除提交后本地列表未同步。"),
               errors)
        || !Check(selectionLabel->text() == QStringLiteral("未选择模型实例。"),
                  QStringLiteral("空选择未同步到中央工作区。"),
                  errors))
    {
        return 9;
    }

    error.clear();
    if (!Check(!workflow.RemoveInstances(removeRequest, &error),
               QStringLiteral("重复删除必须 fail-closed。"),
               errors)
        || !Check(workflow.SceneRevision() == 3U,
                  QStringLiteral("删除负例不得改变 scene revision。"),
                  errors))
    {
        return 10;
    }

    QTextStream(stdout) << "HOSTFLOW_HB02_PASS" << Qt::endl;
    return 0;
}
