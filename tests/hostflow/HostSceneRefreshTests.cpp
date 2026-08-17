#include "HostModelImportWorkflow.h"
#include "ModuleClient.h"
#include "render/CpuRasterBackend.h"
#include "render/SceneRenderPolicy.h"
#include "render/TopViewRenderPolicy.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

#include <cstdint>
#include <cstdlib>

namespace
{
void Require(const bool condition, const QString& message)
{
    if (!condition)
    {
        QTextStream(stderr) << "H-D-04 FAIL: " << message << Qt::endl;
        std::exit(1);
    }
}

QString ArgumentValue(const QStringList& arguments, const QString& name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments.at(index + 1) : QString{};
}

const TopViewInstance* FindTopInstance(
    const TopViewFrame& frame,
    const QString& instanceId)
{
    for (const TopViewInstance& instance : frame.instances)
    {
        if (instance.instanceId == instanceId)
        {
            return &instance;
        }
    }
    return nullptr;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QString modulePath = ArgumentValue(
        application.arguments(), QStringLiteral("--module"));
    const QString repositoryRoot = ArgumentValue(
        application.arguments(), QStringLiteral("--repo-root"));
    Require(!modulePath.isEmpty() && !repositoryRoot.isEmpty(),
            QStringLiteral("--module and --repo-root are required"));

    const QString modelPath = QDir(repositoryRoot).filePath(QStringLiteral(
        "samples/models/textured/fixtures/policy_textured_small.obj"));
    Require(QFileInfo::exists(modelPath),
            QStringLiteral("textured refresh fixture is missing"));

    ModuleClient client;
    QString error;
    Require(client.Open(modulePath, QByteArrayLiteral("{}"), &error), error);
    HostModelImportWorkflow workflow(client);
    QStringList instanceIds;
    for (int index = 0; index < 22; ++index)
    {
        hostmodelimportresult result;
        Require(workflow.ImportModel(modelPath, &result, &error), error);
        instanceIds.push_back(result.instanceid);
    }
    Require(workflow.InstanceCount() == 22,
            QStringLiteral("22-instance scene was not admitted"));

    TopViewRenderPolicy topRenderer(client);
    CpuRasterBackend backend;
    SceneRenderPolicy threeDRenderer(client, backend);
    TopViewFrame initialTop;
    ThreeDFrame initialThreeD;
    Require(topRenderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &initialTop, &error),
            error);
    Require(threeDRenderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &initialThreeD, &error),
            error);
    Require(initialTop.instances.size() == 22
            && initialThreeD.descriptor.instances.size() == 22U,
            QStringLiteral("initial dual view did not contain 22 instances"));
    const std::uint64_t initialMeshUploads =
        threeDRenderer.MeshUploadCount();
    const std::uint64_t initialTextureUploads =
        threeDRenderer.TextureUploadCount();
    const int initialPreviewCount = topRenderer.CachedPreviewCount();

    hostsceneeditresult layoutResult;
    const quint64 revisionBeforeLayout = workflow.SceneRevision();
    Require(workflow.ApplyGridLayout(
                hostgridlayoutrequest{11, 2, 10.0, 10.0},
                &layoutResult, &error),
            error);
    Require(layoutResult.scenerevision == revisionBeforeLayout + 1U,
            QStringLiteral("grid layout did not commit exactly once"));

    TopViewFrame layoutTop;
    ThreeDFrame layoutThreeD;
    Require(topRenderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &layoutTop, &error),
            error);
    Require(threeDRenderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &layoutThreeD, &error),
            error);
    Require(layoutTop.instances.size() == 22
            && layoutThreeD.descriptor.instances.size() == 22U
            && layoutTop.sceneRevision == workflow.SceneRevision()
            && layoutThreeD.sceneRevision == workflow.SceneRevision(),
            QStringLiteral("22-instance layout did not refresh both views once"));
    Require(threeDRenderer.MeshUploadCount() == initialMeshUploads
            && threeDRenderer.TextureUploadCount() == initialTextureUploads,
            QStringLiteral("layout matrix change invalidated mesh or texture"));
    Require(topRenderer.CachedPreviewCount() == initialPreviewCount,
            QStringLiteral("layout matrix change invalidated surface preview"));

    const TopViewInstance* beforeTransform = FindTopInstance(
        layoutTop, instanceIds.front());
    Require(beforeTransform != nullptr,
            QStringLiteral("selected instance missing after layout"));
    const double translationBefore = beforeTransform->worldMatrix.at(3);
    hostsceneeditresult transformResult;
    Require(workflow.ApplyTransforms(
                QStringList{instanceIds.front()},
                hosttransformrequest{
                    0.5,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    1.0,
                    false,
                    false},
                &transformResult, &error),
            error);

    TopViewFrame transformedTop;
    ThreeDFrame transformedThreeD;
    Require(topRenderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &transformedTop, &error),
            error);
    Require(threeDRenderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &transformedThreeD, &error),
            error);
    const TopViewInstance* afterTransform = FindTopInstance(
        transformedTop, instanceIds.front());
    Require(afterTransform != nullptr
            && afterTransform->worldMatrix.at(3) > translationBefore,
            QStringLiteral("transform refresh retained a stale worldMatrix"));
    Require(threeDRenderer.MeshUploadCount() == initialMeshUploads
            && threeDRenderer.TextureUploadCount() == initialTextureUploads
            && topRenderer.CachedPreviewCount() == initialPreviewCount,
            QStringLiteral("pure transform invalidated immutable resources"));

    QTextStream(stdout)
        << "HOSTFLOW_H_D_04_REFRESH_PASS instances="
        << transformedTop.instances.size()
        << " layoutRevision=" << layoutResult.scenerevision
        << " transformRevision=" << transformResult.scenerevision
        << " meshUploadDelta="
        << (threeDRenderer.MeshUploadCount() - initialMeshUploads)
        << " textureUploadDelta="
        << (threeDRenderer.TextureUploadCount() - initialTextureUploads)
        << Qt::endl;
    return 0;
}
