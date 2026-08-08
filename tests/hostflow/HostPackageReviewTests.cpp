#include "apps/slicer_ui_host_sim/HostModelImportWorkflow.h"
#include "apps/slicer_ui_host_sim/HostPackageReviewController.h"
#include "apps/slicer_ui_host_sim/HostSliceJobController.h"
#include "apps/slicer_ui_host_sim/HostSliceSettings.h"
#include "apps/slicer_ui_host_sim/ModuleClient.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QImage>
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

bool WaitForCompletion(
    HostSliceJobController& controller,
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
    QTimer::singleShot(60000, &loop, &QEventLoop::quit);
    loop.exec();
    if (!Check(completed, QStringLiteral("H-B-07 fixture 切片超时。"), errors))
    {
        return false;
    }
    *completion = controller.Completion();
    return true;
}

bool ProducePackage(
    ModuleClient& client,
    const QString& modelPath,
    const QString& packageDirectory,
    QTextStream& errors)
{
    HostModelImportWorkflow workflow(client);
    QString error;
    if (!workflow.SetPendingSceneContext(
            QStringLiteral("host-reference-default"),
            hostbuildvolume{},
            &error))
    {
        errors << "场景上下文设置失败：" << error << Qt::endl;
        return false;
    }
    hostmodelimportresult imported;
    if (!workflow.ImportModel(modelPath, &imported, &error))
    {
        errors << "模型导入失败：" << error << Qt::endl;
        return false;
    }
    hostsceneeditresult layout;
    if (!workflow.ApplyGridLayout(hostgridlayoutrequest{}, &layout, &error))
    {
        errors << "场景排版失败：" << error << Qt::endl;
        return false;
    }
    hostsceneeditresult translated;
    if (!workflow.ApplyTransforms(
            QStringList{imported.instanceid},
            hosttransformrequest{1.0, 2.0, 0.0},
            &translated,
            &error))
    {
        errors << "场景边界内移失败：" << error << Qt::endl;
        return false;
    }

    hostslicesettings settings;
    settings.profileid = QStringLiteral("host-reference-default");
    settings.modelpath = modelPath;
    settings.modelformat = QStringLiteral("obj");
    settings.outputdirectory = packageDirectory;
    settings.layerthicknessmm = 0.2;
    hosteffectiveprofile profile;
    if (!HostEffectiveProfileBuilder::Build(settings, &profile, &error))
    {
        errors << "有效 Profile 构造失败：" << error << Qt::endl;
        return false;
    }
    HostSliceJobController job(client);
    if (!job.Start(workflow.SceneHandle(), profile, &error))
    {
        errors << "切片提交失败：" << error << Qt::endl;
        return false;
    }
    hostslicejobcompletion completion;
    return WaitForCompletion(job, &completion, errors)
        && Check(
            completion.success,
            QStringLiteral("H-B-07 fixture 切片失败：%1 %2")
                .arg(completion.code, completion.message),
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
                  QStringLiteral("H-B-07 模型 fixture 不存在。"), errors)
        || !Check(outputRoot.isValid(),
                  QStringLiteral("H-B-07 临时目录不可用。"), errors))
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
    const QString packageDirectory = QDir(outputRoot.path()).filePath(
        QStringLiteral("review-package"));
    if (!ProducePackage(client, modelPath, packageDirectory, errors))
    {
        return 4;
    }

    HostPackageReviewController controller(client);
    const bool loaded = controller.Load(packageDirectory, &error);
    if (!Check(loaded,
               QStringLiteral("结果包加载失败：%1").arg(error), errors))
    {
        return 5;
    }
    const hostpackagereview& review = controller.Review();
    if (!Check(review.valid, QStringLiteral("生产包 verify 未通过。"), errors)
        || !Check(review.schema == QStringLiteral("p0.rgbwsv.2"),
                  QStringLiteral("生产包 schema 不闭合。"), errors)
        || !Check(review.bitdepth == 8
                      && review.polarity == QStringLiteral("black_is_print"),
                  QStringLiteral("生产包位深或极性不闭合。"), errors)
        || !Check(review.channels == QStringList{
                      QStringLiteral("R"), QStringLiteral("G"),
                      QStringLiteral("B"), QStringLiteral("W"),
                      QStringLiteral("S"), QStringLiteral("V")},
                  QStringLiteral("生产包通道顺序不闭合。"), errors)
        || !Check(review.layercount == review.layers.size()
                      && review.layercount > 0,
                  QStringLiteral("逐层描述数量不闭合。"), errors)
        || !Check(review.instancecount == 1
                      && !review.profileversion.isEmpty()
                      && !review.profilehash.isEmpty(),
                  QStringLiteral("实例或 Profile 摘要未持久化。"), errors))
    {
        return 6;
    }
    for (int layerIndex = 0; layerIndex < review.layers.size(); ++layerIndex)
    {
        if (!Check(review.layers.at(layerIndex).layerindex == layerIndex,
                   QStringLiteral("逐层描述顺序错误。"), errors))
        {
            return 6;
        }
    }

    QString previewPath;
    const bool previewRendered = controller.RenderPreview(
            0,
            QStringList{
                QStringLiteral("R"), QStringLiteral("G"),
                QStringLiteral("B"), QStringLiteral("W"),
                QStringLiteral("S"), QStringLiteral("V")},
            &previewPath,
            &error);
    if (!Check(previewRendered,
        QStringLiteral("生产 TIFF 预览失败：%1").arg(error), errors)
        || !Check(!QImage(previewPath).isNull(),
                  QStringLiteral("模块预览无法由 QImage 解码。"), errors))
    {
        return 7;
    }

    hostpackagereport report;
    const bool reportRead = controller.ReadReport(
        QStringLiteral("slice"), &report, &error);
    if (!Check(reportRead,
        QStringLiteral("slice 报告读取失败：%1").arg(error), errors)
        || !Check(report.name == QStringLiteral("slice")
                      && !report.schema.isEmpty()
                      && !report.data.isEmpty(),
                  QStringLiteral("slice 报告响应不闭合。"), errors))
    {
        return 8;
    }

    HostPackageReviewController invalidController(client);
    if (!Check(!invalidController.Load(
            QDir(outputRoot.path()).filePath(QStringLiteral("missing")),
            &error),
        QStringLiteral("缺失生产包必须 fail-closed。"), errors)
        || !Check(!controller.RenderPreview(
            review.layercount,
            QStringList{QStringLiteral("S")},
            &previewPath,
            &error),
        QStringLiteral("越界层预览必须 fail-closed。"), errors))
    {
        return 9;
    }

    QTextStream(stdout)
        << "HOSTFLOW_HB07_PASS layers=" << review.layercount
        << " preview=production_tiff report=slice" << Qt::endl;
    return 0;
}
