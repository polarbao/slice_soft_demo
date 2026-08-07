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

int UiSmokeTestRunner::DiagnosticSemanticPreview(
    const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);

    constexpr std::size_t channelCount{6U};
    auto evidence = std::make_shared<
        slicer_core::
            TextureFillPartitionReleaseBenchmarkResult>();
    auto& partition = evidence->partition;
    partition.available = true;
    partition.partitionPass = true;
    partition.status = "diagnostic";
    partition.grid.width = 4;
    partition.grid.height = 3;
    partition.grid.depth = 1;
    partition.grid.originXMm = 1.0;
    partition.grid.originYMm = 2.0;
    partition.grid.originZMm = 3.0;
    partition.grid.spacingXMm = 0.04;
    partition.grid.spacingYMm = 0.05;
    partition.grid.spacingZMm = 0.10;
    partition.widthMetrics.effectiveWidthMm = 0.20;
    partition.widthMetrics.allTexture = false;
    partition.modelMask.grid = partition.grid;
    partition.textureSurfaceMask.grid = partition.grid;
    partition.modelFillMask.grid = partition.grid;
    partition.modelMask.values.assign(12U, 0U);
    partition.textureSurfaceMask.values.assign(12U, 0U);
    partition.modelFillMask.values.assign(12U, 0U);
    partition.modelMask.values.at(5U) = 1U;
    partition.textureSurfaceMask.values.at(5U) = 1U;
    partition.modelMask.values.at(6U) = 1U;
    partition.modelFillMask.values.at(6U) = 1U;

    DiagnosticAnalysisResult analysis;
    analysis.state = DiagnosticAnalysisState::Succeeded;
    analysis.identity.sessionid =
        QStringLiteral("diagnostic-session");
    analysis.identity.sceneid =
        QStringLiteral("scene-semantic-smoke");
    analysis.identity.modelid =
        QStringLiteral("model-a");
    analysis.identity.instanceid =
        QStringLiteral("instance-a");
    analysis.identity.confighash =
        QStringLiteral("0123456789abcdef");
    analysis.identity.scenerevision = 7U;
    analysis.identity.transformrevision = 2U;
    analysis.evidence = evidence;

    auto layer =
        std::make_shared<slicer_core::RgbwsvLayerBuffer>();
    layer->sourceIdentity = "semantic-smoke-layer";
    layer->layerIndex = 37;
    layer->zMm = 3.05;
    layer->width = 4U;
    layer->height = 3U;
    layer->dpiX = 635;
    layer->dpiY = 508;
    layer->originxmm = 1.0;
    layer->originymm = 2.0;
    layer->originzmm = 3.0;
    layer->pixelsizexmm = 0.04;
    layer->pixelsizeymm = 0.05;
    layer->layerthicknessmm = 0.10;
    layer->sceneidentityavailable = true;
    layer->sceneid = "scene-semantic-smoke";
    layer->scenerevision = 7U;
    layer->pixels.assign(
        4U * 3U * channelCount,
        255U);
    layer->pixels.at(10U * channelCount + 3U) = 0U;
    layer->pixels.at(4U) = 0U;
    layer->pixels.at(11U * channelCount + 5U) =
        127U;

    MaterialClosureDiagnosticsSummary closureSummary;
    closureSummary.reportavailable = true;
    closureSummary.schemavalid = true;
    closureSummary.confidence = QStringLiteral("exact");
    closureSummary.closurestatus = QStringLiteral("pass");
    closureSummary.productionacceptance = QStringLiteral("passed");
    MaterialClosureLayerUi closureLayer;
    closureLayer.layerindex = 37;
    closureLayer.zmm = 3.05;
    closureLayer.closurestatus = QStringLiteral("pass");
    closureSummary.layers.push_back(closureLayer);

    DiagnosticSemanticPreviewPanel panel;
    panel.resize(640, 480);
    panel.SetMaterialClosureSummary(closureSummary);
    panel.SetDiagnosticAnalysis(analysis);
    panel.SetProductionLayer(layer);
    QApplication::processEvents();

    if (panel.CurrentImageForTest().isNull()
        || panel.LayerIndexForTest() != 37
        || !ContainsAll(
            panel.StatusForTest(),
            {QStringLiteral("同层 layer=37"),
             QStringLiteral("z=3.050 mm"),
             QStringLiteral("Texture=1"),
             QStringLiteral("Fill=1"),
             QStringLiteral("W=1"),
             QStringLiteral("S=1"),
             QStringLiteral("V=1"),
             QStringLiteral("width=0.20 mm"),
             QStringLiteral("scene 身份已匹配"),
             QStringLiteral("材料闭环=通过（gap=0）")}))
    {
        return fail(
            QStringLiteral(
                "diagnostic-semantic-preview 同层状态或图像不完整：")
            + panel.StatusForTest());
    }

    if (!panel.SetDisplayModeForTest(
            DiagnosticSemanticDisplayMode::
                TextureSurface)
        || panel.CurrentImageForTest().pixelColor(1, 1)
            != QColor(0, 151, 167)
        || !panel.SetDisplayModeForTest(
            DiagnosticSemanticDisplayMode::ModelFill)
        || panel.CurrentImageForTest().pixelColor(2, 1)
            != QColor(230, 126, 34))
    {
        return fail(
            QStringLiteral(
                "diagnostic-semantic-preview Texture/Fill 伪彩分区不正确。"));
    }

    auto staleLayer =
        std::make_shared<slicer_core::RgbwsvLayerBuffer>(
            *layer);
    staleLayer->scenerevision = 8U;
    panel.SetProductionLayer(staleLayer);
    if (!panel.CurrentImageForTest().isNull()
        || !panel.StatusForTest().contains(
            QStringLiteral("sceneId/revision 与当前诊断身份不一致")))
    {
        return fail(
            QStringLiteral(
                "diagnostic-semantic-preview 未拒绝 stale scene 身份。"));
    }

    DiagnosticAnalysisResult missing;
    missing.state = DiagnosticAnalysisState::Failed;
    panel.SetDiagnosticAnalysis(missing);
    if (!panel.CurrentImageForTest().isNull()
        || !panel.StatusForTest().contains(
            QStringLiteral("未评估")))
    {
        return fail(
            QStringLiteral(
                "diagnostic-semantic-preview 缺证据状态不明确。"));
    }

    return pass(
        QStringLiteral(
            "diagnostic-semantic-preview layer=37 Texture=1 Fill=1 W=1 S=1 V=1 stale=blocked"));
}

