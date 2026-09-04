// 宿主场景工艺重绑回归。
//
// 本用例守的是【用户能否走到切片这一步】，而不是【切出的结果对不对】。
// 两者都需要，但此前只有后者有守卫：
//   - T-08 门禁走 `slicer_cli --config`，完全绕过宿主；
//   - matvol_t_host_profile 只构造 Profile 并比对哈希，从不导入模型、不建场景。
// 结果是「导入模型 → 切换工艺 → 开始切片」这条用户实际路径零覆盖，
// 于是「场景一旦绑定就再也换不了工艺，且 UI 无出口」这一缺陷穿过了全部绿灯，
// 直到用户手动点下去才暴露。本文件即该缝隙的守卫。

#include "apps/slicer_ui_host_sim/HostModelImportWorkflow.h"
#include "apps/slicer_ui_host_sim/HostSliceSettings.h"
#include "apps/slicer_ui_host_sim/ModuleClient.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

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

    // 3. 已绑定场景拒绝改选工艺——这正是用户遇到的阻塞，属既定设计而非缺陷。
    error.clear();
    if (!Check(!workflow.SetPendingSceneContext(
                   transferProfile, buildVolume, &error),
               QStringLiteral("已绑定场景不得静默接受新工艺。"), errors)
        || !Check(error.contains(QStringLiteral("新建场景")),
                  QStringLiteral("拒绝原因须指明出路，实为：%1").arg(error),
                  errors))
    {
        return 7;
    }

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
