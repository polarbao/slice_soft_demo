// 宿主场景工艺重绑回归。
//
// 导入并编辑后直接切换工艺必须保留模型资源与摆放；实际 Worker 切片由
// HostSliceJobTests 覆盖。ResetScene 仍作为显式清空模型/更换画幅的出口。

#include "apps/slicer_ui_host_sim/HostModelImportWorkflow.h"
#include "apps/slicer_ui_host_sim/HostSliceSettings.h"
#include "apps/slicer_ui_host_sim/ModuleClient.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

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
        ? arguments.at(index + 1) : QString{};
}

hostbuildvolume ReferenceBuildVolume()
{
    // 默认值即宿主参考设备体积；显式构造只为让用例不依赖默认值将来是否改动。
    hostbuildvolume volume;
    volume.widthmm = 230.0;
    volume.heightmm = 100.0;
    volume.zlimitmm = 60.0;
    return volume;
}
}  // namespace

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
               QStringLiteral("slicer_module.dll 不存在。"), errors)
        || !Check(QDir(repositoryRoot).exists(),
                  QStringLiteral("仓库根目录不存在。"), errors))
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
    const hostbuildvolume buildVolume = ReferenceBuildVolume();
    const QString legacyProfile = QStringLiteral("host-reference-default");
    const QString transferProfile =
        QStringLiteral("host-reference-transfer-channel");

    // 1. 空场景允许自由选定工艺。
    if (!Check(workflow.SetPendingSceneContext(
                   legacyProfile, buildVolume, &error),
               QStringLiteral("空场景应接受任意有效工艺：%1").arg(error),
               errors))
    {
        return 4;
    }

    // 2. 导入模型使场景成形，工艺随之绑定。
    const QString modelPath = QDir(repositoryRoot).filePath(
        QStringLiteral("samples/models/openvdb/surface_shell_cube_no_uv.obj"));
    hostmodelimportresult imported;
    error.clear();
    if (!workflow.ImportModel(modelPath, &imported, &error))
    {
        errors << "模型导入失败：" << error << Qt::endl;
        return 5;
    }
    if (!Check(workflow.SceneHandle() != 0U,
               QStringLiteral("导入后应产生场景句柄。"), errors)
        || !Check(workflow.SceneProfileId() == legacyProfile,
                  QStringLiteral("场景应绑定导入时生效的工艺。"), errors))
    {
        return 6;
    }

    // Switching the profile must preserve imported resources and edited placement.
    hosttransformrequest transform;
    transform.deltaxmm=12; transform.rotatezdegrees=90;
    hostsceneeditresult edit;
    if(!workflow.ApplyTransforms({imported.instanceid},transform,&edit,&error)) return 20;
    const auto snapshot=[&]()
    {
        QByteArray bytes;
        const QJsonObject request{
            {QStringLiteral("capability"),QStringLiteral("scene.get_snapshot")},
            {QStringLiteral("sceneHandle"),static_cast<qint64>(workflow.SceneHandle())}};
        if(!client.Execute(QJsonDocument(request).toJson(QJsonDocument::Compact),&bytes,&error)) return QJsonObject{};
        return QJsonDocument::fromJson(bytes).object().value(QStringLiteral("scene")).toObject();
    };
    const auto before=snapshot();
    const auto previousHandle=workflow.SceneHandle();
    error.clear();
    if (!Check(workflow.SetPendingSceneContext(
                   transferProfile, buildVolume, &error),
               QStringLiteral("导入后改选工艺应成功：%1").arg(error), errors))
    {
        return 7;
    }
    const auto after=snapshot();
    const auto oldInstance=before.value(QStringLiteral("instances")).toArray().first().toObject();
    const auto newInstance=after.value(QStringLiteral("instances")).toArray().first().toObject();
    for(const auto* field:{"instanceId","modelId","requestedTransform","derivedLayoutTransform","effectiveTransform","effectiveBboxMm","sourceTransformIdentity"})
        if(!Check(oldInstance.value(field)==newInstance.value(field),QStringLiteral("切换改变实例字段 %1").arg(field),errors)) return 21;
    if(!Check(workflow.SceneHandle()!=previousHandle && workflow.InstanceCount()==1
        && after.value("resolvedProfileId").toString()==transferProfile
        && newInstance.value("resolvedProfileId").toString()==transferProfile
        && before.value("models")==after.value("models")
        && before.value("resourceScopes")==after.value("resourceScopes"),QStringLiteral("重绑身份/资源错误"),errors)) return 22;
    const auto currentHandle=workflow.SceneHandle();
    auto invalidVolume=buildVolume;invalidVolume.widthmm+=1;
    if(!Check(!workflow.SetPendingSceneContext(legacyProfile,invalidVolume,&error)
        && workflow.SceneHandle()==currentHandle && workflow.SceneProfileId()==transferProfile,
        QStringLiteral("非法画幅变更必须保留当前工艺/场景"),errors)) return 23;

    // 4. 缺陷在于此前【没有出路】：ResetScene 之前不存在，
    //    且 RemoveInstances 只删实例、不清场景绑定，删光模型也退不出该状态。
    //    以下断言把这条出路钉死。
    workflow.ResetScene();
    if (!Check(workflow.SceneHandle() == 0U,
               QStringLiteral("解绑后场景句柄必须清零。"), errors)
        || !Check(workflow.InstanceCount() == 0,
                  QStringLiteral("解绑后不得残留模型实例。"), errors))
    {
        return 8;
    }

    error.clear();
    if (!Check(workflow.SetPendingSceneContext(
                   transferProfile, buildVolume, &error),
               QStringLiteral("解绑后应可改选工艺：%1").arg(error), errors))
    {
        return 9;
    }

    // 5. 解绑保留用户当前选定的工艺，而不是把它退回默认——
    //    否则用户每次解绑后都要重选一遍。
    if (!Check(workflow.SceneProfileId() == transferProfile,
               QStringLiteral("解绑后待生效工艺应为用户新选定值，实为：%1")
                   .arg(workflow.SceneProfileId()),
               errors))
    {
        return 10;
    }

    // 6. 新工艺下可重新成形场景，闭合整条路径。
    hostmodelimportresult reimported;
    error.clear();
    if (!workflow.ImportModel(modelPath, &reimported, &error))
    {
        errors << "解绑后重新导入失败：" << error << Qt::endl;
        return 11;
    }
    if (!Check(workflow.SceneProfileId() == transferProfile,
               QStringLiteral("新场景须绑定改选后的工艺。"), errors))
    {
        return 12;
    }

    QTextStream(stdout)
        << "HOSTFLOW_SCENE_PROFILE_REBIND_PASS profile="
        << workflow.SceneProfileId() << Qt::endl;
    return 0;
}