int UiSmokeTestRunner::PreviewLegendProbeContext(const UiSmokeTestOptions& options)
{
    const QString packagePath = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(packagePath);
    if (package.manifest_path.isEmpty())
    {
        return fail(QStringLiteral("preview-legend-probe-context 未找到 manifest：") + packagePath);
    }

    PreviewWorkspace workspace;
    workspace.LoadPackage(package);
    auto* production = workspace.findChild<LayerPreviewPanel*>(QStringLiteral("productionLayerView"));
    auto* legendBar = workspace.findChild<QWidget*>(QStringLiteral("previewLegendBar"));
    auto* protocolHint = workspace.findChild<QLabel*>(QStringLiteral("previewProtocolHint"));
    auto* probeContext = workspace.findChild<QLabel*>(QStringLiteral("previewProbeContext"));
    if (production == nullptr || legendBar == nullptr || protocolHint == nullptr || probeContext == nullptr)
    {
        return fail(QStringLiteral("preview-legend-probe-context 缺少图例或探针上下文控件。"));
    }

    const QString legend = workspace.LegendTextForTest();
    if (!ContainsAll(
            legend,
            {QStringLiteral("RGB 模型颜色/填充"), QStringLiteral("W 白墨模型填充"),
             QStringLiteral("S 模型外支撑或内部镂空支撑"), QStringLiteral("V 光油或光油模型填充"),
             QStringLiteral("真实空白无材料"), QStringLiteral("black_is_print"),
             QStringLiteral("0=打印"), QStringLiteral("255=不打印"),
             QStringLiteral("显示颜色不等于生产值")})
        || !ContainsAll(
            protocolHint->text(),
            {QStringLiteral("RGBWSV uint8"), QStringLiteral("black_is_print"),
             QStringLiteral("0=打印"), QStringLiteral("255=不打印"),
             QStringLiteral("不等于 TIFF 生产值")}))
    {
        return fail(QStringLiteral("preview-legend-probe-context 图例或生产/显示值说明不完整。"));
    }

    const auto probeSemantic = [production](
                                   const QString& channel,
                                   const QString& expectedSemantic) -> QString
    {
        for (const int layerIndex : production->LayerIndices())
        {
            production->SelectLayer(layerIndex);
            if (!WaitForCondition(
                    [production, layerIndex]()
                    {
                        return production->IsLayerReadyForTest()
                            && production
                                   ->LoadedLayerIndexForTest()
                                == layerIndex;
                    }))
            {
                return {};
            }
            if (!production->SelectChannelForTest(channel))
            {
                return {};
            }
            const QImage image = production->CurrentImageForTest();
            for (int y = 0; y < image.height(); ++y)
            {
                for (int x = 0; x < image.width(); ++x)
                {
                    const QColor color = image.pixelColor(x, y);
                    if (color.alpha() == 0
                        || (color.red() > 245 && color.green() > 245 && color.blue() > 245))
                    {
                        continue;
                    }
                    const QString context = production->ProbePixelForTest(x, y);
                    if (context.contains(expectedSemantic))
                    {
                        return context;
                    }
                }
            }
        }
        return {};
    };

    const QString rgbProbe = probeSemantic(QStringLiteral("rgb"), QStringLiteral("RGB模型颜色或填充"));
    const QString whiteProbe = probeSemantic(QStringLiteral("white"), QStringLiteral("白墨模型填充"));
    const QString supportProbe = probeSemantic(QStringLiteral("support"), QStringLiteral("支撑填充"));
    const QString varnishProbe = probeSemantic(QStringLiteral("varnish"), QStringLiteral("光油表面或外侧层"));
    if (rgbProbe.isEmpty() || whiteProbe.isEmpty() || supportProbe.isEmpty() || varnishProbe.isEmpty())
    {
        return fail(
            QStringLiteral("preview-legend-probe-context 未在真实 RGBWSV 夹具中识别完整 RGB/W/S/V 语义。"));
    }

    QString emptyProbe;
    production->SelectLayer(production->LayerIndices().first());
    if (!WaitForCondition(
            [production]()
            {
                return production->IsLayerReadyForTest()
                    && production->LoadedLayerIndexForTest()
                        == production->LayerIndices().first();
            }))
    {
        return fail(
            QStringLiteral(
                "preview-legend-probe-context 首层 TIFF 异步读取超时。"));
    }
    production->SelectChannelForTest(QStringLiteral("support"));
    const QImage supportImage = production->CurrentImageForTest();
    for (int y = 0; y < supportImage.height() && emptyProbe.isEmpty(); ++y)
    {
        for (int x = 0; x < supportImage.width(); ++x)
        {
            const QColor color = supportImage.pixelColor(x, y);
            if (color.red() <= 245 || color.green() <= 245 || color.blue() <= 245)
            {
                continue;
            }
            const QString context = production->ProbePixelForTest(x, y);
            if (context.contains(QStringLiteral("材料语义=真实空白")))
            {
                emptyProbe = context;
                break;
            }
        }
    }
    if (emptyProbe.isEmpty())
    {
        return fail(QStringLiteral("preview-legend-probe-context 未在真实 RGBWSV 夹具中识别真实空白。"));
    }

    if (!ContainsAll(
            workspace.ProbeContextForTest(),
            {QStringLiteral("生产值 RGBWSV="), QStringLiteral("打印通道=无"),
             QStringLiteral("材料语义=真实空白"), QStringLiteral("black_is_print"),
             QStringLiteral("显示颜色=伪彩或真彩预览")})
        || workspace.ProbeContextForTest() != probeContext->text())
    {
        return fail(QStringLiteral("preview-legend-probe-context 六通道探针未同步到统一工作区。"));
    }

    return pass(
        QStringLiteral("preview-legend-probe-context legend=RGBWSV probes=RGB,W,S,V,Empty layers=%1")
            .arg(production->LayerIndices().size()));
}

