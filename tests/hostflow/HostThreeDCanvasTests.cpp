#include "HostModelImportWorkflow.h"
#include "ModuleClient.h"
#include "ThreeDCanvasWidget.h"
#include "ViewWorkspaceWidget.h"
#include "render/CpuRasterBackend.h"
#include "render/SceneRenderPolicy.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <cstdlib>

namespace
{
class MeasuringRenderBackend final : public slicer::render::IRenderBackend
{
public:
    [[nodiscard]] slicer::render::BackendCaps Caps() const override
    {
        return {"ra02_measurement", true, false, false, 16384U, 0U};
    }

    bool UploadMesh(const slicer::render::MeshDesc& mesh) override
    {
        m_vertexCount += mesh.vertexCount;
        m_triangleCount += mesh.triangleCount;
        m_meshBytes += static_cast<quint64>(mesh.vertexCount) * 32U
            + static_cast<quint64>(mesh.triangleCount) * 12U;
        return true;
    }

    bool UploadTexture(const slicer::render::TextureDesc& texture) override
    {
        m_textureBytes += static_cast<quint64>(texture.widthPx)
            * static_cast<quint64>(texture.heightPx) * 4U;
        return true;
    }

    bool UploadMaterial(const slicer::render::MaterialDesc&) override
    {
        return true;
    }

    void ReleaseUnused(const std::vector<std::string>&) override
    {
    }

    [[nodiscard]] slicer::render::FrameResult RenderFrame(
        const slicer::render::FrameDesc&) override
    {
        slicer::render::FrameResult result;
        result.ok = true;
        return result;
    }

    bool RenderToImage(
        const slicer::render::FrameDesc&,
        slicer::render::ImageOut&) override
    {
        return false;
    }

    [[nodiscard]] slicer::render::PickResult Pick(
        const slicer::render::FrameDesc&,
        const int,
        const int) override
    {
        return {};
    }

    [[nodiscard]] quint64 VertexCount() const
    {
        return m_vertexCount;
    }

    [[nodiscard]] quint64 TriangleCount() const
    {
        return m_triangleCount;
    }

    [[nodiscard]] quint64 MeshBytes() const
    {
        return m_meshBytes;
    }

    [[nodiscard]] quint64 TextureBytes() const
    {
        return m_textureBytes;
    }

private:
    quint64 m_vertexCount{0U};
    quint64 m_triangleCount{0U};
    quint64 m_meshBytes{0U};
    quint64 m_textureBytes{0U};
};

void Require(const bool condition, const QString& message)
{
    if (!condition)
    {
        QTextStream(stderr) << "H-D-02 FAIL: " << message << Qt::endl;
        std::exit(1);
    }
}

QString ArgumentValue(const QStringList& arguments, const QString& name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments.at(index + 1) : QString{};
}

QImage ToImage(const slicer::render::ImageOut& output)
{
    if (output.widthPx == 0U || output.heightPx == 0U
        || output.rgba8.size() != static_cast<std::size_t>(
            output.widthPx) * output.heightPx * 4U)
    {
        return {};
    }
    return QImage(
        output.rgba8.data(),
        static_cast<int>(output.widthPx),
        static_cast<int>(output.heightPx),
        static_cast<int>(output.widthPx * 4U),
        QImage::Format_RGBA8888).copy();
}

int CountColorfulPixels(const QImage& image)
{
    int count{0};
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QRgb pixel = image.pixel(x, y);
            const int minimum = (std::min)(
                {qRed(pixel), qGreen(pixel), qBlue(pixel)});
            const int maximum = (std::max)(
                {qRed(pixel), qGreen(pixel), qBlue(pixel)});
            count += maximum - minimum > 35 ? 1 : 0;
        }
    }
    return count;
}

