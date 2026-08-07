#include "apps/slicer_debug_ui/models/SceneDocument.h"
#include "apps/slicer_debug_ui/models/SceneSelectionModel.h"
#include "apps/slicer_debug_ui/widgets/ModelTopViewWidget.h"
#include "apps/slicer_ui_host_sim/ModuleClient.h"
#include "apps/slicer_ui_host_sim/render/TopViewRenderPolicy.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <utility>
#include <vector>

namespace
{
void Require(bool condition, const QString& message)
{
    if (!condition)
    {
        QTextStream(stderr) << "14E-04 benchmark FAIL: "
                            << message << Qt::endl;
        std::exit(1);
    }
}

int FindDurationSeconds(const QStringList& arguments)
{
    const int index = arguments.indexOf(QStringLiteral("--seconds"));
    if (index < 0 || index + 1 >= arguments.size())
    {
        return 1;
    }
    bool valid = false;
    const int seconds = arguments.at(index + 1).toInt(&valid);
    return valid ? (std::clamp)(seconds, 1, 60) : 1;
}

std::array<double, 16> IdentityMatrix()
{
    std::array<double, 16> matrix{};
    matrix.at(0) = 1.0;
    matrix.at(5) = 1.0;
    matrix.at(10) = 1.0;
    matrix.at(15) = 1.0;
    return matrix;
}

TopViewFrame BuildReferenceFrame(const QImage& source)
{
    TopViewInstance instance;
    instance.instanceId = QStringLiteral("stage14e04-benchmark-instance");
    instance.previewIdentity = QStringLiteral("stage14e04-benchmark-preview");
    instance.appearanceIdentity =
        QStringLiteral("stage14e04-benchmark-appearance");
    instance.textureStatus = QStringLiteral("available");
    instance.localBoundsMm = {0.0, 0.0, 2.0, 1.0};
    instance.worldMatrix = IdentityMatrix();
    instance.surfacePreview = source;

    TopViewFrame frame;
    frame.viewDataIdentity = QStringLiteral("stage14e04-benchmark-viewdata");
    frame.sceneRevision = 1;
    frame.instances.push_back(std::move(instance));
    return frame;
}

slicer_core::BoundingBox Bounds()
{
    slicer_core::BoundingBox bounds;
    bounds.min = {0.0, 0.0, 0.0};
    bounds.max = {2.0, 1.0, 0.2};
    return bounds;
}

slicer_core::SceneViewGeometry BuildMainGeometry(const QImage& source)
{
    slicer_core::SceneViewGeometry geometry;
    geometry.sceneid = "stage14e04-benchmark-scene";
    geometry.modelid = "stage14e04-benchmark-model";
    geometry.instanceid = "stage14e04-benchmark-instance";
    geometry.scenerevision = 1;
    geometry.sourcebboxmm = Bounds();
    geometry.effectivebboxmm = Bounds();
    geometry.worldboundsmm = {{0.0, 0.0}, {2.0, 1.0}};
    geometry.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Admitted;
    geometry.surfacepreview.width = source.width();
    geometry.surfacepreview.height = source.height();
    geometry.surfacepreview.contenthash =
        "stage14e04-benchmark-preview";
    geometry.surfacepreview.rgba.resize(
        static_cast<std::size_t>(source.width())
        * static_cast<std::size_t>(source.height()) * 4U);
    for (int y = 0; y < source.height(); ++y)
    {
        const uchar* row = source.constScanLine(y);
        const auto offset = static_cast<std::size_t>(y)
            * static_cast<std::size_t>(source.width()) * 4U;
        std::copy_n(
            row,
            static_cast<std::size_t>(source.width()) * 4U,
            geometry.surfacepreview.rgba.begin()
                + static_cast<std::ptrdiff_t>(offset));
    }
    return geometry;
}

void PopulateMainDocument(
    SceneDocument* document,
    const QImage& source)
{
    constexpr quint64 generation = 1;
    document->SetLoading(generation, QStringLiteral("benchmark.obj"));
    slicer_core::ModelInstance instance;
    instance.instanceid = "stage14e04-benchmark-instance";
    instance.modelid = "stage14e04-benchmark-model";
    instance.sourcetransformidentity = "stage14e04-benchmark-transform";
    instance.sourcebboxmm = Bounds();
    instance.effectivebboxmm = Bounds();
    Require(
        document->SetSceneContext(
            generation,
            QStringLiteral("stage14e04-benchmark-scene"),
            1,
            QStringLiteral("stage14e04-cache"),
            QStringLiteral("stage14e04-source"),
            QStringLiteral("stage14e04-resource"),
            std::move(instance)),
        QStringLiteral("main baseline scene setup failed"));
    Require(
        document->SetGeometry(generation, BuildMainGeometry(source)),
        QStringLiteral("main baseline geometry setup failed"));
}

struct BenchmarkResult final
{
    qint64 frames{0};
    qint64 elapsedNanoseconds{0};
};

void BenchmarkHostBatch(
    TopViewRenderPolicy* renderer,
    const TopViewFrame& frame,
    QImage* canvas,
    BenchmarkResult* result)
{
    constexpr int kBatchSize{32};
    QElapsedTimer timer;
    timer.start();
    quint32 checksum{0};
    for (int index = 0; index < kBatchSize; ++index)
    {
        Require(
            renderer->RenderInto(frame, canvas),
            QStringLiteral("host retained render failed"));
        checksum ^= canvas->pixel(400, 200);
    }
    result->elapsedNanoseconds += timer.nsecsElapsed();
    result->frames += kBatchSize;
    Require(checksum != 0 || result->frames > 1,
            QStringLiteral("host benchmark was optimized away"));
}

void BenchmarkMainBatch(
    ModelTopViewWidget* widget,
    BenchmarkResult* result)
{
    constexpr int kBatchSize{32};
    QImage canvas(QSize(800, 400), QImage::Format_RGBA8888);
    QElapsedTimer timer;
    timer.start();
    quint32 checksum{0};
    for (int index = 0; index < kBatchSize; ++index)
    {
        widget->render(&canvas);
        checksum ^= canvas.pixel(400, 200);
    }
    result->elapsedNanoseconds += timer.nsecsElapsed();
    result->frames += kBatchSize;
    Require(checksum != 0 || result->frames > 1,
            QStringLiteral("main benchmark was optimized away"));
}

double FramesPerSecond(const BenchmarkResult& result)
{
    return static_cast<double>(result.frames) * 1.0e9
        / static_cast<double>(result.elapsedNanoseconds);
}
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    const int seconds = FindDurationSeconds(application.arguments());
    const QString texturePath = QDir(QStringLiteral(SLICESOFT_SOURCE_DIR))
                                    .filePath(QStringLiteral(
                                        "samples/models/textured/textures/"
                                        "gradient.png"));
    Require(QFileInfo::exists(texturePath), QStringLiteral("texture missing"));
    const QImage source = QImage(texturePath).convertToFormat(
        QImage::Format_RGBA8888);
    Require(!source.isNull(), QStringLiteral("texture decode failed"));

