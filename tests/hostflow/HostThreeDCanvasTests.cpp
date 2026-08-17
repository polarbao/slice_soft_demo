#include "HostModelImportWorkflow.h"
#include "ModuleClient.h"
#include "ThreeDCanvasWidget.h"
#include "ViewWorkspaceWidget.h"
#include "render/CpuRasterBackend.h"
#include "render/SceneRenderPolicy.h"
#include "render/TopViewRenderPolicy.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMouseEvent>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <numeric>

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
        QElapsedTimer timer;
        timer.start();
        const bool uploaded = m_delegate.UploadMesh(mesh);
        m_meshUploadNanoseconds += static_cast<quint64>(timer.nsecsElapsed());
        return uploaded;
    }

    bool UploadTexture(const slicer::render::TextureDesc& texture) override
    {
        m_textureBytes += static_cast<quint64>(texture.widthPx)
            * static_cast<quint64>(texture.heightPx) * 4U;
        QElapsedTimer timer;
        timer.start();
        const bool uploaded = m_delegate.UploadTexture(texture);
        m_textureUploadNanoseconds += static_cast<quint64>(
            timer.nsecsElapsed());
        return uploaded;
    }

    bool UploadMaterial(const slicer::render::MaterialDesc& material) override
    {
        return m_delegate.UploadMaterial(material);
    }

    void ReleaseUnused(
        const std::vector<std::string>& liveIdentities) override
    {
        m_delegate.ReleaseUnused(liveIdentities);
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

    [[nodiscard]] quint64 MeshUploadNanoseconds() const
    {
        return m_meshUploadNanoseconds;
    }

    [[nodiscard]] quint64 TextureUploadNanoseconds() const
    {
        return m_textureUploadNanoseconds;
    }

private:
    CpuRasterBackend m_delegate;
    quint64 m_vertexCount{0U};
    quint64 m_triangleCount{0U};
    quint64 m_meshBytes{0U};
    quint64 m_textureBytes{0U};
    quint64 m_meshUploadNanoseconds{0U};
    quint64 m_textureUploadNanoseconds{0U};
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

double ElapsedMilliseconds(const QElapsedTimer& timer)
{
    return static_cast<double>(timer.nsecsElapsed()) / 1.0e6;
}

void BenchmarkImportDirectory(
    const QString& modulePath,
    const QString& modelDirectory)
{
    const QDir directory(modelDirectory);
    const QFileInfoList files = directory.entryInfoList(
        QStringList{QStringLiteral("*.obj")},
        QDir::Files,
        QDir::Name | QDir::IgnoreCase);
    Require(!files.isEmpty(),
            QStringLiteral("benchmark model directory contains no OBJ files"));
    Require(files.size() <= 22,
            QStringLiteral("benchmark model directory exceeds scene capacity"));

    QStringList modelPaths;
    modelPaths.reserve(files.size());
    for (const QFileInfo& file : files)
    {
        modelPaths.push_back(file.absoluteFilePath());
    }

    ModuleClient client;
    QString error;
    Require(client.Open(modulePath, QByteArrayLiteral("{}"), &error), error);
    HostModelImportWorkflow workflow(client);
    QList<hostmodelimportresult> imported;

    QElapsedTimer totalTimer;
    totalTimer.start();
    QElapsedTimer timer;
    timer.start();
    Require(workflow.ImportModels(modelPaths, &imported, &error), error);
    const double importMs = ElapsedMilliseconds(timer);

    hostgridlayoutrequest layoutRequest;
    hostsceneeditresult layoutResult;
    timer.restart();
    Require(workflow.ApplyGridLayout(layoutRequest, &layoutResult, &error),
            error);
    const double layoutMs = ElapsedMilliseconds(timer);

    TopViewRenderPolicy topRenderer(client);
    TopViewFrame topFrame;
    timer.restart();
    Require(topRenderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &topFrame, &error),
            error);
    const double topRefreshMs = ElapsedMilliseconds(timer);
    timer.restart();
    const QImage topImage = topRenderer.Render(topFrame, QSize(1100, 700));
    Require(!topImage.isNull(), QStringLiteral("benchmark top render failed"));
    const double topRenderMs = ElapsedMilliseconds(timer);

    CpuRasterBackend backend;
    SceneRenderPolicy threeDRenderer(client, backend);
    ThreeDFrame threeDFrame;
    timer.restart();
    Require(threeDRenderer.Refresh(
                workflow.SceneHandle(), workflow.SceneRevision(),
                &threeDFrame, &error),
            error);
    const double threeDRefreshMs = ElapsedMilliseconds(timer);

    CameraController camera;
    camera.Fit(threeDFrame.worldBounds, 1100U, 700U);
    slicer::render::ImageOut image;
    timer.restart();
    Require(threeDRenderer.Render(
                threeDFrame, camera, 1100U, 700U, &image).ok,
            QStringLiteral("benchmark 3D render failed"));
    const double threeDRenderMs = ElapsedMilliseconds(timer);

    QTextStream output(stdout);
    output.setRealNumberNotation(QTextStream::FixedNotation);
    output.setRealNumberPrecision(3);
    output << "HOSTFLOW_IMPORT_BENCHMARK models=" << imported.size()
        << " triangles="
        << std::accumulate(
               imported.cbegin(), imported.cend(), quint64{0U},
               [](const quint64 total, const hostmodelimportresult& item)
               {
                   return total + item.trianglecount;
               })
        << " importMs=" << importMs
        << " layoutMs=" << layoutMs
        << " topRefreshMs=" << topRefreshMs
        << " topRenderMs=" << topRenderMs
        << " threeDRefreshMs=" << threeDRefreshMs
        << " threeDRenderMs=" << threeDRenderMs
        << " totalMs=" << ElapsedMilliseconds(totalTimer)
        << Qt::endl;
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
        evidence << "model,status,lod,vertices,triangles,meshBytes,textureBytes,"
                    "meshUploadNs,textureUploadNs,threeDRefreshNs,error\n";
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
        QElapsedTimer refreshTimer;
        refreshTimer.start();
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
                         << "\",rejected,,,,,,,,,\"" << escapedError << "\"\n";
            }
            continue;
        }
        const quint64 refreshNanoseconds = static_cast<quint64>(
            refreshTimer.nsecsElapsed());
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
                     << backend.TextureBytes() << ','
                     << backend.MeshUploadNanoseconds() << ','
                     << backend.TextureUploadNanoseconds() << ','
                     << refreshNanoseconds << ",\n";
        }
        renderedPaths.push_back(modelPath);
        ++renderedCount;
    }
    Require(renderedCount == 22
                && budgetRejectedCount == 0
                && assetRejectedCount == 14,
            QStringLiteral(
                "R-F-02 frozen matrix changed: rendered=%1 budget=%2 asset=%3")
                .arg(renderedCount)
                .arg(budgetRejectedCount)
                .arg(assetRejectedCount));

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
    QElapsedTimer aggregateThreeDTimer;
    aggregateThreeDTimer.start();
    const bool aggregateRendered = aggregateRenderer.Refresh(
        aggregateWorkflow.SceneHandle(),
        aggregateWorkflow.SceneRevision(),
        &aggregateFrame,
        &aggregateError);
    const quint64 aggregateThreeDRefreshNanoseconds =
        static_cast<quint64>(aggregateThreeDTimer.nsecsElapsed());

    TopViewRenderPolicy aggregateTopRenderer(aggregateClient);
    TopViewFrame aggregateTopFrame;
    QString aggregateTopError;
    QElapsedTimer aggregateTopTimer;
    aggregateTopTimer.start();
    const bool aggregateTopRendered = aggregateTopRenderer.Refresh(
        aggregateWorkflow.SceneHandle(),
        aggregateWorkflow.SceneRevision(),
        &aggregateTopFrame,
        &aggregateTopError);
    const quint64 aggregatePreviewRefreshNanoseconds =
        static_cast<quint64>(aggregateTopTimer.nsecsElapsed());
    quint64 aggregatePreviewBytes{0U};
    QSet<QString> previewIdentities;
    for (const TopViewInstance& instance : aggregateTopFrame.instances)
    {
        if (previewIdentities.contains(instance.previewIdentity))
        {
            continue;
        }
        previewIdentities.insert(instance.previewIdentity);
        aggregatePreviewBytes += static_cast<quint64>(
            instance.surfacePreview.sizeInBytes());
    }
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
            << "previewBytes=" << aggregatePreviewBytes << '\n'
            << "meshUploadNs="
            << aggregateBackend.MeshUploadNanoseconds() << '\n'
            << "textureUploadNs="
            << aggregateBackend.TextureUploadNanoseconds() << '\n'
            << "threeDRefreshNs="
            << aggregateThreeDRefreshNanoseconds << '\n'
            << "previewRefreshNs="
            << aggregatePreviewRefreshNanoseconds << '\n'
            << "topRendered="
            << (aggregateTopRendered ? "true" : "false") << '\n'
            << "previewIdentityCount=" << previewIdentities.size() << '\n'
            << "error=" << aggregateError << '\n'
            << "topError=" << aggregateTopError << '\n';
    }
    Require(aggregateRendered,
            QStringLiteral("R-F-02 aggregate three_d failed: %1")
                .arg(aggregateError));
    Require(aggregateTopRendered,
            QStringLiteral("R-F-02 aggregate top failed: %1")
                .arg(aggregateTopError));
    QTextStream(stdout)
        << "HOSTFLOW_H_D_02_ASSET_MATRIX_PASS total=" << modelPaths.size()
        << " lod0=" << renderedCount
        << " budgetRejected=" << budgetRejectedCount
        << " assetRejected=" << assetRejectedCount
        << " aggregateRendered=" << aggregateRendered
        << " aggregateLod=" << aggregateFrame.meshLod
        << " aggregateTopRendered=" << aggregateTopRendered
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
    const QString benchmarkModelDirectory = ArgumentValue(
        application.arguments(), QStringLiteral("--benchmark-model-dir"));
    if (!benchmarkModelDirectory.isEmpty())
    {
        BenchmarkImportDirectory(modulePath, benchmarkModelDirectory);
        return 0;
    }

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

    const auto dragOrbit = [&](const QPoint& from, const QPoint& to)
    {
        QMouseEvent press(
            QEvent::MouseButtonPress,
            QPointF(from),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(canvas, &press);
        QMouseEvent move(
            QEvent::MouseMove,
            QPointF(to),
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(canvas, &move);
        QMouseEvent release(
            QEvent::MouseButtonRelease,
            QPointF(to),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(canvas, &release);
    };
    const QPoint orbitStart(canvas->width() / 2, canvas->height() / 2);
    constexpr int kOrbitTestDragPixels{100};
    canvas->SetPreset(CameraPreset::Isometric);
    const float initialYawDeg = canvas->Camera().YawDegrees();
    dragOrbit(orbitStart, orbitStart + QPoint(kOrbitTestDragPixels, 0));
    const float actualYawDeltaDeg =
        canvas->Camera().YawDegrees() - initialYawDeg;
    const float expectedYawDeltaDeg = 180.0F
        * static_cast<float>(kOrbitTestDragPixels)
        / static_cast<float>((std::max)(canvas->width(), 1));
    Require(std::abs(actualYawDeltaDeg - expectedYawDeltaDeg) < 0.05F,
            QStringLiteral("horizontal orbit is not viewport-normalized"));

    canvas->SetPreset(CameraPreset::Isometric);
    const float initialPitchDeg = canvas->Camera().PitchDegrees();
    dragOrbit(orbitStart, orbitStart + QPoint(0, kOrbitTestDragPixels));
    const float actualPitchDeltaDeg =
        canvas->Camera().PitchDegrees() - initialPitchDeg;
    const float expectedPitchDeltaDeg = 180.0F
        * static_cast<float>(kOrbitTestDragPixels)
        / static_cast<float>((std::max)(canvas->height(), 1));
    Require(std::abs(actualPitchDeltaDeg - expectedPitchDeltaDeg) < 0.05F
                && actualYawDeltaDeg < actualPitchDeltaDeg,
            QStringLiteral(
                "wide 3D viewport did not reduce horizontal orbit sensitivity"));

    CameraController continuityCamera;
    continuityCamera.Fit(frame.worldBounds, 1100U, 700U);
    continuityCamera.Orbit(0.0F, 42.5F);
    const slicer::render::CameraDesc beforePoleThreshold =
        continuityCamera.BuildCamera();
    continuityCamera.Orbit(0.0F, 1.0F);
    const slicer::render::CameraDesc afterPoleThreshold =
        continuityCamera.BuildCamera();
    const float rightAxisDot =
        beforePoleThreshold.viewMatrix[0]
            * afterPoleThreshold.viewMatrix[0]
        + beforePoleThreshold.viewMatrix[1]
            * afterPoleThreshold.viewMatrix[1]
        + beforePoleThreshold.viewMatrix[2]
            * afterPoleThreshold.viewMatrix[2];
    Require(rightAxisDot > 0.99F,
            QStringLiteral(
                "vertical orbit changed the camera up basis discontinuously"));

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
