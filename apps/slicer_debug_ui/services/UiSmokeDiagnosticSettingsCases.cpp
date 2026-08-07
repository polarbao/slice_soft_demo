#include "UiSmokeTestInternal.h"

using ui_smoke_test_support::BuildExperimentalReportFixture;
using ui_smoke_test_support::BuildMaterialClosureReportFixture;
using ui_smoke_test_support::BuildOpenVdbUtilityReportFixture;
using ui_smoke_test_support::ClosedBoxObjFixture;
using ui_smoke_test_support::ContainsAll;
using ui_smoke_test_support::GlobalRect;
using ui_smoke_test_support::OpenTriangleObjFixture;
using ui_smoke_test_support::ReadJsonObject;
using ui_smoke_test_support::WaitForCondition;
using ui_smoke_test_support::WriteJsonFixture;
using ui_smoke_test_support::WritePreflightFixture;

int UiSmokeTestRunner::DiagnosticSettingsControls(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    ContextInspector* inspector =
        window.findChild<ContextInspector*>(
            QStringLiteral("contextInspector"));
    QDoubleSpinBox* widthSpin =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral(
                "diagnosticTextureSurfaceWidthSpin"));
    QSlider* widthSlider =
        window.findChild<QSlider*>(
            QStringLiteral(
                "diagnosticTextureSurfaceWidthSlider"));
    QComboBox* fillMaterial =
        window.findChild<QComboBox*>(
            QStringLiteral(
                "diagnosticModelFillMaterialCombo"));
    QLabel* subject =
        window.findChild<QLabel*>(
            QStringLiteral(
                "diagnosticSubjectSummaryLabel"));
    QLabel* bounds =
        window.findChild<QLabel*>(
            QStringLiteral(
                "diagnosticWidthBoundsLabel"));
    QLabel* backend =
        window.findChild<QLabel*>(
            QStringLiteral(
                "diagnosticBackendAvailabilityLabel"));
    QLabel* status =
        window.findChild<QLabel*>(
            QStringLiteral(
                "diagnosticStatusLabel"));
    QLabel* diagnosticOnlyNotice =
        window.findChild<QLabel*>(
            QStringLiteral(
                "diagnosticOnlyNoticeLabel"));
    QPushButton* startAnalysis =
        window.findChild<QPushButton*>(
            QStringLiteral(
                "diagnosticStartAnalysisButton"));
    QPushButton* cancelAnalysis =
        window.findChild<QPushButton*>(
            QStringLiteral(
                "diagnosticCancelAnalysisButton"));
    if (inspector == nullptr
        || widthSpin == nullptr
        || widthSlider == nullptr
        || fillMaterial == nullptr
        || subject == nullptr
        || bounds == nullptr
        || backend == nullptr
        || status == nullptr
        || diagnosticOnlyNotice == nullptr
        || startAnalysis == nullptr
        || cancelAnalysis == nullptr)
    {
        return fail(QStringLiteral(
            "12E-09A-03 diagnostic controls missing"));
    }
    if (widthSpin->decimals() != 2
        || std::abs(widthSpin->singleStep() - 0.01)
            > 1.0e-9
        || std::abs(widthSpin->minimum() - 0.10)
            > 1.0e-9
        || std::abs(widthSpin->maximum() - 6.00)
            > 1.0e-9
        || widthSlider->minimum() != 10
        || widthSlider->maximum() != 600)
    {
        return fail(QStringLiteral(
            "12E-09A-03 width range or precision mismatch"));
    }
    if (fillMaterial->findData(
            QStringLiteral("white"))
            < 0
        || fillMaterial->findData(
               QStringLiteral("varnish"))
            < 0
        || fillMaterial->findData(
               QStringLiteral("rgb"))
            < 0)
    {
        return fail(QStringLiteral(
            "12E-09A-03 model-fill material options missing"));
    }
    if (widthSpin->toolTip().isEmpty()
        || widthSlider->toolTip().isEmpty()
        || fillMaterial->toolTip().isEmpty()
        || startAnalysis->toolTip().isEmpty()
        || cancelAnalysis->toolTip().isEmpty()
        || subject->text().isEmpty()
        || bounds->text().isEmpty()
        || backend->text().isEmpty()
        || status->text().isEmpty()
        || !diagnosticOnlyNotice->text().contains(
            QStringLiteral("不会修改生产")))
    {
        return fail(QStringLiteral(
            "12E-09A-03 Chinese status or tooltip missing"));
    }
    widthSpin->setValue(0.23);
    if (widthSlider->value() != 23)
    {
        return fail(QStringLiteral(
            "12E-09A-03 spinbox did not update slider"));
    }
    widthSlider->setValue(41);
    if (std::abs(widthSpin->value() - 0.41)
        > 1.0e-9)
    {
        return fail(QStringLiteral(
            "12E-09A-03 slider did not update spinbox"));
    }
    if (widthSpin->isEnabled()
        || widthSlider->isEnabled()
        || fillMaterial->isEnabled())
    {
        return fail(QStringLiteral(
            "12E-09A-03 controls must be disabled without a model"));
    }
    if (startAnalysis->isEnabled()
        || cancelAnalysis->isEnabled())
    {
        return fail(QStringLiteral(
            "12E-09A-04 actions must be disabled without a model"));
    }

    DiagnosticSettingsPresentation presentation;
    presentation.subjectsummary =
        QStringLiteral(
            "场景 这是一个用于验证最长中文排版的诊断场景身份 / "
            "revision 123456 / 当前实例 "
            "instance-with-a-long-readable-identity");
    presentation.minimumwidthmm = 0.12;
    presentation.maximumwidthmm = 5.43;
    presentation.alltexturethresholdmm = 2.34;
    presentation.backendavailability =
        QStringLiteral(
            "Legacy CPU 可用；OpenVDB 候选后端可用，"
            "但诊断可用不等同于生产准入。");
    presentation.status =
        QStringLiteral(
            "诊断参数已经准备完成；当前仅验证中文参数、状态和 tooltip，"
            "尚未启动异步拓扑、距离场、纹理转移或栅格映射分析。");
    presentation.blockingreasons = QStringList{
        QStringLiteral(
            "这是最长中文阻断原因示例，用于确认窗口缩放时文本不会覆盖相邻控件。")};
    presentation.controlsenabled = true;
    inspector->SetDiagnosticPresentation(
        presentation);
    inspector->ShowTextureDiagnosticPage();
    widthSpin->setValue(0.37);
    const int varnishIndex =
        fillMaterial->findData(
            QStringLiteral("varnish"));
    fillMaterial->setCurrentIndex(varnishIndex);
    if (!widthSpin->isEnabled()
        || !widthSlider->isEnabled()
        || !fillMaterial->isEnabled()
        || !startAnalysis->isEnabled()
        || cancelAnalysis->isEnabled()
        || std::abs(
               window
                   .m_diagnosticTextureSurfaceWidthMm
               - 0.37)
            > 1.0e-9
        || window.m_diagnosticModelFillMaterial
            != QStringLiteral("varnish")
        || !bounds->text().contains(
            QStringLiteral("0.12 mm"))
        || !bounds->text().contains(
            QStringLiteral("5.43 mm"))
        || !bounds->text().contains(
            QStringLiteral("2.34 mm")))
    {
        return fail(QStringLiteral(
            "12E-09A-03 available presentation or edits mismatch"));
    }
    if (std::abs(widthSpin->minimum() - 0.12)
            > 1.0e-9
        || std::abs(widthSpin->maximum() - 5.43)
            > 1.0e-9
        || widthSlider->minimum() != 12
        || widthSlider->maximum() != 543)
    {
        return fail(QStringLiteral(
            "12E-09A-03 derived width bounds not applied"));
    }

    presentation.analysisrunning = true;
    presentation.status =
        QStringLiteral(
            "运行中（running）：后台正在执行拓扑、距离、"
            "纹理分区和栅格映射。");
    inspector->SetDiagnosticPresentation(presentation);
    if (widthSpin->isEnabled()
        || widthSlider->isEnabled()
        || fillMaterial->isEnabled()
        || startAnalysis->isEnabled()
        || !cancelAnalysis->isEnabled()
        || !status->text().contains(
            QStringLiteral("运行中")))
    {
        return fail(QStringLiteral(
            "12E-09A-04 running action state mismatch"));
    }
    presentation.analysisrunning = false;
    inspector->SetDiagnosticPresentation(presentation);

    presentation.maximumwidthmm.reset();
    presentation.alltexturethresholdmm.reset();
    presentation.status =
        QStringLiteral(
            "诊断失败：strict_closed rejected mesh with non-manifold edges");
    inspector->SetDiagnosticPresentation(presentation);
    if (!bounds->text().contains(
            QStringLiteral("最大 未评估"))
        || !bounds->text().contains(
            QStringLiteral("全纹理阈值 未评估"))
        || bounds->text().contains(
            QStringLiteral("最大 0.00 mm")))
    {
        return fail(QStringLiteral(
            "12E-09A diagnostic failure published a false zero width bound"));
    }

    window.show();
    const QList<QSize> sizes{
        QSize(1280, 720),
        QSize(1440, 900),
        QSize(1920, 1080),
    };
    const QList<QWidget*> statusWidgets{
        subject,
        bounds,
        backend,
        status,
    };
    for (const QSize& size : sizes)
    {
        window.resize(size);
        QApplication::processEvents(
            QEventLoop::AllEvents,
            50);
        const QRect inspectorRect =
            GlobalRect(inspector);
        for (QWidget* widget : statusWidgets)
        {
            if (!widget->isVisibleTo(&window)
                || !inspectorRect.contains(
                    GlobalRect(widget)))
            {
                return fail(
                    QStringLiteral(
                        "12E-09A-03 Chinese status clipped at %1x%2")
                        .arg(size.width())
                        .arg(size.height()));
            }
        }
    }

    return pass(QStringLiteral(
        "diagnostic-settings-controls Chinese/"
        "0.01mm/bidirectional/materials/"
        "unavailable/three-sizes"));
}