    ModuleClient unusedClient;
    TopViewRenderPolicy hostRenderer(unusedClient);
    const TopViewFrame frame = BuildReferenceFrame(source);

    SceneDocument document;
    SceneSelectionModel selection;
    PopulateMainDocument(&document, source);
    ModelTopViewWidget mainWidget(&document, &selection);
    mainWidget.resize(800, 400);

    const qint64 durationNanoseconds =
        static_cast<qint64>(seconds) * 1000000000LL;
    BenchmarkResult mainResult;
    BenchmarkResult hostResult;
    QImage hostCanvas(QSize(800, 400), QImage::Format_RGBA8888);
    while (mainResult.elapsedNanoseconds < durationNanoseconds
           || hostResult.elapsedNanoseconds < durationNanoseconds)
    {
        if (mainResult.elapsedNanoseconds < durationNanoseconds)
        {
            BenchmarkMainBatch(&mainWidget, &mainResult);
        }
        if (hostResult.elapsedNanoseconds < durationNanoseconds)
        {
            BenchmarkHostBatch(
                &hostRenderer,
                frame,
                &hostCanvas,
                &hostResult);
        }
    }
    const double mainFps = FramesPerSecond(mainResult);
    const double hostFps = FramesPerSecond(hostResult);
    const double ratio = hostFps / mainFps;
    Require(
        ratio >= 0.90,
        QStringLiteral("UI-M3 ratio below 90%: %1").arg(ratio));

    QTextStream(stdout)
        << "14E-04 UI-M3: PASS seconds=" << seconds
        << " mainFps=" << mainFps
        << " hostFps=" << hostFps
        << " ratio=" << ratio << Qt::endl;
    return 0;
}
