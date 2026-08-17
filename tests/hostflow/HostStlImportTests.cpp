#include "apps/slicer_ui_host_sim/HostModelImportWorkflow.h"
#include "apps/slicer_ui_host_sim/HostSliceJobController.h"
#include "apps/slicer_ui_host_sim/HostSliceSettings.h"
#include "apps/slicer_ui_host_sim/ModuleClient.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include <array>
#include <cmath>

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

bool WriteBinaryStl(const QString& path, QString* error)
{
    const std::array<std::array<float, 12>, 4> facets{{
        {{0.0F, 0.0F, -1.0F,
          0.0F, 0.0F, 0.0F,
          0.0F, 1.0F, 0.0F,
          1.0F, 0.0F, 0.0F}},
        {{0.0F, -1.0F, 0.0F,
          0.0F, 0.0F, 0.0F,
          1.0F, 0.0F, 0.0F,
          0.0F, 0.0F, 1.0F}},
        {{-1.0F, 0.0F, 0.0F,
          0.0F, 0.0F, 0.0F,
          0.0F, 0.0F, 1.0F,
          0.0F, 1.0F, 0.0F}},
        {{0.577350269F, 0.577350269F, 0.577350269F,
          1.0F, 0.0F, 0.0F,
          0.0F, 1.0F, 0.0F,
          0.0F, 0.0F, 1.0F}}
    }};

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法创建二进制 STL：%1").arg(path);
        }
        return false;
    }
    QByteArray header(80, '\0');
    const QByteArray marker("HOSTFLOW H-E-01 BINARY STL");
    header.replace(0, marker.size(), marker);
    if (file.write(header) != header.size())
    {
        return false;
    }
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    stream << static_cast<quint32>(facets.size());
    for (const auto& item : facets)
    {
        for (const float value : item)
        {
            stream << value;
        }
        stream << static_cast<quint16>(0U);
    }
    return stream.status() == QDataStream::Ok;
}

bool WaitForCompletion(
    HostSliceJobController& controller,
    hostslicejobcompletion* completion)
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
    if (!completed)
    {
        return false;
    }
    *completion = controller.Completion();
    return true;
}

bool RunStlSlice(
    const QString& modulePath,
    const QString& modelPath,
    const QString& packageDirectory,
    const quint64 expectedTriangleCount,
    QTextStream& errors)
{
    ModuleClient client;
    QString error;
    if (!client.Open(modulePath, QByteArrayLiteral("{}"), &error))
    {
        errors << "STL 模块加载失败：" << error << Qt::endl;
        return false;
    }
    HostModelImportWorkflow workflow(client);
    if (!workflow.SetPendingSceneContext(
            QStringLiteral("host-reference-stl"),
            hostbuildvolume{},
            &error))
    {
        errors << "STL 场景上下文失败：" << error << Qt::endl;
        return false;
    }
    hostmodelimportresult imported;
    if (!workflow.ImportModel(modelPath, &imported, &error))
    {
        errors << "STL 导入失败：" << error << Qt::endl;
        return false;
    }
    if (!Check(imported.trianglecount == expectedTriangleCount,
               QStringLiteral("STL 三角形统计不符合 fixture：%1 / %2。")
                   .arg(imported.trianglecount)
                   .arg(expectedTriangleCount),
               errors)
        || !Check(!imported.admission.isEmpty(),
                  QStringLiteral("STL 快速预检结论为空。"), errors))
    {
        return false;
    }
    hostsceneeditresult layoutResult;
    if (!workflow.ApplyGridLayout(
            hostgridlayoutrequest{}, &layoutResult, &error))
    {
        errors << "STL 场景排版失败：" << error << Qt::endl;
        return false;
    }
    hostsceneeditresult transformResult;
    if (!workflow.ApplyTransforms(
            QStringList{imported.instanceid},
            hosttransformrequest{1.0, 2.0, 0.0},
            &transformResult,
            &error))
    {
        errors << "STL 场景边界内移失败：" << error << Qt::endl;
        return false;
    }

    hostslicesettings settings;
    settings.profileid = QStringLiteral("host-reference-stl");
    settings.modelpath = modelPath;
    settings.modelformat = QStringLiteral("stl");
    settings.outputdirectory = packageDirectory;
    settings.layerthicknessmm = 0.2;
    hosteffectiveprofile profile;
    if (!HostEffectiveProfileBuilder::Build(settings, &profile, &error))
    {
        errors << "STL 有效 Profile 失败：" << error << Qt::endl;
        return false;
    }
    const QJsonObject input = profile.profile.value(
        QStringLiteral("input")).toObject();
    if (!Check(input.value(QStringLiteral("format")).toString()
                   == QStringLiteral("stl"),
               QStringLiteral("有效 Profile 未保留 STL 格式。"), errors))
    {
        return false;
    }

    HostSliceJobController controller(client);
    if (!controller.Start(workflow.SceneHandle(), profile, &error))
    {
        errors << "STL 切片提交失败：" << error << Qt::endl;
        return false;
    }
    hostslicejobcompletion completion;
    if (!WaitForCompletion(controller, &completion)
        || !Check(completion.success && !completion.cancelled,
                  QStringLiteral("STL 切片未成功：%1 %2")
                      .arg(completion.code, completion.detail),
                  errors)
        || !Check(QFileInfo(QDir(packageDirectory).filePath(
                      QStringLiteral("manifest.json"))).isFile(),
                  QStringLiteral("STL 切片未生成 manifest.json。"), errors))
    {
        return false;
    }
    return true;
}

