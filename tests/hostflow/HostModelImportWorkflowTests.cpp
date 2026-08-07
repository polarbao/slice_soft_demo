#include "apps/slicer_ui_host_sim/HostModelImportWorkflow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

namespace
{
QString ArgumentValue(const QStringList& arguments, const QString& name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments.at(index + 1)
        : QString{};
}

bool Check(
    const bool condition,
    const QString& message,
    QTextStream& errors)
{
    if (!condition)
    {
        errors << message << Qt::endl;
    }
    return condition;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    const QString modulePath = ArgumentValue(
        arguments, QStringLiteral("--module"));
    const QString repositoryRoot = ArgumentValue(
        arguments, QStringLiteral("--repo-root"));
    QTextStream errors(stderr);
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
    const QString objPath = QDir(repositoryRoot).filePath(QStringLiteral(
        "samples/models/openvdb/surface_shell_cube_no_uv.obj"));
    hostmodelimportresult objResult;
    if (!workflow.ImportModel(objPath, &objResult, &error))
    {
        errors << "OBJ 导入流程失败：" << error << Qt::endl;
        return 4;
    }
    if (!Check(!objResult.modelid.isEmpty(),
               QStringLiteral("OBJ 未返回 modelId。"),
               errors)
        || !Check(!objResult.instanceid.isEmpty(),
                  QStringLiteral("OBJ 未返回 instanceId。"),
                  errors)
        || !Check(!objResult.admission.isEmpty(),
                  QStringLiteral("OBJ 未返回预检结论。"),
                  errors)
        || !Check(workflow.SceneHandle() > 0U,
                  QStringLiteral("OBJ 未创建场景会话。"),
                  errors)
        || !Check(workflow.SceneRevision() == 1U,
                  QStringLiteral("OBJ 导入后 revision 应为 1。"),
                  errors))
    {
        return 5;
    }

    const QString threeMfPath = QDir(repositoryRoot).filePath(QStringLiteral(
        "samples/models/3mf/single_rgb_cube_stored.3mf"));
    hostmodelimportresult threeMfResult;
    if (!workflow.ImportModel(threeMfPath, &threeMfResult, &error))
    {
        errors << "3MF 导入流程失败：" << error << Qt::endl;
        return 6;
    }
    if (!Check(!threeMfResult.modelid.isEmpty(),
               QStringLiteral("3MF 未返回 modelId。"),
               errors)
        || !Check(threeMfResult.trianglecount > 0U,
                  QStringLiteral("3MF 三角形统计无效。"),
                  errors)
        || !Check(workflow.SceneRevision() == 2U,
                  QStringLiteral("3MF 导入后 revision 应为 2。"),
                  errors))
    {
        return 7;
    }

    hostmodelimportresult missingResult;
    error.clear();
    const bool missingAccepted = workflow.ImportModel(
        QDir(repositoryRoot).filePath(QStringLiteral("missing.obj")),
        &missingResult,
        &error);
    if (!Check(!missingAccepted && !error.isEmpty(),
               QStringLiteral("缺失模型必须 fail-closed 并返回明确原因。"),
               errors)
        || !Check(workflow.SceneRevision() == 2U,
                  QStringLiteral("导入负例不得改变场景 revision。"),
                  errors))
    {
        return 8;
    }

    QTextStream(stdout)
        << "HOSTFLOW_HB01_PASS sceneHandle=" << workflow.SceneHandle()
        << " revision=" << workflow.SceneRevision()
        << " calls=" << client.CallCount()
        << Qt::endl;
    return 0;
}
