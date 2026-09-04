#include "apps/slicer_ui_host_sim/HostModelImportWorkflow.h"
#include "apps/slicer_ui_host_sim/HostImportDirectoryPolicy.h"
#include "apps/slicer_ui_host_sim/HostImportPlacementPolicy.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>
#include <QTemporaryDir>

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

bool WriteTextFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(content) == content.size();
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

    QTemporaryDir importRoot;
    const QDir importBase(importRoot.path());
    const QString applicationDirectory = importBase.filePath(
        QStringLiteral("application"));
    const QString workingDirectory = importBase.filePath(
        QStringLiteral("working"));
    const QString previousDirectory = importBase.filePath(
        QStringLiteral("previous"));
    const QString buildApplicationDirectory = importBase.filePath(
        QStringLiteral("build-application"));
    QDir().mkpath(QDir(applicationDirectory).filePath(
        QStringLiteral("model")));
    QDir().mkpath(QDir(workingDirectory).filePath(
        QStringLiteral("model")));
    QDir().mkpath(previousDirectory);
    QDir().mkpath(buildApplicationDirectory);
    if (!Check(
            importRoot.isValid()
                && HostImportDirectoryPolicy::Resolve(
                       applicationDirectory,
                       workingDirectory)
                    == QDir(applicationDirectory).absoluteFilePath(
                        QStringLiteral("model")),
            QStringLiteral("运行目录存在 model 时应优先从该目录导入。"),
            errors)
        || !Check(
            HostImportDirectoryPolicy::Resolve(
                applicationDirectory,
                workingDirectory,
                previousDirectory)
                == QDir(previousDirectory).absolutePath(),
            QStringLiteral("本次会话已使用的模型目录应保持优先。"),
            errors)
        || !Check(
            HostImportDirectoryPolicy::Resolve(
                buildApplicationDirectory,
                workingDirectory)
                == QDir(workingDirectory).absoluteFilePath(
                    QStringLiteral("model")),
            QStringLiteral(
                "构建目录无 model 时应回退到工作目录下的 model。"),
            errors))
    {
        return 14;
    }
    if (!Check(
            !HostImportPlacementPolicy::RequiresGridLayout(0),
            QStringLiteral("空场景不得请求自动排版。"),
            errors)
        || !Check(
            HostImportPlacementPolicy::RequiresGridLayout(1),
            QStringLiteral(
                "首个模型导入后必须请求规则排版以进入边界位置。"),
            errors)
        || !Check(
            HostImportPlacementPolicy::RequiresGridLayout(22),
            QStringLiteral("多模型导入后仍必须请求规则排版。"),
            errors))
    {
        return 22;
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

    hostsceneeditresult initialLayoutResult;
    error.clear();
    if (!workflow.ApplyGridLayout(
            hostgridlayoutrequest{}, &initialLayoutResult, &error)
        || !Check(
            workflow.SceneRevision() == 2U,
            QStringLiteral("首个模型的边界排版应推进一次 revision。"),
            errors)
        || !Check(
            initialLayoutResult.collisioncount == 0
                && initialLayoutResult.outofboundscount == 0,
            QStringLiteral(
                "首个模型应被放入规则排版边界且不得碰撞或越界。"),
            errors))
    {
        errors << "首个模型边界排版失败：" << error << Qt::endl;
        return 23;
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
        || !Check(workflow.SceneRevision() == 3U,
                  QStringLiteral("3MF 导入后 revision 应为 3。"),
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
        || !Check(workflow.SceneRevision() == 3U,
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

    const QString degradedObjPath = importBase.filePath(
        QStringLiteral("missing-material.obj"));
    if (!Check(
            WriteTextFile(
                degradedObjPath,
                QByteArrayLiteral(
                    "v 0 0 0\n"
                    "v 10 0 0\n"
                    "v 0 10 0\n"
                    "vt 0 0\n"
                    "vt 1 0\n"
                    "vt 0 1\n"
                    "usemtl mtl0\n"
                    "f 1/1 2/2 3/3\n")),
            QStringLiteral("无法创建缺失 MTL 的 OBJ 夹具。"),
            errors))
    {
        return 15;
    }
    hostmodelimportresult degradedResult;
    error.clear();
    if (!workflow.ImportModel(
            degradedObjPath, &degradedResult, &error))
    {
        errors << "缺失 MTL 的 OBJ 应降级导入：" << error << Qt::endl;
        return 16;
    }
    if (!Check(
            degradedResult.singlematerialonly
                && degradedResult.appearancestatus
                    == QStringLiteral(
                        "degraded_missing_material_definition")
                && !degradedResult.appearancedetail.isEmpty()
                && workflow.RequiresSingleMaterialProcess(),
            QStringLiteral(
                "缺失 MTL 的 OBJ 未声明半透明灰色/单材料限制。"),
            errors))
    {
        return 17;
    }

    const QString missingTextureMtlPath = importBase.filePath(
        QStringLiteral("missing-texture.mtl"));
    const QString missingTextureObjPath = importBase.filePath(
        QStringLiteral("missing-texture.obj"));
    if (!Check(
            WriteTextFile(
                missingTextureMtlPath,
                QByteArrayLiteral(
                    "newmtl mtl0\n"
                    "Kd 1 1 1\n"
                    "map_Kd absent-texture.png\n"))
                && WriteTextFile(
                    missingTextureObjPath,
                    QByteArrayLiteral(
                        "mtllib missing-texture.mtl\n"
                        "v 0 0 0\n"
                        "v 10 0 0\n"
                        "v 0 10 0\n"
                        "vt 0 0\n"
                        "vt 1 0\n"
                        "vt 0 1\n"
                        "usemtl mtl0\n"
                        "f 1/1 2/2 3/3\n")),
            QStringLiteral("无法创建缺失纹理的 OBJ/MTL 夹具。"),
            errors))
    {
        return 18;
    }
    hostmodelimportresult missingTextureResult;
    error.clear();
    if (!workflow.ImportModel(
            missingTextureObjPath, &missingTextureResult, &error))
    {
        errors << "缺失贴图的 OBJ 应降级导入：" << error << Qt::endl;
        return 19;
    }
    if (!Check(
            missingTextureResult.singlematerialonly
                && missingTextureResult.appearancestatus
                    == QStringLiteral("degraded_missing_texture"),
            QStringLiteral("缺失贴图的 OBJ 未声明单材料限制。"),
            errors))
    {
        return 20;
    }

    error.clear();
    if (!Check(
            workflow.RemoveInstances(
                QStringList{
                    degradedResult.instanceid,
                    missingTextureResult.instanceid},
                &error)
                && !workflow.RequiresSingleMaterialProcess()
                && workflow.SingleMaterialRestrictionSummary().isEmpty(),
            QStringLiteral("移除降级模型后单材料限制未解除：%1")
                .arg(error),
            errors))
    {
        return 21;
    }

    QTextStream(stdout)
        << "HOSTFLOW_HE02_PASS sceneHandle=" << workflow.SceneHandle()
        << " revision=" << workflow.SceneRevision()
        << " calls=" << client.CallCount()
        << Qt::endl;
    return 0;
}
