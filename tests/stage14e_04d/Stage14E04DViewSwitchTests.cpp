#include "ModuleClient.h"
#include "SceneInteractionController.h"
#include "TopViewCanvasWidget.h"
#include "ViewWorkspaceWidget.h"
#include "camera/CameraController.h"
#include "camera/ViewModeSwitch.h"
#include "render/CpuRasterBackend.h"
#include "render/SceneRenderPolicy.h"
#include "render/TopViewRenderPolicy.h"
#include "settings/ViewPresentationSettings.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <QStackedWidget>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cstdlib>

namespace
{
void Require(const bool condition, const QString& message)
{
    if (!condition)
    {
        QTextStream(stderr) << "14E-04d FAIL: " << message << Qt::endl;
        std::exit(1);
    }
}

QString ArgumentValue(const QStringList& arguments, const QString& name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments.at(index + 1) : QString{};
}

QJsonObject Execute(ModuleClient& client, const QJsonObject& request)
{
    QByteArray bytes;
    QString error;
    Require(client.Execute(
        QJsonDocument(request).toJson(QJsonDocument::Compact),
        &bytes, &error), error);
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    Require(document.isObject(), QStringLiteral("result is not JSON"));
    return document.object();
}

QJsonObject Transform()
{
    return {{QStringLiteral("translateXMm"), 0.0},
            {QStringLiteral("translateYMm"), 0.0},
            {QStringLiteral("rotateZDeg"), 0.0},
            {QStringLiteral("uniformScale"), 1.0},
            {QStringLiteral("mirrorX"), false},
            {QStringLiteral("mirrorY"), false}};
}

QJsonObject Bounds()
{
    return {
        {QStringLiteral("min"), QJsonObject{
             {QStringLiteral("x"), 0.0}, {QStringLiteral("y"), 0.0},
             {QStringLiteral("z"), 0.05}}},
        {QStringLiteral("max"), QJsonObject{
             {QStringLiteral("x"), 2.0}, {QStringLiteral("y"), 1.0},
             {QStringLiteral("z"), 0.25}}}};
}

QJsonObject BuildScene(const QString& fixturePath, const QString& suffix)
{
    const QJsonObject transform = Transform();
    const QJsonObject bounds = Bounds();
    return {
        {QStringLiteral("schema"),
         QStringLiteral("slicesoft.multimodel_scene.13b.1")},
        {QStringLiteral("subjectType"), QStringLiteral("scene")},
        {QStringLiteral("sceneId"), QStringLiteral("stage14e04d-") + suffix},
        {QStringLiteral("sceneRevision"), 1},
        {QStringLiteral("buildVolume"), QJsonObject{
             {QStringLiteral("source"), QStringLiteral("device_profile")},
             {QStringLiteral("widthMm"), 300.0},
             {QStringLiteral("heightMm"), 100.0},
             {QStringLiteral("zLimitMm"), 60.0},
             {QStringLiteral("origin"), QStringLiteral("lower_left")},
             {QStringLiteral("xDirection"), QStringLiteral("positive")},
             {QStringLiteral("yDirection"), QStringLiteral("positive")},
             {QStringLiteral("isFixture"), false}}},
        {QStringLiteral("layout"), QJsonObject{
             {QStringLiteral("policy"), QStringLiteral("grid")},
             {QStringLiteral("maxColumns"), 1},
             {QStringLiteral("maxRows"), 1},
             {QStringLiteral("columnGapMm"), 10.0},
             {QStringLiteral("rowGapMm"), 10.0},
             {QStringLiteral("spacingMode"), QStringLiteral("edge_clearance")},
             {QStringLiteral("order"), QStringLiteral("row_major")}}},
        {QStringLiteral("materialBindingMode"),
         QStringLiteral("scene_profile_only")},
        {QStringLiteral("resolvedProfileId"),
         QStringLiteral("stage14e04d-profile")},
        {QStringLiteral("resourceScopes"), QJsonArray{QJsonObject{
             {QStringLiteral("resourceScopeId"), QStringLiteral("scope-") + suffix},
             {QStringLiteral("kind"), QStringLiteral("obj_directory")},
             {QStringLiteral("rootPath"), QFileInfo(fixturePath).absolutePath()},
             {QStringLiteral("packagePath"), QString()},
             {QStringLiteral("partIdentity"), QString()}}}},
        {QStringLiteral("models"), QJsonArray{QJsonObject{
             {QStringLiteral("modelId"), QStringLiteral("model-") + suffix},
             {QStringLiteral("sourcePath"), fixturePath},
             {QStringLiteral("format"), QStringLiteral("obj")},
             {QStringLiteral("resourceScopeId"), QStringLiteral("scope-") + suffix},
             {QStringLiteral("sourceHash"), QStringLiteral("source-") + suffix},
             {QStringLiteral("resourceHash"), QStringLiteral("resource-") + suffix},
             {QStringLiteral("displayName"), QStringLiteral("Stage 14E-04d")}}}},
        {QStringLiteral("instances"), QJsonArray{QJsonObject{
             {QStringLiteral("instanceId"), QStringLiteral("instance-") + suffix},
             {QStringLiteral("modelId"), QStringLiteral("model-") + suffix},
             {QStringLiteral("sourceTransformIdentity"),
              QStringLiteral("transform-") + suffix},
             {QStringLiteral("requestedTransform"), transform},
             {QStringLiteral("derivedLayoutTransform"), transform},
             {QStringLiteral("effectiveTransform"), transform},
             {QStringLiteral("visible"), true},
             {QStringLiteral("locked"), false},
             {QStringLiteral("transformRevision"), 0},
             {QStringLiteral("sourceBboxMm"), bounds},
             {QStringLiteral("effectiveBboxMm"), bounds},
             {QStringLiteral("admissionStatus"), QStringLiteral("admitted")},
             {QStringLiteral("resolvedProfileId"),
              QStringLiteral("stage14e04d-profile")}}}}};
}

void Import(ModuleClient& client, const QString& path)
{
    const QJsonObject result = Execute(client, {
        {QStringLiteral("capability"), QStringLiteral("model.import")},
        {QStringLiteral("modelPath"), path},
        {QStringLiteral("options"), QJsonObject{
             {QStringLiteral("computeBBox"), true},
             {QStringLiteral("extractMaterials"), true}}}});
    Require(result.value(QStringLiteral("ok")).toBool(),
            QStringLiteral("fixture import failed"));
}

int CountColorful(const QImage& image)
{
    int count{0};
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QRgb pixel = image.pixel(x, y);
            const int minimum = (std::min)({qRed(pixel), qGreen(pixel), qBlue(pixel)});
            const int maximum = (std::max)({qRed(pixel), qGreen(pixel), qBlue(pixel)});
            count += maximum - minimum > 35 ? 1 : 0;
        }
    }
    return count;
}

