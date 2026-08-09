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

    const quint64 revisionBeforeBatch = workflow.SceneRevision();
    QList<hostmodelimportresult> batchResults;
    error.clear();
    if (!workflow.ImportModels(
            QStringList{objPath, threeMfPath}, &batchResults, &error))
    {
        errors << "批量导入流程失败：" << error << Qt::endl;
        return 9;
    }
    if (!Check(batchResults.size() == 2,
               QStringLiteral("批量导入应返回两个有序结果。"),
               errors)
        || !Check(batchResults.at(0).sourcepath.endsWith(
                      QStringLiteral("surface_shell_cube_no_uv.obj")),
                  QStringLiteral("批量导入结果必须保持用户选择顺序。"),
                  errors)
        || !Check(batchResults.at(0).instanceid
                      != batchResults.at(1).instanceid,
                  QStringLiteral("批量导入实例身份必须唯一。"),
                  errors)
        || !Check(workflow.SceneRevision() == revisionBeforeBatch + 1U,
                  QStringLiteral("批量导入只能推进一次场景 revision。"),
                  errors)
        || !Check(workflow.InstanceCount() == 4,
                  QStringLiteral("批量导入后场景实例数应增加两个。"),
                  errors))
    {
        return 10;
    }

    const quint64 revisionBeforeRejectedBatch = workflow.SceneRevision();
    const int instancesBeforeRejectedBatch = workflow.InstanceCount();
    batchResults.clear();
    error.clear();
    const bool rejectedBatchAccepted = workflow.ImportModels(
        QStringList{
            objPath,
            QDir(repositoryRoot).filePath(QStringLiteral("missing.obj"))},
        &batchResults,
        &error);
    if (!Check(!rejectedBatchAccepted && !error.isEmpty(),
               QStringLiteral("含无效路径的批次必须整体 fail-closed。"),
               errors)
        || !Check(batchResults.isEmpty(),
                  QStringLiteral("失败批次不得返回半成品结果。"),
                  errors)
        || !Check(workflow.SceneRevision() == revisionBeforeRejectedBatch,
                  QStringLiteral("失败批次不得改变场景 revision。"),
                  errors)
        || !Check(workflow.InstanceCount() == instancesBeforeRejectedBatch,
                  QStringLiteral("失败批次不得留下半个场景。"),
                  errors))
    {
        return 11;
    }

    const QString texturedPath = QDir(repositoryRoot).filePath(QStringLiteral(
        "samples/models/openvdb_candidate/closed_textured_obj.obj"));
    hostmodelimportresult texturedResult;
    error.clear();
    if (!workflow.ImportModel(texturedPath, &texturedResult, &error))
    {
        errors << "带纹理 OBJ 导入流程失败：" << error << Qt::endl;
        return 12;
    }
    const QStringList texturePaths = workflow.TexturePaths();
    if (!Check(texturedResult.texturepaths.size() == 1,
               QStringLiteral("导入响应应保留唯一源贴图路径。"),
               errors)
        || !Check(QFileInfo(texturedResult.texturepaths.constFirst()).isFile(),
                  QStringLiteral("导入响应返回的源贴图必须存在。"),
                  errors)
        || !Check(texturePaths.contains(
                      texturedResult.texturepaths.constFirst()),
                  QStringLiteral("场景纹理集合应包含带纹理实例的源贴图。"),
                  errors))
    {
        return 13;
    }

    QTextStream(stdout)
        << "HOSTFLOW_HE02_PASS sceneHandle=" << workflow.SceneHandle()
        << " revision=" << workflow.SceneRevision()
        << " calls=" << client.CallCount()
        << Qt::endl;
    return 0;
}