void VerifyRealAssetMatrix(
    const QString& modulePath,
    const QString& repositoryRoot,
    const QString& evidenceRoot)
{
    const QString modelRoot = QDir(repositoryRoot).filePath(
        QStringLiteral("model/obj"));
    QStringList modelPaths;
    QDirIterator iterator(
        modelRoot,
        QStringList{QStringLiteral("*.obj")},
        QDir::Files,
        QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
        modelPaths.append(iterator.next());
    }
    modelPaths.sort(Qt::CaseInsensitive);
    Require(modelPaths.size() >= 36,
            QStringLiteral("RB-P1 real-asset matrix contains fewer than 36 OBJ files"));

    int renderedCount{0};
    int budgetRejectedCount{0};
    int assetRejectedCount{0};
    QStringList renderedPaths;
    QFile evidenceFile;
    QTextStream evidence;
    if (!evidenceRoot.isEmpty())
    {
        Require(QDir().mkpath(evidenceRoot),
                QStringLiteral("R-A-02 evidence directory creation failed"));
        evidenceFile.setFileName(QDir(evidenceRoot).filePath(
            QStringLiteral("render_ra02_real_asset_matrix.csv")));
        Require(evidenceFile.open(QIODevice::WriteOnly | QIODevice::Text),
                QStringLiteral("R-A-02 evidence file creation failed"));
        evidence.setDevice(&evidenceFile);
        evidence << "model,status,lod,vertices,triangles,meshBytes,textureBytes,error\n";
    }
    for (const QString& modelPath : modelPaths)
    {
        ModuleClient matrixClient;
        QString error;
        Require(matrixClient.Open(
                    modulePath, QByteArrayLiteral("{}"), &error),
                error);
        HostModelImportWorkflow workflow(matrixClient);
        hostmodelimportresult imported;
        Require(workflow.ImportModel(modelPath, &imported, &error),
                QStringLiteral("%1：%2").arg(modelPath, error));

        MeasuringRenderBackend backend;
        SceneRenderPolicy renderer(matrixClient, backend);
        ThreeDFrame frame;
        if (!renderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &frame, &error))
        {
            if (error.contains(QStringLiteral("PM-SLICER-VIEWDATA-BUDGET")))
            {
                ++budgetRejectedCount;
            }
            else
            {
                Require(error.contains(QStringLiteral("PM-SLICER-INPUT-0001"))
                            || error.contains(
                                QStringLiteral("PM-SLICER-INPUT-0002")),
                        QStringLiteral("%1：%2").arg(modelPath, error));
                ++assetRejectedCount;
            }
            if (evidence.device() != nullptr)
            {
                QString escapedError = error;
                escapedError.replace('"', QStringLiteral("\"\""));
                evidence << '"' << QDir(modelRoot).relativeFilePath(modelPath)
                         << "\",rejected,,,,,,\"" << escapedError << "\"\n";
            }
            continue;
        }
        Require(frame.meshLod == QStringLiteral("lod0")
                    && !frame.descriptor.instances.empty(),
                QStringLiteral(
                    "%1：auto LOD returned destructive degradation %2")
                    .arg(modelPath, frame.meshLod));
        if (evidence.device() != nullptr)
        {
            evidence << '"' << QDir(modelRoot).relativeFilePath(modelPath)
                     << "\",rendered," << frame.meshLod << ','
                     << backend.VertexCount() << ','
                     << backend.TriangleCount() << ','
                     << backend.MeshBytes() << ','
                     << backend.TextureBytes() << ",\n";
        }
        renderedPaths.push_back(modelPath);
        ++renderedCount;
    }

    ModuleClient aggregateClient;
    QString aggregateError;
    Require(aggregateClient.Open(
                modulePath, QByteArrayLiteral("{}"), &aggregateError),
            aggregateError);
    HostModelImportWorkflow aggregateWorkflow(aggregateClient);
    QList<hostmodelimportresult> aggregateImports;
    Require(aggregateWorkflow.ImportModels(
                renderedPaths, &aggregateImports, &aggregateError),
            QStringLiteral("R-A-02 aggregate import: %1").arg(aggregateError));
    MeasuringRenderBackend aggregateBackend;
    SceneRenderPolicy aggregateRenderer(aggregateClient, aggregateBackend);
    ThreeDFrame aggregateFrame;
    const bool aggregateRendered = aggregateRenderer.Refresh(
        aggregateWorkflow.SceneHandle(),
        aggregateWorkflow.SceneRevision(),
        &aggregateFrame,
        &aggregateError);
    if (!evidenceRoot.isEmpty())
    {
        QFile aggregateFile(QDir(evidenceRoot).filePath(
            QStringLiteral("render_ra02_aggregate_scene.txt")));
        Require(aggregateFile.open(QIODevice::WriteOnly | QIODevice::Text),
                QStringLiteral("R-A-02 aggregate evidence creation failed"));
        QTextStream aggregateEvidence(&aggregateFile);
        aggregateEvidence
            << "validAssets=" << renderedPaths.size() << '\n'
            << "sceneInstances=" << aggregateImports.size() << '\n'
            << "rendered=" << (aggregateRendered ? "true" : "false") << '\n'
            << "lod=" << aggregateFrame.meshLod << '\n'
            << "vertices=" << aggregateBackend.VertexCount() << '\n'
            << "triangles=" << aggregateBackend.TriangleCount() << '\n'
            << "meshBytes=" << aggregateBackend.MeshBytes() << '\n'
            << "textureBytes=" << aggregateBackend.TextureBytes() << '\n'
            << "error=" << aggregateError << '\n';
    }
    QTextStream(stdout)
        << "HOSTFLOW_H_D_02_ASSET_MATRIX_PASS total=" << modelPaths.size()
        << " lod0=" << renderedCount
        << " budgetRejected=" << budgetRejectedCount
        << " assetRejected=" << assetRejectedCount
        << " aggregateRendered=" << aggregateRendered
        << " aggregateLod=" << aggregateFrame.meshLod
        << " evidence=" << evidenceFile.fileName()
        << Qt::endl;
}
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    const QString modulePath = ArgumentValue(
        application.arguments(), QStringLiteral("--module"));
    const QString repositoryRoot = ArgumentValue(
        application.arguments(), QStringLiteral("--repo-root"));
    const QString evidenceRoot = ArgumentValue(
        application.arguments(), QStringLiteral("--evidence-root"));
    Require(!modulePath.isEmpty() && !repositoryRoot.isEmpty(),
            QStringLiteral("--module and --repo-root are required"));

    const QString modelPath = QDir(repositoryRoot).filePath(QStringLiteral(
        "model/obj/小马物语/小马物语小指/MF_Xiao_ma_Xiaozhi_ty03.obj"));
    Require(QFileInfo::exists(modelPath),
            QStringLiteral("production textured model is missing"));

    ModuleClient client;
    QString error;
    Require(client.Open(modulePath, QByteArrayLiteral("{}"), &error), error);
    HostModelImportWorkflow workflow(client);
    hostmodelimportresult imported;
    Require(workflow.ImportModel(modelPath, &imported, &error), error);
    hostgridlayoutrequest layoutRequest;
    hostsceneeditresult layoutResult;
    Require(workflow.ApplyGridLayout(
                layoutRequest, &layoutResult, &error),
            error);

    CpuRasterBackend backend;
    SceneRenderPolicy renderer(client, backend);
    ThreeDFrame frame;
    Require(renderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &frame, &error),
            error);
    Require(frame.meshLod == QStringLiteral("lod0"),
            QStringLiteral("RB-P1 auto LOD did not retain the valid lod0 mesh"));

    ViewWorkspaceWidget workspace;
    workspace.resize(1100, 700);
    workspace.SetMode(HostViewMode::ThreeD);
    workspace.show();
    application.processEvents();
    ThreeDCanvasWidget* canvas = workspace.ThreeDCanvas();
    Require(canvas != nullptr,
            QStringLiteral("visible three-dimensional canvas is missing"));
    workspace.SetThreeDSceneBounds(frame.worldBounds);
    int localRenderCount{0};
    bool renderSucceeded{true};
    workspace.ThreeDCanvas()->SetCameraChangedCallback([&]()
    {
        const QSize size = workspace.ThreeDRenderSize();
        slicer::render::ImageOut output;
        renderSucceeded = renderer.Render(
            frame,
            canvas->Camera(),
            static_cast<std::uint32_t>(size.width()),
            static_cast<std::uint32_t>(size.height()),
            &output).ok;
        const QImage image = ToImage(output);
        renderSucceeded = renderSucceeded && !image.isNull();
        if (renderSucceeded)
        {
            workspace.SetThreeDImage(image);
        }
        ++localRenderCount;
    });
    canvas->FitScene();
    Require(renderSucceeded && canvas->HasImage()
            && CountColorfulPixels(workspace.grab().toImage()) > 20,
            QStringLiteral("three-dimensional canvas lost real texture pixels"));

    auto* presetCombo = workspace.findChild<QComboBox*>(
        QStringLiteral("threeDCameraPresetCombo"));
    auto* projectionCombo = workspace.findChild<QComboBox*>(
        QStringLiteral("threeDToolbarProjectionCombo"));
    Require(presetCombo != nullptr && presetCombo->count() == 7
            && projectionCombo != nullptr && projectionCombo->count() == 2,
            QStringLiteral("seven presets or projection controls are missing"));

    client.ResetCallCount();
    canvas->Orbit(8.0F, -3.0F);
    canvas->Pan(0.25F, -0.1F);
    canvas->ZoomAtCursor(1.0F, 0.3F, -0.2F);
    const std::array<CameraPreset, 7> presets{
        CameraPreset::Top,
        CameraPreset::Bottom,
        CameraPreset::Front,
        CameraPreset::Back,
        CameraPreset::Left,
        CameraPreset::Right,
        CameraPreset::Isometric};
    for (const CameraPreset preset : presets)
    {
        canvas->SetPreset(preset);
    }
    canvas->SetProjection(slicer::render::Projection::Perspective);
    canvas->SetProjection(slicer::render::Projection::Orthographic);
    const quint64 cameraCallCount = client.CallCount();
    Require(renderSucceeded && localRenderCount >= 13
            && cameraCallCount == 0U,
            QStringLiteral("UI-M7 camera operation crossed the DLL boundary"));

    CpuRasterBackend budgetBackend;
    SceneRenderPolicy budgetRenderer(client, budgetBackend, 1LL);
    ThreeDFrame rejectedFrame;
    error.clear();
    Require(!budgetRenderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &rejectedFrame, &error)
            && error.contains(QStringLiteral("PM-SLICER-VIEWDATA-BUDGET"))
            && error.contains(QStringLiteral("不会使用破碎网格"))
            && rejectedFrame.descriptor.instances.empty(),
            QStringLiteral("RB-P1 budget failure was not explicit and fail-closed"));

    application.processEvents();
    if (!evidenceRoot.isEmpty())
    {
        Require(QDir().mkpath(evidenceRoot),
                QStringLiteral("evidence directory creation failed"));
        Require(workspace.grab().save(QDir(evidenceRoot).filePath(
                    QStringLiteral("hostflow_hd02_textured_three_d.png"))),
                QStringLiteral("visible three-dimensional screenshot failed"));
    }

    QTextStream(stdout)
        << "HOSTFLOW_H_D_02_THREE_D_PASS lod=" << frame.meshLod
        << " localRenders=" << localRenderCount
        << " cameraCalls=" << cameraCallCount
        << " budgetError=PM-SLICER-VIEWDATA-BUDGET"
        << Qt::endl;
    if (application.arguments().contains(
            QStringLiteral("--real-asset-matrix")))
    {
        VerifyRealAssetMatrix(modulePath, repositoryRoot, evidenceRoot);
    }
    return 0;
}