int CountColorful(const slicer::render::ImageOut& image)
{
    int count{0};
    for (std::size_t offset = 0; offset + 3 < image.rgba8.size(); offset += 4)
    {
        const auto red = image.rgba8[offset];
        const auto green = image.rgba8[offset + 1];
        const auto blue = image.rgba8[offset + 2];
        count += (std::max)({red, green, blue})
                - (std::min)({red, green, blue}) > 35 ? 1 : 0;
    }
    return count;
}

void VerifySettingsRoundTrip()
{
    QTemporaryDir directory;
    Require(directory.isValid(), QStringLiteral("temporary config root failed"));
    const QString path = directory.filePath(QStringLiteral("session.json"));
    ViewPresentationSettings first(path);
    QString error;
    Require(first.Load(&error), error);
    Require(first.DefaultViewMode() == HostViewMode::Top,
            QStringLiteral("contract default is not top"));
    first.SetDefaultViewMode(HostViewMode::ThreeD);
    first.SetThreeDProjection(slicer::render::Projection::Perspective);
    Require(first.Save(&error), error);
    ViewPresentationSettings restored(path);
    Require(restored.Load(&error), error);
    Require(restored.DefaultViewMode() == HostViewMode::ThreeD
            && restored.ThreeDProjection()
                == slicer::render::Projection::Perspective,
            QStringLiteral("UI-M13 settings round-trip failed"));
}

