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

int UiSmokeTestRunner::TiffNativePreviewNoPng(
    const UiSmokeTestOptions& options)
{
    const QString packagePath =
        absoluteFromRepo(options, options.package_path);
    const QDir packageDirectory(packagePath);
    QFile manifestFile(packageDirectory.filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return fail(
            QStringLiteral("tiff-native-preview-no-png 未找到 manifest：")
            + packagePath);
    }

    QJsonParseError parseError;
    const QJsonDocument manifestDocument =
        QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    const QJsonObject preview =
        manifestDocument.object().value(QStringLiteral("preview")).toObject();
    if (parseError.error != QJsonParseError::NoError
        || preview.value(QStringLiteral("outputPolicy")).toString()
            != QStringLiteral("tiff_native")
        || preview.value(QStringLiteral("productionSource")).toString()
            != QStringLiteral("rgbwsv_tiff")
        || preview.value(QStringLiteral("automaticDiagnosticImages")).toBool(true)
        || !preview.value(QStringLiteral("files")).toArray().isEmpty())
    {
        return fail(
            QStringLiteral("tiff-native-preview-no-png manifest 预览合同不完整。"));
    }
    if (packageDirectory.exists(QStringLiteral("preview")))
    {
        return fail(
            QStringLiteral("tiff-native-preview-no-png 不应生成 preview 目录。"));
    }

    const int previewResult = TiffNativePreviewAllMaterials(options);
    if (previewResult != 0)
    {
        return previewResult;
    }
    return pass(
        QStringLiteral(
            "tiff-native-preview-no-png policy=tiff_native previewDir=absent source=RGBWSV_TIFF"));
}

int UiSmokeTestRunner::PreviewPhysicalAspect(
    const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        return fail(QStringLiteral(
            "preview-physical-aspect 无法创建临时目录。"));
    }

    const auto createPackage =
        [&temporaryDirectory](
            const QString& name,
            const bool includePhysicalScale) -> PackageSummary
    {
        const QDir root(temporaryDirectory.path());
        const QString packagePath = root.filePath(name);
        const QDir packageDirectory(packagePath);
        if (!QDir().mkpath(packageDirectory.filePath(
                QStringLiteral("preview"))))
        {
            return {};
        }

        QImage image(QSize(100, 100), QImage::Format_ARGB32);
        image.fill(QColor(32, 96, 160));
        if (!image.save(
                packageDirectory.filePath(
                    QStringLiteral("preview/rgb_layer_000000.png"))))
        {
            return {};
        }

        QJsonObject grid{
            {QStringLiteral("widthPx"), 100},
            {QStringLiteral("heightPx"), 100},
            {QStringLiteral("layerCount"), 1},
            {QStringLiteral("layerThicknessMm"), 0.01},
        };
        if (includePhysicalScale)
        {
            grid.insert(QStringLiteral("dpiX"), 635);
            grid.insert(QStringLiteral("dpiY"), 600);
            grid.insert(
                QStringLiteral("pixelSizeXmm"),
                25.4 / 635.0);
            grid.insert(
                QStringLiteral("pixelSizeYmm"),
                25.4 / 600.0);
        }

        const QJsonObject manifest{
            {QStringLiteral("schema"),
             QStringLiteral("p0.rgbwsv.2")},
            {QStringLiteral("grid"), grid},
            {QStringLiteral("layers"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("index"), 0},
                     {QStringLiteral("zMm"), 0.0}}}},
        };
        if (!WriteJsonFixture(
                packageDirectory.filePath(
                    QStringLiteral("manifest.json")),
                manifest))
        {
            return {};
        }
        return PackageLoader().load(packagePath);
    };

    const PackageSummary physicalPackage =
        createPackage(QStringLiteral("physical"), true);
    if (physicalPackage.manifest_path.isEmpty())
    {
        return fail(QStringLiteral(
            "preview-physical-aspect 无法创建非等方夹具。"));
    }

    LayerPreviewPanel layerPanel;
    layerPanel.LoadPackage(physicalPackage);
    if (!layerPanel.SelectChannelForTest(QStringLiteral("rgb"))
        || layerPanel.PhysicalDisplaySizeForTest() != QSize(94, 100)
        || !ContainsAll(
            layerPanel.StatusForTest(),
            {QStringLiteral("DPI=635x600"),
             QStringLiteral("像素=0.040000x0.042333 mm")}))
    {
        return fail(
            QStringLiteral(
                "preview-physical-aspect 生产层未按 635/600 校正：")
            + layerPanel.StatusForTest());
    }

    PreviewOverlayPanel overlayPanel;
    overlayPanel.loadPackage(physicalPackage);
    if (overlayPanel.PhysicalDisplaySizeForTest() != QSize(94, 100)
        || !ContainsAll(
            overlayPanel.StatusForTest(),
            {QStringLiteral("DPI=635x600"),
             QStringLiteral("像素=0.040000x0.042333 mm")}))
    {
        return fail(
            QStringLiteral(
                "preview-physical-aspect 叠加层未按 635/600 校正：")
            + overlayPanel.StatusForTest());
    }

    const PackageSummary fallbackPackage =
        createPackage(QStringLiteral("fallback"), false);
    LayerPreviewPanel fallbackLayerPanel;
    fallbackLayerPanel.LoadPackage(fallbackPackage);
    fallbackLayerPanel.SelectChannelForTest(QStringLiteral("rgb"));
    PreviewOverlayPanel fallbackOverlayPanel;
    fallbackOverlayPanel.loadPackage(fallbackPackage);
    const QString fallbackText =
        QStringLiteral("缺少 grid 物理像素元数据，按方形像素显示");
    if (fallbackLayerPanel.PhysicalDisplaySizeForTest()
            != QSize(100, 100)
        || fallbackOverlayPanel.PhysicalDisplaySizeForTest()
            != QSize(100, 100)
        || !fallbackLayerPanel.StatusForTest().contains(fallbackText)
        || !fallbackOverlayPanel.StatusForTest().contains(fallbackText))
    {
        return fail(QStringLiteral(
            "preview-physical-aspect 缺失元数据时未明确降级。"));
    }

    return pass(QStringLiteral(
        "preview-physical-aspect corrected=94x100 fallback=100x100"));
}