bool VerifyNegativeInputs(
    const QString& modulePath,
    const QString& root,
    QTextStream& errors)
{
    const QString disguisedPath = QDir(root).filePath(
        QStringLiteral("disguised.stl"));
    QFile disguised(disguisedPath);
    if (!disguised.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || disguised.write("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n") < 0)
    {
        return false;
    }
    disguised.close();

    const QString truncatedPath = QDir(root).filePath(
        QStringLiteral("truncated.stl"));
    QFile truncated(truncatedPath);
    if (!truncated.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || truncated.write(QByteArray(84, '\0')) != 84)
    {
        return false;
    }
    truncated.close();

    ModuleClient client;
    QString error;
    if (!client.Open(modulePath, QByteArrayLiteral("{}"), &error))
    {
        return false;
    }
    HostModelImportWorkflow workflow(client);
    for (const QString& path : {disguisedPath, truncatedPath})
    {
        hostmodelimportresult result;
        error.clear();
        if (!Check(!workflow.ImportModel(path, &result, &error)
                       && !error.isEmpty(),
                   QStringLiteral("损坏 STL 必须显式失败：%1").arg(path),
                   errors)
            || !Check(workflow.SceneRevision() == 0U,
                      QStringLiteral("损坏 STL 不得推进场景 revision。"),
                      errors))
        {
            return false;
        }
    }
    hostmodelimportresult unknown;
    error.clear();
    const QString unknownPath = QDir(root).filePath(QStringLiteral("model.ply"));
    QFile unknownFile(unknownPath);
    if (!unknownFile.open(QIODevice::WriteOnly)
        || unknownFile.write("ply\n") < 0)
    {
        return false;
    }
    unknownFile.close();
    return Check(!workflow.ImportModel(unknownPath, &unknown, &error)
                     && !error.isEmpty(),
                 QStringLiteral("未知格式必须在宿主侧 fail-closed。"), errors)
        && Check(workflow.SceneRevision() == 0U,
                 QStringLiteral("未知格式不得推进场景 revision。"), errors);
}

