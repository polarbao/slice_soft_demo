#include "apps/slicer_ui_host_sim/HostModelImportWorkflow.h"
#include "apps/slicer_ui_host_sim/HostSliceJobController.h"
#include "apps/slicer_ui_host_sim/HostSliceSettings.h"
#include "apps/slicer_ui_host_sim/ModuleClient.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

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

hosteffectiveprofile BuildProfile(
    const QString& modelPath,
    const QString& packageDirectory,
    const double layerThicknessMm,
    QTextStream& errors)
{
    hostslicesettings settings;
    settings.profileid = QStringLiteral("host-reference-default");
    settings.modelpath = modelPath;
    settings.modelformat = QStringLiteral("obj");
    settings.outputdirectory = packageDirectory;
    settings.layerthicknessmm = layerThicknessMm;
    hosteffectiveprofile profile;
    QString error;
    if (!HostEffectiveProfileBuilder::Build(settings, &profile, &error))
    {
        errors << "有效 Profile 构造失败：" << error << Qt::endl;
    }
    return profile;
}

bool WaitForCompletion(
    HostSliceJobController& controller,
    const int timeoutMs,
    hostslicejobcompletion* completion,
    QTextStream& errors)
{
    QEventLoop loop;
    bool completed = false;
    QObject::connect(
        &controller,
        &HostSliceJobController::SigCompleted,
        &loop,
        [&]()
        {
            completed = true;
            loop.quit();
        });
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec();
    if (!Check(
            completed,
            QStringLiteral("切片作业在 %1 ms 内未终结。").arg(timeoutMs),
            errors))
    {
        return false;
    }
    *completion = controller.Completion();
    return true;
}

bool HasTemporaryResidue(const QString& root)
{
    QDirIterator iterator(
        root,
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
        const QString name = QFileInfo(iterator.next()).fileName();
        if (name.contains(QStringLiteral(".staging"))
            || name.contains(QStringLiteral(".backup"))
            || name.contains(QStringLiteral(".lease")))
        {
            return true;
        }
    }
    return false;
}

bool VerifySuccessfulJob(
    ModuleClient& client,
    const quint64 sceneHandle,
    const QString& modelPath,
    const QString& packageDirectory,
    QTextStream& errors)
{
    const hosteffectiveprofile profile = BuildProfile(
        modelPath, packageDirectory, 0.2, errors);
    if (profile.profile.isEmpty())
    {
        return false;
    }
    HostSliceJobController controller(client);
    QString error;
    if (!Check(
            controller.Start(sceneHandle, profile, &error),
            QStringLiteral("生产切片提交失败：%1").arg(error),
            errors))
    {
        return false;
    }
    hostslicejobcompletion completion;
    return WaitForCompletion(controller, 60000, &completion, errors)
        && Check(completion.success && !completion.cancelled,
                 QStringLiteral("生产切片未成功：%1 %2 %3 result=%4")
                     .arg(
                         completion.code,
                         completion.message,
                         completion.detail,
                         QString::fromUtf8(QJsonDocument(completion.result)
                                               .toJson(QJsonDocument::Compact))),
                 errors)
        && Check(QFileInfo(QDir(packageDirectory).filePath(
                     QStringLiteral("manifest.json"))).isFile(),
                 QStringLiteral("成功作业未发布 manifest.json。"),
                 errors)
        && Check(!controller.IsActive(),
                 QStringLiteral("终结后仍保留公开作业句柄。"),
                 errors);
}