void VerifyWhiteContrast()
{
    TopViewFrame frame;
    frame.viewDataIdentity = QStringLiteral("white-contrast");
    frame.sceneRevision = 1U;
    frame.decor.buildWidthMm = 20.0;
    frame.decor.buildHeightMm = 10.0;
    frame.decor.coordinateFrameResolved = true;
    TopViewInstance instance;
    instance.instanceId = QStringLiteral("white");
    instance.localBoundsMm = {2.0, 2.0, 18.0, 8.0};
    instance.worldMatrix = {1.0, 0.0, 0.0, 0.0,
                            0.0, 1.0, 0.0, 0.0,
                            0.0, 0.0, 1.0, 0.0,
                            0.0, 0.0, 0.0, 1.0};
    instance.surfacePreview = QImage(16, 8, QImage::Format_RGBA8888);
    instance.surfacePreview.fill(Qt::white);
    frame.instances.push_back(instance);
    ModuleClient unusedClient;
    TopViewRenderPolicy renderer(unusedClient);
    const QImage image = renderer.Render(frame, QSize(640, 360));
    Require(!image.isNull(), QStringLiteral("white contrast render failed"));
    bool foundWhite{false};
    bool foundOutline{false};
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QRgb pixel = image.pixel(x, y);
            foundWhite = foundWhite || pixel == qRgb(255, 255, 255);
            foundOutline = foundOutline || pixel == qRgb(23, 25, 28);
        }
    }
    Require(foundWhite && foundOutline,
            QStringLiteral("white texture is not distinguishable"));
}

