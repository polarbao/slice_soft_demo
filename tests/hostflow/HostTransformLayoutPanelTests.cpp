#include "apps/slicer_ui_host_sim/HostModelImportWorkflow.h"
#include "apps/slicer_ui_host_sim/HostTransformLayoutPanel.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTextStream>

#include <cmath>

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
    hostmodelimportresult first;
    hostmodelimportresult second;
    if (!workflow.ImportModel(
            QDir(repositoryRoot).filePath(QStringLiteral(
                "samples/models/openvdb/surface_shell_cube_no_uv.obj")),
            &first,
            &error)
        || !workflow.ImportModel(
            QDir(repositoryRoot).filePath(QStringLiteral(
                "samples/models/3mf/single_rgb_cube_stored.3mf")),
            &second,
            &error))
    {
        errors << "H-B-03 模型准备失败：" << error << Qt::endl;
        return 4;
    }

    HostTransformLayoutPanel panel;
    panel.SetCommandsEnabled(true);
    panel.SetSceneState(workflow.InstanceCount(), workflow.SceneRevision());
    panel.SetSelectedInstances(QStringList{first.instanceid, second.instanceid});
    bool transformCommitted = false;
    bool landCommitted = false;
    bool layoutCommitted = false;
    hostsceneeditresult transformResult;
    hostsceneeditresult layoutResult;
    QObject::connect(
        &panel,
        &HostTransformLayoutPanel::SigTransformRequested,
        [&](const QStringList& instanceIds,
            const double deltaXMm,
            const double deltaYMm,
            const double rotateXDegrees,
            const double rotateYDegrees,
            const double rotateZDegrees,
            const double scaleFactor,
            const bool mirrorX,
            const bool mirrorY,
            const bool landOnBuildPlate)
        {
            transformCommitted = workflow.ApplyTransforms(
                instanceIds,
                hosttransformrequest{
                    deltaXMm,
                    deltaYMm,
                    rotateXDegrees,
                    rotateYDegrees,
                    rotateZDegrees,
                    scaleFactor,
                    mirrorX,
                    mirrorY,
                    landOnBuildPlate},
                &transformResult,
                &error);
        });
    QObject::connect(
        &panel,
        &HostTransformLayoutPanel::SigLandOnBuildPlateRequested,
        [&](const QStringList& instanceIds)
        {
            landCommitted = workflow.LandOnBuildPlate(
                instanceIds, &transformResult, &error);
        });
    QObject::connect(
        &panel,
        &HostTransformLayoutPanel::SigLayoutRequested,
        [&](const int maxColumns,
            const int maxRows,
            const double columnGapMm,
            const double rowGapMm)
        {
            layoutCommitted = workflow.ApplyGridLayout(
                hostgridlayoutrequest{
                    maxColumns, maxRows, columnGapMm, rowGapMm},
                &layoutResult,
                &error);
        });

    auto* deltaX = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("hostTransformDeltaXSpin"));
    auto* rotateX = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("hostTransformRotateXSpin"));
    auto* rotateY = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("hostTransformRotateYSpin"));
    auto* rotateZ = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("hostTransformRotateZSpin"));
    auto* scale = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("hostTransformScaleSpin"));
    auto* mirrorX = panel.findChild<QCheckBox*>(
        QStringLiteral("hostTransformMirrorXCheck"));
    auto* autoLand = panel.findChild<QCheckBox*>(
        QStringLiteral("hostTransformAutoLandCheck"));
    auto* applyTransform = panel.findChild<QPushButton*>(
        QStringLiteral("hostTransformApplyButton"));
    auto* land = panel.findChild<QPushButton*>(
        QStringLiteral("hostTransformLandButton"));
    auto* columns = panel.findChild<QSpinBox*>(
        QStringLiteral("hostLayoutColumnsSpin"));
    auto* rows = panel.findChild<QSpinBox*>(
        QStringLiteral("hostLayoutRowsSpin"));
    auto* applyLayout = panel.findChild<QPushButton*>(
        QStringLiteral("hostLayoutApplyButton"));
    if (!Check(deltaX != nullptr && rotateX != nullptr
                   && rotateY != nullptr && rotateZ != nullptr
                   && scale != nullptr
                   && mirrorX != nullptr && autoLand != nullptr
                   && autoLand->isChecked() && applyTransform != nullptr
                   && land != nullptr
                   && columns != nullptr && rows != nullptr
                   && applyLayout != nullptr,
               QStringLiteral("变换或排版控件不完整。"), errors))
    {
        return 5;
    }
    const hostgridlayoutrequest defaultLayout = panel.LayoutRequest();
    if (!Check(
            defaultLayout.maxcolumns == 11
                && defaultLayout.maxrows == 2
                && std::abs(defaultLayout.columngapmm - 10.0) < 1.0e-9
                && std::abs(defaultLayout.rowgapmm - 10.0) < 1.0e-9,
            QStringLiteral(
                "导入自动排版与手动排版必须共享 11x2、10 mm 默认参数。"),
            errors))
    {
        return 11;
    }

    client.ResetCallCount();
    deltaX->setValue(4.0);
    rotateX->setValue(8.0);
    rotateY->setValue(-6.0);
    rotateZ->setValue(15.0);
    scale->setValue(1.05);
    mirrorX->setChecked(true);
    if (!Check(client.CallCount() == 0U,
               QStringLiteral("本地参数编辑不得跨 DLL 调用。"), errors))
    {
        return 6;
    }
    applyTransform->click();
    if (!Check(transformCommitted,
               QStringLiteral("选中实例变换 Commit 失败：%1").arg(error),
               errors)
        || !Check(workflow.SceneRevision() == 3U,
                  QStringLiteral("多实例变换只应递增一次 revision。"), errors)
        || !Check(transformResult.scenerevision == 3U,
                  QStringLiteral("变换结果未返回权威 revision。"), errors)
        || !Check(client.CallCount() > 0U,
                  QStringLiteral("点击提交必须跨公开 SPI。"), errors))
    {
        return 7;
    }

    land->click();
    if (!Check(landCommitted,
               QStringLiteral("显式触底 Commit 失败：%1").arg(error), errors)
        || !Check(workflow.SceneRevision() == 4U,
                  QStringLiteral("显式触底只应递增一次 revision。"), errors))
    {
        return 12;
    }

    client.ResetCallCount();
    columns->setValue(2);
    rows->setValue(1);
    if (!Check(client.CallCount() == 0U,
               QStringLiteral("排版参数编辑不得跨 DLL 调用。"), errors))
    {
        return 8;
    }
    applyLayout->click();
    if (!Check(layoutCommitted,
               QStringLiteral("规则排版 Commit 失败：%1").arg(error), errors)
        || !Check(workflow.SceneRevision() == 5U,
                  QStringLiteral("规则排版只应递增一次 revision。"), errors)
        || !Check(layoutResult.collisioncount == 0,
                  QStringLiteral("规则排版后不应存在模型碰撞。"), errors)
        || !Check(layoutResult.outofboundscount == 0,
                  QStringLiteral("规则排版后不应存在越界模型。"), errors))
    {
        return 9;
    }

    client.ResetCallCount();
    hostsceneeditresult invalidResult;
    error.clear();
    if (!Check(!workflow.ApplyGridLayout(
                   hostgridlayoutrequest{1, 1, 10.0, 10.0},
                   &invalidResult,
                   &error),
               QStringLiteral("容量不足的排版必须 fail-closed。"), errors)
        || !Check(client.CallCount() == 0U,
                  QStringLiteral("宿主可判定的排版负例不得跨 DLL。"), errors)
        || !Check(workflow.SceneRevision() == 5U,
                  QStringLiteral("排版负例不得改变 revision。"), errors))
    {
        return 10;
    }

    QTextStream(stdout)
        << "HOSTFLOW_HB03_PASS revision=" << workflow.SceneRevision()
        << Qt::endl;
    return 0;
}
