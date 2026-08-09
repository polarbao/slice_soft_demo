#include "apps/slicer_ui_host_sim/HostModelImportWorkflow.h"
#include "apps/slicer_ui_host_sim/HostSliceSettings.h"
#include "apps/slicer_ui_host_sim/HostSliceSettingsPanel.h"
#include "apps/slicer_ui_host_sim/ModuleClient.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QSpinBox>
#include <QStringList>
#include <QTemporaryDir>
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

hostslicesettings MakeSettings(
    const QString& modelPath,
    const QString& outputDirectory)
{
    return hostslicesettings{
        QStringLiteral("host-reference-default"),
        modelPath,
        QStringLiteral("obj"),
        outputDirectory,
        635,
        600,
        0.038,
        HostMaterialStrategy::RgbSolid,
        hostbuildvolume{}};
}

bool VerifyEffectiveProfiles(
    const QString& modelPath,
    const QString& outputDirectory,
    QTextStream& errors)
{
    hostslicesettings settings = MakeSettings(modelPath, outputDirectory);
    hosteffectiveprofile first;
    QString error;
    if (!Check(
            HostEffectiveProfileBuilder::Build(settings, &first, &error),
            QStringLiteral("默认有效 Profile 构造失败：%1").arg(error),
            errors))
    {
        return false;
    }
    const QJsonObject output = first.profile.value(
        QStringLiteral("output")).toObject();
    const QJsonObject material = first.profile.value(
        QStringLiteral("modelMaterial")).toObject();
    const QJsonObject support = first.profile.value(
        QStringLiteral("support")).toObject();
    const QJsonObject processSupport = first.profile.value(
        QStringLiteral("materialProcessProfile")).toObject().value(
            QStringLiteral("support")).toObject();
    if (!Check(
            output.value(QStringLiteral("dpiX")).toInt() == 635
                && output.value(QStringLiteral("dpiY")).toInt() == 600
                && output.value(
                    QStringLiteral("layerThicknessMm")).toDouble() == 0.038,
            QStringLiteral("参考默认 DPI/层厚未进入有效 Profile。"),
            errors)
        || !Check(
            material.value(QStringLiteral("materialChannel")).toString()
                == QStringLiteral("RGB"),
            QStringLiteral("默认材料策略应映射到 RGB。"),
            errors)
        || !Check(
            support.value(QStringLiteral("enabled")).toBool()
                && support.value(QStringLiteral("mode")).toString()
                    == QStringLiteral("bottom_projection")
                && support.value(QStringLiteral("placement")).toString()
                    == QStringLiteral("lower")
                && support.value(QStringLiteral("value")).toInt(-1) == 0
                && support.value(QStringLiteral("internalVoid")).toObject()
                    .value(QStringLiteral("enabled")).toBool()
                && processSupport.value(
                    QStringLiteral("expected")).toBool(),
            QStringLiteral("默认 lower support Profile 段不完整。"),
            errors))
    {
        return false;
    }

    hosteffectiveprofile repeated;
    if (!Check(
            HostEffectiveProfileBuilder::Build(
                settings, &repeated, &error)
                && repeated.profilehash == first.profilehash,
            QStringLiteral("相同输入的 Profile hash 必须确定。"),
            errors))
    {
        return false;
    }
    settings.dpix = 636;
    hosteffectiveprofile changed;
    if (!Check(
            HostEffectiveProfileBuilder::Build(settings, &changed, &error)
                && changed.profilehash != first.profilehash,
            QStringLiteral("DPI 变化必须使 Profile hash 变化。"),
            errors))
    {
        return false;
    }
    settings = MakeSettings(modelPath, outputDirectory);
    settings.support.mode = HostSupportMode::FullVerticalProjection;
    settings.support.offsetmm = 0.25;
    settings.support.minareapx = 32;
    settings.support.baseprojection.enabled = true;
    settings.support.baseprojection.layercount = 30;
    hosteffectiveprofile supportChanged;
    if (!Check(
            HostEffectiveProfileBuilder::Build(
                settings, &supportChanged, &error)
                && supportChanged.profilehash != first.profilehash,
            QStringLiteral("支撑参数变化必须使 Profile hash 变化。"),
            errors))
    {
        return false;
    }
    const QJsonObject changedSupport = supportChanged.profile.value(
        QStringLiteral("support")).toObject();
    if (!Check(
            changedSupport.value(QStringLiteral("mode")).toString()
                    == QStringLiteral("full_vertical_projection")
                && !changedSupport.contains(QStringLiteral("placement"))
                && changedSupport.value(
                    QStringLiteral("baseProjection")).toObject().value(
                        QStringLiteral("layerCount")).toInt() == 30,
            QStringLiteral("高级支撑参数未进入有效 Profile。"),
            errors))
    {
        return false;
    }

    settings = MakeSettings(modelPath, outputDirectory);
    settings.support.enabled = false;
    settings.support.mode = HostSupportMode::None;
    settings.support.internalvoid.enabled = false;
    settings.support.baseprojection.enabled = false;
    hosteffectiveprofile disabledSupport;
    if (!Check(
            HostEffectiveProfileBuilder::Build(
                settings, &disabledSupport, &error),
            QStringLiteral("关闭支撑的有效 Profile 构造失败：%1").arg(error),
            errors))
    {
        return false;
    }
    const QJsonObject disabledRoot = disabledSupport.profile.value(
        QStringLiteral("support")).toObject();
    const QJsonObject disabledProcess = disabledSupport.profile.value(
        QStringLiteral("materialProcessProfile")).toObject().value(
            QStringLiteral("support")).toObject();
    if (!Check(
            !disabledRoot.value(QStringLiteral("enabled")).toBool()
                && disabledRoot.value(QStringLiteral("mode")).toString()
                    == QStringLiteral("none")
                && !disabledProcess.value(
                    QStringLiteral("expected")).toBool(),
            QStringLiteral("关闭支撑时 expected/mode 未同步。"),
            errors))
    {
        return false;
    }

    settings = MakeSettings(modelPath, outputDirectory);
    settings.materialstrategy = HostMaterialStrategy::WhiteSolid;
    hosteffectiveprofile white;
    if (!Check(
            HostEffectiveProfileBuilder::Build(settings, &white, &error),
            QStringLiteral("白墨有效 Profile 构造失败：%1").arg(error),
            errors))
    {
        return false;
    }
    const QJsonObject whiteMaterial = white.profile.value(
        QStringLiteral("modelMaterial")).toObject();
    if (!Check(
            whiteMaterial.value(
                QStringLiteral("materialChannel")).toString()
                    == QStringLiteral("W")
                && whiteMaterial.value(
                    QStringLiteral("whiteValue")).toInt() == 0
                && whiteMaterial.value(
                    QStringLiteral("varnishValue")).toInt() == 255,
            QStringLiteral("白墨实体映射必须只写 W 通道。"),
            errors))
    {
        return false;
    }

    settings.outputdirectory = QStringLiteral("relative/package");
    error.clear();
    if (!Check(
        !HostEffectiveProfileBuilder::Validate(settings, &error)
            && !error.isEmpty(),
        QStringLiteral("相对输出目录必须 fail-closed。"),
        errors))
    {
        return false;
    }
    settings = MakeSettings(modelPath, outputDirectory);
    settings.materialstrategy = static_cast<HostMaterialStrategy>(99);
    error.clear();
    if (!Check(
        !HostEffectiveProfileBuilder::Validate(settings, &error),
        QStringLiteral("未知材料策略必须 fail-closed。"),
        errors))
    {
        return false;
    }
    settings = MakeSettings(modelPath, outputDirectory);
    settings.support.baseprojection.layercount = 1001;
    error.clear();
    return Check(
        !HostEffectiveProfileBuilder::Validate(settings, &error),
        QStringLiteral("越界铺底层数必须 fail-closed。"),
        errors);
}