int UiSmokeTestRunner::DiagnosticsCollapse(const UiSmokeTestOptions& options)
{
    const QString packagePath = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(packagePath);
    if (package.manifest_path.isEmpty())
    {
        return fail(QStringLiteral("diagnostics-collapse 未找到 manifest：") + packagePath);
    }

    MainWindow window(options.repo_root);
    auto* dock = window.findChild<DiagnosticsDock*>(QStringLiteral("diagnosticsDock"));
    auto* diagnosticsTabs = window.findChild<QTabWidget*>(QStringLiteral("diagnosticsTabs"));
    auto* workspaceTabs = window.findChild<QTabWidget*>(QStringLiteral("mainWorkspaceTabs"));
    auto* diagnosticsAction = window.findChild<QAction*>(QStringLiteral("diagnosticsToggleAction"));
    auto* preview = window.findChild<PreviewWorkspace*>(QStringLiteral("previewWorkspace"));
    if (dock == nullptr || diagnosticsTabs == nullptr || workspaceTabs == nullptr
        || diagnosticsAction == nullptr || preview == nullptr)
    {
        return fail(QStringLiteral("diagnostics-collapse 缺少 dock、菜单入口或中央工作区。"));
    }

    QStringList workspaceTitles;
    for (int index = 0; index < workspaceTabs->count(); ++index)
    {
        workspaceTitles.push_back(workspaceTabs->tabText(index));
    }
    if (workspaceTitles
        != QStringList{
            QStringLiteral("模型"),
            QStringLiteral("预览"),
            QStringLiteral("配置")})
    {
        return fail(QStringLiteral("diagnostics-collapse 中央页签仍包含历史报告/曲线入口：")
                    + workspaceTitles.join(QStringLiteral(",")));
    }
    if (dock->TabTitles()
        != QStringList{
            QStringLiteral("报告"),
            QStringLiteral("材料闭环"),
            QStringLiteral("曲线"),
            QStringLiteral("材料参数"),
            QStringLiteral("工艺对比"),
            QStringLiteral("切片耗时"),
            QStringLiteral("日志")})
    {
        return fail(QStringLiteral("diagnostics-collapse 诊断页签集合不正确。"));
    }
    if (window.findChildren<ReportPanel*>().size() != 1
        || window.findChildren<MaterialClosurePanel*>().size() != 1
        || window.findChildren<ChannelChartPanel*>().size() != 1
        || window.findChildren<LogPanel*>().size() != 1)
    {
        return fail(QStringLiteral("diagnostics-collapse 存在重复诊断 panel 实例。"));
    }
    if (dock->IsExpanded() || !dock->isHidden() || diagnosticsAction->isChecked())
    {
        return fail(QStringLiteral("diagnostics-collapse 诊断区域未默认折叠。"));
    }

    dock->LoadPackage(package);
    preview->LoadPackage(package);
    if (dock->ChartView()->layerStatCount() <= 0 || preview->LayerIndices().isEmpty())
    {
        return fail(QStringLiteral("diagnostics-collapse 输出包未加载到曲线或预览。"));
    }
    const int layerIndex = preview->LayerIndices().last();
    preview->SelectLayer(layerIndex);

    dock->SetExpanded(true);
    QApplication::processEvents();
    if (!dock->IsExpanded() || dock->isHidden())
    {
        return fail(QStringLiteral("diagnostics-collapse 无法展开诊断区域。"));
    }
    dock->SetExpanded(false);
    QApplication::processEvents();
    if (dock->IsExpanded() || !dock->isHidden())
    {
        return fail(QStringLiteral("diagnostics-collapse 无法收起诊断区域。"));
    }
    if (preview->CurrentLayerIndex() != layerIndex)
    {
        return fail(QStringLiteral("diagnostics-collapse 折叠操作改变了预览真实 layerIndex。"));
    }

    return pass(
        QStringLiteral("diagnostics-collapse default=collapsed tabs=%1 workspace=%2 layer=%3")
            .arg(dock->TabTitles().join(QStringLiteral(",")))
            .arg(workspaceTitles.join(QStringLiteral(",")))
            .arg(layerIndex));
}