bool VerifyCancellation(
    ModuleClient& client,
    const quint64 sceneHandle,
    const QString& modelPath,
    const QString& root,
    QTextStream& errors)
{
    const QString packageDirectory = QDir(root).filePath(
        QStringLiteral("cancel-package"));
    const hosteffectiveprofile profile = BuildProfile(
        modelPath, packageDirectory, 0.0001, errors);
    if (profile.profile.isEmpty())
    {
        return false;
    }
    HostSliceJobController controller(client);
    QString error;
    if (!Check(controller.Start(sceneHandle, profile, &error),
               QStringLiteral("取消用例提交失败：%1").arg(error), errors)
        || !Check(controller.Cancel(&error),
                  QStringLiteral("取消请求失败：%1").arg(error), errors))
    {
        return false;
    }
    hostslicejobcompletion completion;
    return WaitForCompletion(controller, 5000, &completion, errors)
        && Check(completion.cancelled && !completion.success,
                 QStringLiteral("作业未以 cancelled 终结：%1")
                     .arg(completion.code),
                 errors)
        && Check(completion.cancellatencyms >= 0
                     && completion.cancellatencyms <= 2000,
                 QStringLiteral("取消延迟超出 2 秒：%1 ms")
                     .arg(completion.cancellatencyms),
                 errors)
        && Check(!QFileInfo::exists(packageDirectory),
                 QStringLiteral("取消作业不得发布生产包。"),
                 errors)
        && Check(!HasTemporaryResidue(root),
                 QStringLiteral("取消后仍存在 staging/backup/lease 残留。"),
                 errors);
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTextStream errors(stderr);
    const QString modulePath = ArgumentValue(
        application.arguments(), QStringLiteral("--module"));
    const QString repositoryRoot = ArgumentValue(
        application.arguments(), QStringLiteral("--repo-root"));
    const QString modelPath = QDir(repositoryRoot).filePath(QStringLiteral(
        "samples/models/openvdb/surface_shell_cube_no_uv.obj"));
    QTemporaryDir outputRoot;
    if (!Check(QFileInfo(modulePath).isFile(),
               QStringLiteral("slicer_module.dll 不存在。"), errors)
        || !Check(QFileInfo(modelPath).isFile(),
                  QStringLiteral("H-B-06 模型 fixture 不存在。"), errors)
        || !Check(outputRoot.isValid(),
                  QStringLiteral("临时输出根目录不可用。"), errors))
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
    if (!workflow.SetPendingSceneContext(
            QStringLiteral("host-reference-default"),
            hostbuildvolume{},
            &error))
    {
        errors << "场景上下文设置失败：" << error << Qt::endl;
        return 4;
    }
    hostmodelimportresult imported;
    if (!workflow.ImportModel(modelPath, &imported, &error))
    {
        errors << "模型导入失败：" << error << Qt::endl;
        return 5;
    }
    hostsceneeditresult layoutResult;
    if (!workflow.ApplyGridLayout(
            hostgridlayoutrequest{}, &layoutResult, &error))
    {
        errors << "生产场景排版失败：" << error << Qt::endl;
        return 5;
    }
    hostsceneeditresult transformResult;
    if (!workflow.ApplyTransforms(
            QStringList{imported.instanceid},
            hosttransformrequest{1.0, 2.0, 0.0},
            &transformResult,
            &error))
    {
        errors << "生产场景边界内移失败：" << error << Qt::endl;
        return 5;
    }

    HostSliceJobController invalidController(client);
    const quint64 callsBeforeInvalid = client.CallCount();
    const hosteffectiveprofile invalidProfile = BuildProfile(
        modelPath,
        QDir(outputRoot.path()).filePath(QStringLiteral("invalid-package")),
        0.2,
        errors);
    if (!Check(!invalidController.Start(0U, invalidProfile, &error),
               QStringLiteral("零 sceneHandle 必须 fail-closed。"), errors)
        || !Check(client.CallCount() == callsBeforeInvalid,
                  QStringLiteral("本地负例不得调用模块。"), errors)
        || !VerifySuccessfulJob(
            client,
            workflow.SceneHandle(),
            modelPath,
            QDir(outputRoot.path()).filePath(QStringLiteral("success-package")),
            errors)
        || !VerifyCancellation(
            client,
            workflow.SceneHandle(),
            modelPath,
            outputRoot.path(),
            errors))
    {
        return 6;
    }

    QTextStream(stdout)
        << "HOSTFLOW_HB06_PASS success=1 cancelLatency<=2000ms residues=0"
        << Qt::endl;
    return 0;
}