bool VerifyExternalStlLanding(
    const QString& modulePath,
    const QString& modelPath,
    QTextStream& errors)
{
    if (modelPath.isEmpty())
    {
        return true;
    }
    ModuleClient client;
    QString error;
    if (!client.Open(modulePath, QByteArrayLiteral("{}"), &error))
    {
        errors << "外部 STL 模块加载失败：" << error << Qt::endl;
        return false;
    }
    HostModelImportWorkflow workflow(client);
    hostmodelimportresult imported;
    if (!workflow.ImportModel(modelPath, &imported, &error))
    {
        errors << "外部 STL 导入失败：" << error << Qt::endl;
        return false;
    }
    const QJsonObject request{
        {QStringLiteral("capability"), QStringLiteral("scene.get_snapshot")},
        {QStringLiteral("sceneHandle"),
         static_cast<qint64>(workflow.SceneHandle())}};
    QByteArray responseBytes;
    if (!client.Execute(
            QJsonDocument(request).toJson(QJsonDocument::Compact),
            &responseBytes,
            &error))
    {
        errors << "外部 STL 场景快照失败：" << error << Qt::endl;
        return false;
    }
    const QJsonObject scene = QJsonDocument::fromJson(responseBytes)
        .object().value(QStringLiteral("scene")).toObject();
    const QJsonArray instances = scene.value(
        QStringLiteral("instances")).toArray();
    if (instances.size() != 1)
    {
        errors << "外部 STL 场景实例数量不正确。" << Qt::endl;
        return false;
    }
    const QJsonObject instance = instances.at(0).toObject();
    const QJsonObject requested = instance.value(
        QStringLiteral("requestedTransform")).toObject();
    const double minimumZ = instance.value(
        QStringLiteral("effectiveBboxMm")).toObject()
        .value(QStringLiteral("min")).toObject()
        .value(QStringLiteral("z")).toDouble();
    const bool passed = imported.trianglecount > 0U
        && requested.value(QStringLiteral("landOnBuildPlate")).toBool()
        && std::abs(minimumZ) <= 1.0e-9;
    if (!passed)
    {
        errors << "外部 STL 未以 minZ=0 的触底状态进入权威场景。"
               << Qt::endl;
    }
    return passed;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QString modulePath = ArgumentValue(
        application.arguments(), QStringLiteral("--module"));
    const QString repositoryRoot = ArgumentValue(
        application.arguments(), QStringLiteral("--repo-root"));
    const QString externalStl = ArgumentValue(
        application.arguments(), QStringLiteral("--external-stl"));
    QTextStream errors(stderr);
    QTemporaryDir root;
    const QString asciiPath = QDir(repositoryRoot).filePath(
        QStringLiteral("samples/models/sample.stl"));
    const QString binaryPath = QDir(root.path()).filePath(
        QStringLiteral("tetrahedron_binary.stl"));
    QString error;
    if (!Check(QFileInfo(modulePath).isFile(),
               QStringLiteral("slicer_module.dll 不存在。"), errors)
        || !Check(QFileInfo(asciiPath).isFile(),
                  QStringLiteral("ASCII STL fixture 不存在。"), errors)
        || !Check(root.isValid(),
                  QStringLiteral("临时目录不可用。"), errors)
        || !Check(WriteBinaryStl(binaryPath, &error),
                  QStringLiteral("二进制 STL fixture 创建失败：%1").arg(error),
                  errors)
        || !RunStlSlice(
            modulePath,
            asciiPath,
            QDir(root.path()).filePath(QStringLiteral("ascii-package")),
            12U,
            errors)
        || !RunStlSlice(
            modulePath,
            binaryPath,
            QDir(root.path()).filePath(QStringLiteral("binary-package")),
            4U,
            errors)
        || !VerifyNegativeInputs(modulePath, root.path(), errors)
        || !VerifyExternalStlLanding(modulePath, externalStl, errors))
    {
        return 2;
    }
    QTextStream(stdout)
        << "HOSTFLOW_H_E_01_STL_PASS ascii=1 binary=1 slices=2 negatives=3"
        << " external=" << (externalStl.isEmpty() ? 0 : 1)
        << Qt::endl;
    return 0;
}
