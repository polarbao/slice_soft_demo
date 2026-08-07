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

int UiSmokeTestRunner::PreviewWorkspaceSharedLayer(const UiSmokeTestOptions& options)
{
    const QString packagePath = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(packagePath);
    if (package.manifest_path.isEmpty())
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 未找到 manifest：") + packagePath);
    }

    PreviewWorkspace workspace;
    workspace.LoadPackage(package);
    auto* production = workspace.findChild<LayerPreviewPanel*>(QStringLiteral("productionLayerView"));
    auto* semantic = workspace.findChild<DiagnosticSemanticPreviewPanel*>(
        QStringLiteral("diagnosticSemanticPreviewPanel"));
    auto* overlay = workspace.findChild<PreviewOverlayPanel*>(QStringLiteral("materialOverlayView"));
    auto* raw = workspace.findChild<PreviewPanel*>(QStringLiteral("rawPreviewView"));
    if (production == nullptr || semantic == nullptr || overlay == nullptr || raw == nullptr)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 缺少生产或诊断预览面板。"));
    }
    if (workspace.LayerIndices().isEmpty()
        || workspace.LayerIndices() != production->LayerIndices())
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 未优先使用生产真实层范围。"));
    }

    const QVector<int> previewLayers = overlay->LayerIndices();
    if (previewLayers.isEmpty())
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 夹具缺少 overlay preview 层。"));
    }
    const int previewLayer = previewLayers.first();
    if (!workspace.SelectLayer(previewLayer)
        || production->CurrentLayerIndex() != previewLayer
        || overlay->CurrentLayerIndex() != previewLayer
        || raw->CurrentLayerIndex() != previewLayer)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 有图层未同步到三个模式。"));
    }

    workspace.SetMode(PreviewWorkspaceMode::Diagnostic);
    workspace.SetDiagnosticMode(
        DiagnosticPreviewMode::MaterialOverlay);
    workspace.SetDiagnosticMode(
        DiagnosticPreviewMode::RawPreview);
    workspace.SetMode(PreviewWorkspaceMode::Production);
    if (workspace.CurrentLayerIndex() != previewLayer
        || production->CurrentLayerIndex() != previewLayer
        || overlay->CurrentLayerIndex() != previewLayer
        || raw->CurrentLayerIndex() != previewLayer)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 模式切换改变了真实层号。"));
    }

    QComboBox* rawChannel = raw->findChild<QComboBox*>(QStringLiteral("rawPreviewChannelSelector"));
    QComboBox* overlayMode = overlay->findChild<QComboBox*>(QStringLiteral("overlayModeSelector"));
    QComboBox* workspaceMode =
        workspace.findChild<QComboBox*>(
            QStringLiteral(
                "previewWorkspaceModeSelector"));
    QComboBox* diagnosticMode =
        workspace.findChild<QComboBox*>(
            QStringLiteral(
                "diagnosticPreviewModeSelector"));
    if (rawChannel == nullptr
        || overlayMode == nullptr
        || workspaceMode == nullptr
        || diagnosticMode == nullptr
        || workspaceMode->count() != 2
        || diagnosticMode->count() != 3
        || diagnosticMode->findData(
               static_cast<int>(DiagnosticPreviewMode::TextureFillSemantics))
            < 0)
    {
        return fail(
            QStringLiteral(
                "preview-workspace-shared-layer 未收敛为生产/诊断两个一级入口，或缺少诊断子入口。"));
    }
    rawChannel->setCurrentText(QStringLiteral("RGB"));
    overlayMode->setCurrentText(QStringLiteral("RGB + W 白墨"));

    int sparseLayer = -1;
    for (const int layerIndex : workspace.LayerIndices())
    {
        if (!raw->LayerIndices().contains(layerIndex))
        {
            sparseLayer = layerIndex;
            break;
        }
    }
    if (sparseLayer < 0 || !workspace.SelectLayer(sparseLayer))
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 夹具未覆盖稀疏 preview 缺层。"));
    }
    if (production->CurrentLayerIndex() != sparseLayer
        || overlay->CurrentLayerIndex() != sparseLayer
        || raw->CurrentLayerIndex() != sparseLayer)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 缺图层没有保持共享 layerIndex。"));
    }
    if (!ContainsAll(
            raw->StatusForTest(),
            {QStringLiteral("layer=%1").arg(sparseLayer), QStringLiteral("未跨层兜底")})
        || !ContainsAll(
            workspace.StatusForTest(),
            {QStringLiteral("共享 layer=%1").arg(sparseLayer), QStringLiteral("缺图不跨层兜底")}))
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 缺图状态未明确禁止跨层兜底。"));
    }

    int overlaySparseLayer = -1;
    for (const int layerIndex : workspace.LayerIndices())
    {
        workspace.SelectLayer(layerIndex);
        if (overlay->StatusForTest().contains(QStringLiteral("未跨层兜底")))
        {
            overlaySparseLayer = layerIndex;
            break;
        }
    }
    if (overlaySparseLayer < 0
        || !ContainsAll(
            overlay->StatusForTest(),
            {QStringLiteral("layer=%1").arg(overlaySparseLayer), QStringLiteral("未跨层兜底")}))
    {
        return fail(QStringLiteral("preview-workspace-shared-layer overlay 未覆盖同层材料缺失。"));
    }

    const int panelDrivenLayer = previewLayers.last();
    production->SelectLayer(panelDrivenLayer);
    if (workspace.CurrentLayerIndex() != panelDrivenLayer
        || overlay->CurrentLayerIndex() != panelDrivenLayer
        || raw->CurrentLayerIndex() != panelDrivenLayer)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer panel 信号未回写共享状态。"));
    }

    return pass(
        QStringLiteral("preview-workspace-shared-layer layers=%1 rawSparse=%2 overlaySparse=%3 preview=%4 primaryModes=2 diagnosticModes=3")
            .arg(workspace.LayerIndices().size())
            .arg(sparseLayer)
            .arg(overlaySparseLayer)
            .arg(previewLayer));
}

