#include "ModuleClient.h"
#include "MoveOptimizationPolicy.h"
#include "SceneInteractionController.h"
#include "render/TopViewRenderPolicy.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <cstdlib>

namespace
{
constexpr auto kInstanceId = "stage14e04-instance";

void Require(bool condition, const QString& message)
{
    if (!condition)
    {
        QTextStream(stderr) << "14E-04 FAIL: " << message << Qt::endl;
        std::exit(1);
    }
}

QString FindArgumentValue(
    const QStringList& arguments,
    const QString& name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments.at(index + 1)
        : QString{};
}

QJsonObject ExecuteJson(
    ModuleClient& client,
    const QJsonObject& request)
{
    QByteArray bytes;
    QString error;
    Require(
        client.Execute(
            QJsonDocument(request).toJson(QJsonDocument::Compact),
            &bytes,
            &error),
        error);
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    Require(document.isObject(), QStringLiteral("result is not JSON"));
    return document.object();
}

QJsonObject Transform(double translateX = 0.0)
{
    return QJsonObject{
        {QStringLiteral("translateXMm"), translateX},
        {QStringLiteral("translateYMm"), 0.0},
        {QStringLiteral("rotateZDeg"), 0.0},
        {QStringLiteral("uniformScale"), 1.0},
        {QStringLiteral("mirrorX"), false},
        {QStringLiteral("mirrorY"), false}};
}

QJsonObject Bounds(double translateX = 0.0)
{
    return QJsonObject{
        {QStringLiteral("min"), QJsonObject{
             {QStringLiteral("x"), translateX},
             {QStringLiteral("y"), 0.0},
             {QStringLiteral("z"), 0.05}}},
        {QStringLiteral("max"), QJsonObject{
             {QStringLiteral("x"), translateX + 2.0},
             {QStringLiteral("y"), 1.0},
             {QStringLiteral("z"), 0.25}}}};
}

QJsonObject BuildScene(const QString& fixturePath)
{
    const QString rootPath = QFileInfo(fixturePath).absolutePath();
    const QJsonObject transform = Transform();
    const QJsonObject bounds = Bounds();
    return QJsonObject{
        {QStringLiteral("schema"),
         QStringLiteral("slicesoft.multimodel_scene.13b.1")},
        {QStringLiteral("subjectType"), QStringLiteral("scene")},
        {QStringLiteral("sceneId"), QStringLiteral("stage14e04-scene")},
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
             {QStringLiteral("spacingMode"),
              QStringLiteral("edge_clearance")},
             {QStringLiteral("order"), QStringLiteral("row_major")}}},
        {QStringLiteral("materialBindingMode"),
         QStringLiteral("scene_profile_only")},
        {QStringLiteral("resolvedProfileId"),
         QStringLiteral("stage14e04-profile")},
        {QStringLiteral("resourceScopes"), QJsonArray{QJsonObject{
             {QStringLiteral("resourceScopeId"),
              QStringLiteral("stage14e04-scope")},
             {QStringLiteral("kind"), QStringLiteral("obj_directory")},
             {QStringLiteral("rootPath"), rootPath},
             {QStringLiteral("packagePath"), QString()},
             {QStringLiteral("partIdentity"), QString()}}}},
        {QStringLiteral("models"), QJsonArray{QJsonObject{
             {QStringLiteral("modelId"), QStringLiteral("stage14e04-model")},
             {QStringLiteral("sourcePath"), fixturePath},
             {QStringLiteral("format"), QStringLiteral("obj")},
             {QStringLiteral("resourceScopeId"),
              QStringLiteral("stage14e04-scope")},
             {QStringLiteral("sourceHash"),
              QStringLiteral("stage14e04-source")},
             {QStringLiteral("resourceHash"),
              QStringLiteral("stage14e04-resource")},
             {QStringLiteral("displayName"),
              QStringLiteral("Stage 14E-04 textured fixture")}}}},
        {QStringLiteral("instances"), QJsonArray{QJsonObject{
             {QStringLiteral("instanceId"), QString::fromLatin1(kInstanceId)},
             {QStringLiteral("modelId"), QStringLiteral("stage14e04-model")},
             {QStringLiteral("sourceTransformIdentity"),
              QStringLiteral("stage14e04-transform")},
             {QStringLiteral("requestedTransform"), transform},
             {QStringLiteral("derivedLayoutTransform"), transform},
             {QStringLiteral("effectiveTransform"), transform},
             {QStringLiteral("visible"), true},
             {QStringLiteral("locked"), false},
             {QStringLiteral("transformRevision"), 0},
             {QStringLiteral("sourceBboxMm"), bounds},
             {QStringLiteral("effectiveBboxMm"), bounds},
             {QStringLiteral("admissionStatus"),
              QStringLiteral("admitted")},
             {QStringLiteral("resolvedProfileId"),
              QStringLiteral("stage14e04-profile")}}}}};
}

int CountOpaqueColors(const QImage& image)
{
    QSet<QRgb> colors;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QRgb pixel = image.pixel(x, y);
            if (qAlpha(pixel) != 0)
            {
                colors.insert(qRgb(qRed(pixel), qGreen(pixel), qBlue(pixel)));
            }
            if (colors.size() >= 2)
            {
                return colors.size();
            }
        }
    }
    return colors.size();
}

int CountPaintedPixels(const QImage& image)
{
    const QRgb background = qRgb(43, 45, 49);
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (image.pixel(x, y) != background)
            {
                ++count;
            }
        }
    }
    return count;
}

