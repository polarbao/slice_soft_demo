#include "ModuleClient.h"
#include "SceneInteractionController.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <cstdlib>

namespace
{
void Require(bool condition, const QString& message)
{
    if (!condition)
    {
        QTextStream(stderr) << "14E-03 FAIL: " << message << Qt::endl;
        std::exit(1);
    }
}

QString FindArgumentValue(
    const QStringList& arguments,
    const QString& name)
{
    const int index = arguments.indexOf(name);
    if (index < 0 || index + 1 >= arguments.size())
    {
        return {};
    }
    return arguments.at(index + 1);
}

QJsonObject Transform()
{
    return QJsonObject{
        {QStringLiteral("translateXMm"), 0.0},
        {QStringLiteral("translateYMm"), 0.0},
        {QStringLiteral("rotateZDeg"), 0.0},
        {QStringLiteral("uniformScale"), 1.0},
        {QStringLiteral("mirrorX"), false},
        {QStringLiteral("mirrorY"), false}};
}

QJsonObject Bounds()
{
    return QJsonObject{
        {QStringLiteral("min"), QJsonObject{
             {QStringLiteral("x"), 0.0},
             {QStringLiteral("y"), 0.0},
             {QStringLiteral("z"), 0.0}}},
        {QStringLiteral("max"), QJsonObject{
             {QStringLiteral("x"), 1.0},
             {QStringLiteral("y"), 1.0},
             {QStringLiteral("z"), 0.0}}}};
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
        {QStringLiteral("sceneId"), QStringLiteral("stage14e03-scene")},
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
         QStringLiteral("stage14e03-profile")},
        {QStringLiteral("resourceScopes"), QJsonArray{QJsonObject{
             {QStringLiteral("resourceScopeId"),
              QStringLiteral("stage14e03-scope")},
             {QStringLiteral("kind"), QStringLiteral("obj_directory")},
             {QStringLiteral("rootPath"), rootPath},
             {QStringLiteral("packagePath"), QString()},
             {QStringLiteral("partIdentity"), QString()}}}},
        {QStringLiteral("models"), QJsonArray{QJsonObject{
             {QStringLiteral("modelId"), QStringLiteral("stage14e03-model")},
             {QStringLiteral("sourcePath"), fixturePath},
             {QStringLiteral("format"), QStringLiteral("obj")},
             {QStringLiteral("resourceScopeId"),
              QStringLiteral("stage14e03-scope")},
             {QStringLiteral("sourceHash"),
              QStringLiteral("stage14e03-source")},
             {QStringLiteral("resourceHash"),
              QStringLiteral("stage14e03-resource")},
             {QStringLiteral("displayName"),
              QStringLiteral("Stage 14E-03 fixture")}}}},
        {QStringLiteral("instances"), QJsonArray{QJsonObject{
             {QStringLiteral("instanceId"),
              QStringLiteral("stage14e03-instance")},
             {QStringLiteral("modelId"), QStringLiteral("stage14e03-model")},
             {QStringLiteral("sourceTransformIdentity"),
              QStringLiteral("stage14e03-transform")},
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
              QStringLiteral("stage14e03-profile")}}}}};
}

QJsonObject ExecuteRequest(
    ModuleClient& client,
    const QJsonObject& request)
{
    QString error;
    pm_job_t* job = client.Submit(
        QJsonDocument(request).toJson(QJsonDocument::Compact),
        &error);
    Require(job != nullptr, QStringLiteral("submit failed: %1").arg(error));

    QByteArray progress;
    Require(client.Poll(job, &progress, &error), error);
    QByteArray resultBytes;
    Require(client.Result(job, &resultBytes, &error), error);
    client.Release(job);

    const QJsonDocument resultDocument = QJsonDocument::fromJson(resultBytes);
    Require(resultDocument.isObject(), QStringLiteral("result is not JSON"));
    return resultDocument.object();
}