int UiSmokeTestRunner::TiffNativePreviewAllMaterials(
    const UiSmokeTestOptions& options)
{
    const QString packagePath =
        absoluteFromRepo(
            options,
            options.package_path);
    PackageSummary package =
        PackageLoader().load(packagePath);
    if (package.manifest_path.isEmpty())
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials 未找到 manifest：")
            + packagePath);
    }

    // Production preview must remain usable even when its package summary
    // does not expose any preview PNG paths.
    package.preview_paths.clear();

    PreviewWorkspace workspace;
    workspace.LoadPackage(package);
    auto* production =
        workspace.findChild<LayerPreviewPanel*>(
            QStringLiteral("productionLayerView"));
    auto* workspaceMode =
        workspace.findChild<QComboBox*>(
            QStringLiteral(
                "previewWorkspaceModeSelector"));
    auto* diagnosticMode =
        workspace.findChild<QComboBox*>(
            QStringLiteral(
                "diagnosticPreviewModeSelector"));
    auto* overlay =
        workspace.findChild<PreviewOverlayPanel*>(
            QStringLiteral("materialOverlayView"));
    auto* semantic =
        workspace.findChild<DiagnosticSemanticPreviewPanel*>(
            QStringLiteral("diagnosticSemanticPreviewPanel"));
    auto* raw =
        workspace.findChild<PreviewPanel*>(
            QStringLiteral("rawPreviewView"));
    if (production == nullptr
        || workspaceMode == nullptr
        || diagnosticMode == nullptr
        || overlay == nullptr
        || semantic == nullptr
        || raw == nullptr
        || workspaceMode->count() != 2
        || diagnosticMode->count() != 3)
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials 统一生产/诊断入口不完整。"));
    }

    const QVector<int> layerIndices =
        production->LayerIndices();
    if (layerIndices.size() < 3
        || production->DataSourceForTest()
            != QStringLiteral(
                "manifest/layers RGBWSV TIFF"))
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials 未使用 manifest 权威 TIFF 层源。"));
    }

    const QStringList requiredModes{
        QStringLiteral("rgb"),
        QStringLiteral("red"),
        QStringLiteral("green"),
        QStringLiteral("blue"),
        QStringLiteral("white"),
        QStringLiteral("support"),
        QStringLiteral("varnish"),
        QStringLiteral("rgb_white"),
        QStringLiteral("rgb_support"),
        QStringLiteral("rgb_varnish"),
        QStringLiteral(
            "rgb_support_white_varnish"),
        QStringLiteral("occupancy"),
        QStringLiteral("empty"),
    };
    for (const QString& mode : requiredModes)
    {
        if (!production->AvailableChannels().contains(
                mode))
        {
            return fail(
                QStringLiteral(
                    "tiff-native-preview-all-materials 缺少模式：")
                + mode);
        }
    }

    const QList<int> requestedLayers{
        layerIndices.first(),
        layerIndices.at(layerIndices.size() / 2),
        layerIndices.last(),
    };
    for (const int layerIndex : requestedLayers)
    {
        if (!workspace.SelectLayer(layerIndex)
            || !WaitForCondition(
                [production, layerIndex]()
                {
                    return production
                               ->IsLayerReadyForTest()
                        && production
                               ->LoadedLayerIndexForTest()
                            == layerIndex;
                }))
        {
            return fail(
                QStringLiteral(
                    "tiff-native-preview-all-materials 异步切层失败：layer=%1 status=%2")
                    .arg(layerIndex)
                    .arg(production->StatusForTest()));
        }
        if (!ContainsAll(
                production->StatusForTest(),
                {
                    QStringLiteral("layer=%1")
                        .arg(layerIndex),
                    QStringLiteral("z="),
                    QStringLiteral("DPI="),
                    QStringLiteral(
                        "层序=低Z->高Z"),
                    QStringLiteral(
                        "数据源=manifest/layers TIFF"),
                    QStringLiteral("cache="),
                }))
        {
            return fail(
                QStringLiteral(
                    "tiff-native-preview-all-materials 层元数据状态不完整：")
                + production->StatusForTest());
        }
    }

    const quint64 requestsBeforeModes =
        production->LayerRequestCountForTest();
    for (const QString& mode : requiredModes)
    {
        if (!production->SelectChannelForTest(mode)
            || production->CurrentImageForTest().isNull())
        {
            return fail(
                QStringLiteral(
                    "tiff-native-preview-all-materials 合成失败：mode=")
                + mode);
        }
    }
    if (production->LayerRequestCountForTest()
        != requestsBeforeModes)
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials 模式切换重复读取了 TIFF。"));
    }

    const QImage image =
        production->CurrentImageForTest();
    const QString probe =
        production->ProbePixelForTest(
            image.width() / 2,
            image.height() / 2);
    if (!ContainsAll(
            probe,
            {
                QStringLiteral("display=("),
                QStringLiteral("raw=("),
                QStringLiteral("生产值 RGBWSV=("),
                QStringLiteral(
                    "black_is_print/0打印/255不打印"),
            }))
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials 六通道探针不完整：")
            + probe);
    }

    const int rapidFirst = layerIndices.at(1);
    const int rapidMiddle =
        layerIndices.at(layerIndices.size() / 3);
    const int rapidLast =
        layerIndices.at(layerIndices.size() - 2);
    production->SelectLayer(rapidFirst);
    production->SelectLayer(rapidMiddle);
    production->SelectLayer(rapidLast);
    if (!WaitForCondition(
            [production, rapidLast]()
            {
                return production->IsLayerReadyForTest()
                    && production
                           ->LoadedLayerIndexForTest()
                        == rapidLast;
            })
        || production->CurrentLayerIndex()
            != rapidLast)
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials stale generation 覆盖了最新层：")
            + production->StatusForTest());
    }

    workspace.SetMode(
        PreviewWorkspaceMode::Diagnostic);
    workspace.SetDiagnosticMode(
        DiagnosticPreviewMode::MaterialOverlay);
    workspace.SetDiagnosticMode(
        DiagnosticPreviewMode::RawPreview);
    workspace.SetMode(
        PreviewWorkspaceMode::Production);
    if (workspace.CurrentLayerIndex() != rapidLast
        || workspace.CurrentMode()
            != PreviewWorkspaceMode::Production)
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials 模式切换改变了真实层。"));
    }

    return pass(
        QStringLiteral(
            "tiff-native-preview-all-materials layers=%1 modes=%2 requests=%3 source=TIFF primaryModes=2 diagnosticModes=3")
            .arg(layerIndices.size())
            .arg(requiredModes.size())
            .arg(
                production
                    ->LayerRequestCountForTest()));
}