void VerifyWorkspaceControls()
{
    ViewWorkspaceWidget workspace;
    auto* stack = workspace.findChild<QStackedWidget*>(
        QStringLiteral("viewCanvasStack"));
    auto* topButton = workspace.findChild<QToolButton*>(
        QStringLiteral("topViewButton"));
    auto* threeDButton = workspace.findChild<QToolButton*>(
        QStringLiteral("threeDViewButton"));
    Require(stack != nullptr && topButton != nullptr && threeDButton != nullptr,
            QStringLiteral("dual-view segmented controls are missing"));
    workspace.SetMode(HostViewMode::ThreeD);
    Require(stack->currentIndex() == 1 && threeDButton->isChecked(),
            QStringLiteral("central three_d switch did not update immediately"));
    workspace.SetMode(HostViewMode::Top);
    Require(stack->currentIndex() == 0 && topButton->isChecked(),
            QStringLiteral("central top switch did not update immediately"));
}
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    VerifySettingsRoundTrip();
    VerifyWhiteContrast();
    VerifyWorkspaceControls();

    const QString modulePath = ArgumentValue(
        application.arguments(), QStringLiteral("--module"));
    Require(!modulePath.isEmpty(), QStringLiteral("--module is required"));
    const QDir fixtures(QDir(QStringLiteral(SLICESOFT_SOURCE_DIR)).filePath(
        QStringLiteral("samples/models/textured/fixtures")));
    const QString valid = fixtures.filePath(
        QStringLiteral("policy_textured_small.obj"));
    ModuleClient client;
    QString error;
    Require(client.Open(modulePath, QByteArrayLiteral("{}"), &error), error);
    Import(client, valid);
    SceneInteractionController controller(client);
    Require(controller.Initialize(BuildScene(valid, QStringLiteral("valid")), &error),
            error);

    TopViewRenderPolicy topRenderer(client);
    TopViewFrame topFrame;
    Require(topRenderer.Refresh(controller.SceneHandle(),
        controller.SceneRevision(), &topFrame, &error), error);
    Require(!topFrame.decor.coordinateFrameResolved
            && topFrame.decor.buildWidthMm == 300.0
            && topFrame.decor.buildHeightMm == 100.0,
            QStringLiteral("grid extent/fallback diagnostic contract failed"));
    const QImage topImage = topRenderer.Render(topFrame, QSize(900, 360));
    Require(CountColorful(topImage) > 0,
            QStringLiteral("UI-M9 top texture is not visible"));

    ViewWorkspaceWidget visibleWorkspace;
    visibleWorkspace.resize(960, 520);
    visibleWorkspace.SetTopImage(topImage);
    TopViewCanvasWidget* topCanvas = visibleWorkspace.TopCanvas();
    Require(topCanvas != nullptr && topCanvas->HasImage(),
            QStringLiteral("H-D-01 top canvas did not retain ViewData image"));
    client.ResetCallCount();
    topCanvas->ZoomAt(1.5, QPointF(320.0, 180.0));
    topCanvas->PanBy(QPointF(24.0, -12.0));
    Require(topCanvas->ZoomFactor() > 1.0
            && topCanvas->PanOffset() != QPointF()
            && client.CallCount() == 0U,
            QStringLiteral("H-D-01 local pan/zoom crossed DLL boundary"));
    visibleWorkspace.show();
    QApplication::processEvents();
    visibleWorkspace.hide();

    CpuRasterBackend backend;
    SceneRenderPolicy threeDRenderer(client, backend);
    ThreeDFrame threeDFrame;
    Require(threeDRenderer.Refresh(controller.SceneHandle(),
        controller.SceneRevision(), &threeDFrame, &error), error);
    CameraController camera;
    camera.Fit(threeDFrame.worldBounds, 640U, 360U);
    slicer::render::ImageOut threeDImage;
    Require(threeDRenderer.Render(
        threeDFrame, camera, 640U, 360U, &threeDImage).ok,
        QStringLiteral("three_d render failed"));
    Require(CountColorful(threeDImage) > 0,
            QStringLiteral("UI-M10 three_d texture is not visible"));

    const QString topIdentity = topFrame.viewDataIdentity;
    const QString threeDIdentity = threeDFrame.viewDataIdentity;
    const quint64 sceneRevision = controller.SceneRevision();
    const quint64 topReads = topRenderer.BlobReadCount();
    const quint64 threeDReads = threeDRenderer.BlobReadCount();
    ViewModeSwitch viewSwitch;
    client.ResetCallCount();
    for (int index = 0; index < 100; ++index)
    {
        viewSwitch.SetMode(index % 2 == 0
            ? HostViewMode::Top : HostViewMode::ThreeD);
    }
    Require(client.CallCount() == 0U
            && controller.SceneRevision() == sceneRevision
            && topFrame.viewDataIdentity == topIdentity
            && threeDFrame.viewDataIdentity == threeDIdentity
            && topRenderer.BlobReadCount() == topReads
            && threeDRenderer.BlobReadCount() == threeDReads,
            QStringLiteral("UI-M11 view switch changed authoritative state/cache"));

    const QString missing = fixtures.filePath(
        QStringLiteral("missing_texture_small.obj"));
    Import(client, missing);
    SceneInteractionController badController(client);
    Require(badController.Initialize(
        BuildScene(missing, QStringLiteral("missing")), &error), error);
    error.clear();
    TopViewFrame badFrame;
    Require(!topRenderer.Refresh(badController.SceneHandle(),
        badController.SceneRevision(), &badFrame, &error)
        && !error.isEmpty(),
        QStringLiteral("missing texture silently became a gray model"));

    QTextStream(stdout)
        << "14E-04d dual-view contract: PASS calls=" << client.CallCount()
        << " topReads=" << topReads << " threeDReads=" << threeDReads
        << Qt::endl;
    return 0;
}