bool VerifyPanelIsLocal(
    ModuleClient& client,
    const QString& modelPath,
    const QString& outputDirectory,
    QTextStream& errors)
{
    HostSliceSettingsPanel panel;
    panel.SetSelectedProfileId(QStringLiteral("host-reference-default"));
    auto* outputEdit = panel.findChild<QLineEdit*>(
        QStringLiteral("hostSliceOutputEdit"));
    auto* dpiXSpin = panel.findChild<QSpinBox*>(
        QStringLiteral("hostSliceDpiXSpin"));
    auto* materialCombo = panel.findChild<QComboBox*>(
        QStringLiteral("hostSliceMaterialCombo"));
    auto* supportEnabled = panel.findChild<QCheckBox*>(
        QStringLiteral("hostSupportEnabledCheck"));
    auto* supportMode = panel.findChild<QComboBox*>(
        QStringLiteral("hostSupportModeCombo"));
    auto* supportOffset = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("hostSupportOffsetSpin"));
    if (!Check(
            outputEdit != nullptr && dpiXSpin != nullptr
                && materialCombo != nullptr && supportEnabled != nullptr
                && supportMode != nullptr && supportOffset != nullptr,
            QStringLiteral("切片设置面板控件不完整。"),
            errors))
    {
        return false;
    }
    outputEdit->setText(outputDirectory);
    panel.SetModelPath(modelPath);
    client.ResetCallCount();
    dpiXSpin->setValue(700);
    materialCombo->setCurrentIndex(1);
    supportMode->setCurrentIndex(2);
    supportOffset->setValue(0.2);
    QCoreApplication::processEvents();
    if (!Check(
        panel.IsReady(),
        QStringLiteral("有效设置未生成 Profile 预览。"),
        errors)
        || !Check(
            client.CallCount() == 0U,
            QStringLiteral("切片参数编辑不得调用 DLL。"),
            errors))
    {
        return false;
    }
    panel.SetSelectedProfileId(
        QStringLiteral("host-reference-package-review"), false);
    return Check(
        !panel.IsReady(),
        QStringLiteral("不含 slice.rgbwsv 的诊断 Profile 不得生成切片配置。"),
        errors);
}