double Percentile95(QVector<double> samples)
{
    std::sort(samples.begin(), samples.end());
    const int index = (std::min)(
        samples.size() - 1,
        static_cast<int>(samples.size() * 0.95));
    return samples.at(index);
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QString modulePath = FindArgumentValue(
        application.arguments(),
        QStringLiteral("--module"));
    Require(!modulePath.isEmpty(), QStringLiteral("--module is required"));

    const QString fixturePath = QDir(QStringLiteral(SLICESOFT_SOURCE_DIR))
                                    .filePath(QStringLiteral(
                                        "samples/models/textured/fixtures/"
                                        "policy_textured_small.obj"));
    Require(QFileInfo::exists(fixturePath), QStringLiteral("fixture missing"));

    ModuleClient client;
    QString error;
    Require(client.Open(modulePath, QByteArrayLiteral("{}"), &error), error);
    const QJsonObject imported = ExecuteJson(
        client,
        QJsonObject{
            {QStringLiteral("capability"), QStringLiteral("model.import")},
            {QStringLiteral("modelPath"), fixturePath},
            {QStringLiteral("options"), QJsonObject{
                 {QStringLiteral("computeBBox"), true},
                 {QStringLiteral("extractMaterials"), true}}}});
    Require(imported.value(QStringLiteral("ok")).toBool(),
            QStringLiteral("model import failed"));

    SceneInteractionController controller(client);
    Require(controller.Initialize(BuildScene(fixturePath), &error), error);

    TopViewRenderPolicy renderer(client);
    TopViewFrame frame;
    Require(
        renderer.Refresh(
            controller.SceneHandle(),
            controller.SceneRevision(),
            &frame,
            &error),
        error);
    Require(frame.instances.size() == 1,
            QStringLiteral("top ViewData instance count mismatch"));
    Require(frame.instances.front().textureStatus == QStringLiteral("available"),
            QStringLiteral("textured fixture was not reported available"));
    Require(CountOpaqueColors(frame.instances.front().surfacePreview) >= 2,
            QStringLiteral("surfacePreview lost real texture variation"));
    const QImage initial = renderer.Render(frame, QSize(800, 400));
    Require(!initial.isNull() && CountPaintedPixels(initial) > 0,
            QStringLiteral("top renderer produced an empty frame"));

    const quint64 firstReads = renderer.BlobReadCount();
    TopViewFrame cachedFrame;
    Require(
        renderer.Refresh(
            controller.SceneHandle(),
            controller.SceneRevision(),
            &cachedFrame,
            &error),
        error);
    Require(renderer.BlobReadCount() == firstReads,
            QStringLiteral("same preview identity bypassed the local cache"));

    MoveOptimizationPolicy movement;
    client.ResetCallCount();
    Require(movement.Begin(frame, QString::fromLatin1(kInstanceId)),
            QStringLiteral("local movement did not start"));
    QElapsedTimer renderTimer;
    renderTimer.start();
    for (int index = 0; index < 300; ++index)
    {
        Require(
            movement.UpdateTranslation(index * 0.001, 0.0, 0.0),
            QStringLiteral("local movement update failed"));
        Require(!renderer.Render(movement.Frame(), QSize(800, 400)).isNull(),
                QStringLiteral("local movement render failed"));
    }
    const qint64 renderElapsedMs = renderTimer.elapsed();
    Require(client.CallCount() == 0,
            QStringLiteral("local movement/render crossed the DLL boundary"));
    movement.Rollback();

    QVector<double> commitMilliseconds;
    commitMilliseconds.reserve(60);
    const quint64 snapshotsBefore = controller.SnapshotReadCount();
    for (int index = 0; index < 60; ++index)
    {
        const double delta = index % 2 == 0 ? 0.01 : -0.01;
        Require(controller.BeginTransient(QString::fromLatin1(kInstanceId)),
                QStringLiteral("Commit transient did not start"));
        Require(movement.Begin(frame, QString::fromLatin1(kInstanceId)),
                QStringLiteral("Commit preview did not start"));
        Require(controller.UpdateTransientTranslation(delta, 0.0, 0.0),
                QStringLiteral("Commit transform update failed"));
        Require(movement.UpdateTranslation(delta, 0.0, 0.0),
                QStringLiteral("Commit preview update failed"));
        QElapsedTimer commitTimer;
        commitTimer.start();
        Require(controller.CommitTransient(&error) == CommitOutcome::Committed,
                error);
        commitMilliseconds.push_back(
            static_cast<double>(commitTimer.nsecsElapsed()) / 1000000.0);
        Require(
            movement.AcceptCommit(
                controller.SceneRevision(),
                controller.ViewDataIdentity()),
            QStringLiteral("Commit ViewData identity was not adopted"));
        frame = movement.Frame();
    }

    const double p95Ms = Percentile95(commitMilliseconds);
    Require(p95Ms <= 150.0,
            QStringLiteral("UI-M2 P95 exceeded 150 ms: %1").arg(p95Ms));
    Require(controller.SnapshotReadCount() == snapshotsBefore,
            QStringLiteral("normal Commits appended snapshots"));

    TopViewFrame movedFrame;
    Require(
        renderer.Refresh(
            controller.SceneHandle(),
            controller.SceneRevision(),
            &movedFrame,
            &error),
        error);
    Require(renderer.BlobReadCount() == firstReads,
            QStringLiteral("world movement invalidated preview cache"));
    Require(movedFrame.instances.front().previewIdentity
                == cachedFrame.instances.front().previewIdentity,
            QStringLiteral("world movement changed preview identity"));

    QTextStream(stdout)
        << "14E-04 top-view contract: PASS colors="
        << CountOpaqueColors(frame.instances.front().surfacePreview)
        << " blobReads=" << renderer.BlobReadCount()
        << " commits=" << commitMilliseconds.size()
        << " p95Ms=" << p95Ms
        << " localFrames=" << 300
        << " localRenderMs=" << renderElapsedMs << Qt::endl;
    return 0;
}