QJsonObject BuildExternalCommit(
    quint64 sceneHandle,
    quint64 sceneRevision)
{
    const QJsonObject operation{
        {QStringLiteral("type"), QStringLiteral("translate")},
        {QStringLiteral("instanceId"),
         QStringLiteral("stage14e03-instance")},
        {QStringLiteral("deltaMm"), QJsonArray{0.5, 0.0, 0.0}}};
    return QJsonObject{
        {QStringLiteral("capability"),
         QStringLiteral("scene.apply_operation")},
        {QStringLiteral("operationId"),
         QStringLiteral("stage14e03-external-commit")},
        {QStringLiteral("sceneHandle"), static_cast<qint64>(sceneHandle)},
        {QStringLiteral("currentSceneRevision"),
         static_cast<qint64>(sceneRevision)},
        {QStringLiteral("expectedSceneRevision"),
         static_cast<qint64>(sceneRevision)},
        {QStringLiteral("operations"), QJsonArray{operation}}};
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
                                        "tests/fixtures/stage14b/"
                                        "model_with_normals.obj"));
    Require(QFileInfo::exists(fixturePath), QStringLiteral("fixture missing"));

    ModuleClient client;
    QString error;
    Require(
        client.Open(modulePath, QByteArrayLiteral("{}"), &error),
        error);

    const QJsonObject imported = ExecuteRequest(
        client,
        QJsonObject{
            {QStringLiteral("capability"), QStringLiteral("model.import")},
            {QStringLiteral("modelPath"), fixturePath},
            {QStringLiteral("options"), QJsonObject{
                 {QStringLiteral("computeBBox"), true},
                 {QStringLiteral("extractMaterials"), true}}}});
    Require(
        imported.value(QStringLiteral("ok")).toBool(),
        QStringLiteral("model import failed"));

    SceneInteractionController controller(client);
    Require(controller.Initialize(BuildScene(fixturePath), &error), error);
    Require(controller.SceneHandle() != 0, QStringLiteral("scene handle missing"));
    Require(
        controller.SnapshotReadCount() == 1,
        QStringLiteral("bootstrap must perform one explicit refresh"));

    client.ResetCallCount();
    Require(
        controller.BeginTransient(QStringLiteral("stage14e03-instance")),
        QStringLiteral("transient did not start"));
    for (int index = 0; index < 50; ++index)
    {
        Require(
            controller.UpdateTransientTranslation(
                index * 0.01,
                index * 0.02,
                0.0),
            QStringLiteral("transient update failed"));
    }
    Require(
        client.CallCount() == 0,
        QStringLiteral("UI-M1 violated: pointer updates crossed DLL"));

    const quint64 snapshotCountBeforeCommit = controller.SnapshotReadCount();
    Require(
        controller.CommitTransient(&error) == CommitOutcome::Committed,
        error);
    Require(
        controller.SnapshotReadCount() == snapshotCountBeforeCommit,
        QStringLiteral("normal Commit appended a forbidden snapshot"));
    Require(!controller.HasTransient(), QStringLiteral("Commit left transient state"));

    const quint64 revisionBeforeExternal = controller.SceneRevision();
    const QJsonObject externalResult = ExecuteRequest(
        client,
        BuildExternalCommit(
            controller.SceneHandle(),
            revisionBeforeExternal));
    Require(
        externalResult.value(QStringLiteral("ok")).toBool(),
        QStringLiteral("external commit failed"));
    Require(
        static_cast<quint64>(externalResult.value(
            QStringLiteral("newSceneRevision")).toDouble()) ==
            revisionBeforeExternal + 1,
        QStringLiteral("external revision did not advance once"));

    Require(
        controller.BeginTransient(QStringLiteral("stage14e03-instance")),
        QStringLiteral("stale transient did not start"));
    Require(
        controller.UpdateTransientTranslation(1.0, 0.0, 0.0),
        QStringLiteral("stale transient update failed"));
    Require(
        controller.CommitTransient(&error) == CommitOutcome::StaleRecovered,
        QStringLiteral("stale Commit did not recover: %1").arg(error));
    Require(
        controller.SceneRevision() == revisionBeforeExternal + 1,
        QStringLiteral("stale path retried or adopted wrong revision"));
    Require(
        controller.SnapshotReadCount() == snapshotCountBeforeCommit + 1,
        QStringLiteral("stale path did not read exactly one snapshot"));
    Require(
        !controller.HasTransient(),
        QStringLiteral("stale recovery retained transient state"));

    QTextStream(stdout)
        << "14E-03 interaction contract: PASS revision="
        << controller.SceneRevision()
        << " snapshots=" << controller.SnapshotReadCount()
        << " calls=" << client.CallCount() << Qt::endl;
    return 0;
}