bool VerifySceneAuthority(
    ModuleClient& client,
    const QString& modelPath,
    QTextStream& errors)
{
    HostModelImportWorkflow workflow(client);
    hostbuildvolume volume;
    volume.widthmm = 210.0;
    volume.heightmm = 90.0;
    volume.zlimitmm = 50.0;
    QString error;
    if (!Check(
            workflow.SetPendingSceneContext(
                QStringLiteral("host-reference-material-parity"),
                volume,
                &error),
            QStringLiteral("宿主场景上下文设置失败：%1").arg(error),
            errors))
    {
        return false;
    }
    hostmodelimportresult imported;
    if (!Check(
            workflow.ImportModel(modelPath, &imported, &error),
            QStringLiteral("场景上下文验证模型导入失败：%1").arg(error),
            errors))
    {
        return false;
    }

    const QJsonObject request{
        {QStringLiteral("capability"), QStringLiteral("scene.get_snapshot")},
        {QStringLiteral("sceneHandle"),
         static_cast<qint64>(workflow.SceneHandle())}};
    QByteArray responseBytes;
    if (!Check(
            client.Execute(
                QJsonDocument(request).toJson(QJsonDocument::Compact),
                &responseBytes,
                &error),
            QStringLiteral("场景快照读取失败：%1").arg(error),
            errors))
    {
        return false;
    }
    const QJsonObject response = QJsonDocument::fromJson(
        responseBytes).object();
    const QJsonObject scene = response.value(QStringLiteral("scene")).toObject();
    const QJsonObject snapshotVolume = scene.value(
        QStringLiteral("buildVolume")).toObject();
    if (!Check(
            scene.value(QStringLiteral("resolvedProfileId")).toString()
                == QStringLiteral("host-reference-material-parity")
                && snapshotVolume.value(
                    QStringLiteral("widthMm")).toDouble() == 210.0
                && snapshotVolume.value(
                    QStringLiteral("heightMm")).toDouble() == 90.0,
            QStringLiteral("首次 Commit 未采用宿主 Profile/buildVolume。"),
            errors))
    {
        return false;
    }

    const quint64 revision = workflow.SceneRevision();
    volume.widthmm = 220.0;
    error.clear();
    return Check(
        !workflow.SetPendingSceneContext(
            QStringLiteral("host-reference-material-parity"),
            volume,
            &error)
            && workflow.SceneRevision() == revision,
        QStringLiteral("已绑定场景必须拒绝 buildVolume 静默变更。"),
        errors);
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
    const QString modelPath = QDir(repositoryRoot).filePath(QStringLiteral(
        "samples/models/openvdb/surface_shell_cube_no_uv.obj"));
    QTemporaryDir outputRoot;
    if (!Check(QFileInfo(modulePath).isFile(),
               QStringLiteral("slicer_module.dll 不存在。"), errors)
        || !Check(QFileInfo(modelPath).isFile(),
                  QStringLiteral("H-B-05 模型 fixture 不存在。"), errors)
        || !Check(outputRoot.isValid(),
                  QStringLiteral("临时输出根目录不可用。"), errors))
    {
        return 2;
    }
    const QString outputDirectory = QDir(outputRoot.path()).filePath(
        QStringLiteral("package"));

    ModuleClient client;
    QString error;
    if (!client.Open(modulePath, QByteArrayLiteral("{}"), &error))
    {
        errors << "模块加载失败：" << error << Qt::endl;
        return 3;
    }
    if (!VerifyEffectiveProfiles(modelPath, outputDirectory, errors)
        || !VerifyPanelIsLocal(
            client, modelPath, outputDirectory, errors)
        || !VerifySceneAuthority(client, modelPath, errors))
    {
        return 4;
    }
    QTextStream(stdout)
        << "HOSTFLOW_HE03_PASS profile=host-reference-default"
        << " dpi=635x600 layer=0.038 support=editable" << Qt::endl;
    return 0;
}
