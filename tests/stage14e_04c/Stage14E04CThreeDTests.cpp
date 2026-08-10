#include "ModuleClient.h"
#include "SceneInteractionController.h"
#include "camera/CameraController.h"
#include "render/CpuRasterBackend.h"
#include "render/MeshAttributeDecoder.h"
#include "render/SceneRenderPolicy.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace
{
constexpr auto kInstanceId = "stage14e04c-instance";

void Require(const bool condition, const QString& message)
{
    if (!condition)
    {
        QTextStream(stderr) << "14E-04c FAIL: " << message << Qt::endl;
        std::exit(1);
    }
}

QString ArgumentValue(const QStringList& arguments, const QString& name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size()
        ? arguments.at(index + 1) : QString{};
}

QJsonObject ExecuteJson(ModuleClient& client, const QJsonObject& request)
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
    return {
        {QStringLiteral("translateXMm"), 0.0},
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
             {QStringLiteral("x"), 0.0},
             {QStringLiteral("y"), 0.0},
             {QStringLiteral("z"), 0.05}}},
        {QStringLiteral("max"), QJsonObject{
             {QStringLiteral("x"), 2.0},
             {QStringLiteral("y"), 1.0},
             {QStringLiteral("z"), 0.25}}}};
}

QJsonObject BuildScene(const QString& fixturePath)
{
    const QJsonObject transform = Transform();
    const QJsonObject bounds = Bounds();
    return {
        {QStringLiteral("schema"),
         QStringLiteral("slicesoft.multimodel_scene.13b.1")},
        {QStringLiteral("subjectType"), QStringLiteral("scene")},
        {QStringLiteral("sceneId"), QStringLiteral("stage14e04c-scene")},
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
         QStringLiteral("stage14e04c-profile")},
        {QStringLiteral("resourceScopes"), QJsonArray{QJsonObject{
             {QStringLiteral("resourceScopeId"),
              QStringLiteral("stage14e04c-scope")},
             {QStringLiteral("kind"), QStringLiteral("obj_directory")},
             {QStringLiteral("rootPath"), QFileInfo(fixturePath).absolutePath()},
             {QStringLiteral("packagePath"), QString()},
             {QStringLiteral("partIdentity"), QString()}}}},
        {QStringLiteral("models"), QJsonArray{QJsonObject{
             {QStringLiteral("modelId"), QStringLiteral("stage14e04c-model")},
             {QStringLiteral("sourcePath"), fixturePath},
             {QStringLiteral("format"), QStringLiteral("obj")},
             {QStringLiteral("resourceScopeId"),
              QStringLiteral("stage14e04c-scope")},
             {QStringLiteral("sourceHash"),
              QStringLiteral("stage14e04c-source")},
             {QStringLiteral("resourceHash"),
              QStringLiteral("stage14e04c-resource")},
             {QStringLiteral("displayName"),
              QStringLiteral("Stage 14E-04c textured fixture")}}}},
        {QStringLiteral("instances"), QJsonArray{QJsonObject{
             {QStringLiteral("instanceId"), QString::fromLatin1(kInstanceId)},
             {QStringLiteral("modelId"), QStringLiteral("stage14e04c-model")},
             {QStringLiteral("sourceTransformIdentity"),
              QStringLiteral("stage14e04c-transform")},
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
              QStringLiteral("stage14e04c-profile")}}}}};
}

QJsonObject QueryViewData(
    ModuleClient& client,
    const quint64 sceneHandle,
    const quint64 sceneRevision,
    const QString& attributeFormat = {})
{
    QJsonObject request{
        {QStringLiteral("capability"), QStringLiteral("scene.get_viewdata")},
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sceneHandle"), static_cast<qint64>(sceneHandle)},
        {QStringLiteral("expectedSceneRevision"),
         static_cast<qint64>(sceneRevision)},
        {QStringLiteral("viewMode"), QStringLiteral("three_d")},
        {QStringLiteral("texturePolicy"),
         QStringLiteral("require_if_present")},
        {QStringLiteral("lod"), QStringLiteral("lod0")},
        {QStringLiteral("meshTransform"), QStringLiteral("local")},
        {QStringLiteral("maxBytes"), 64 * 1024 * 1024},
        {QStringLiteral("content"), QJsonArray{
             QStringLiteral("bbox"),
             QStringLiteral("outline"),
             QStringLiteral("mesh"),
             QStringLiteral("appearance")}}};
    if (!attributeFormat.isEmpty())
    {
        request.insert(
            QStringLiteral("meshAttributeFormat"),
            attributeFormat);
    }
    return ExecuteJson(client, request);
}

QJsonObject FirstMesh(const QJsonObject& viewData)
{
    Require(viewData.value(QStringLiteral("ok")).toBool(),
            QStringLiteral("ViewData query failed"));
    const QJsonArray meshes = viewData.value(QStringLiteral("meshes")).toArray();
    Require(meshes.size() == 1, QStringLiteral("expected one reusable mesh"));
    return meshes.first().toObject();
}

void VerifyFloat32DecoderCompatibility()
{
    const std::array<float, 3> source{0.0F, 1.0F, -2.0F};
    const QByteArray blob(
        reinterpret_cast<const char*>(source.data()),
        static_cast<int>(sizeof(source)));
    const QJsonObject buffers{{QStringLiteral("position"), QJsonObject{
        {QStringLiteral("format"), QStringLiteral("float32x3")},
        {QStringLiteral("byteOffset"), 0},
        {QStringLiteral("byteLength"), static_cast<int>(sizeof(source))}}}};
    std::vector<float> decoded;
    QString error;
    Require(slicer::render::DecodeMeshAttribute(
                blob, buffers, QStringLiteral("position"), 3, 1,
                &decoded, &error),
            error);
    Require(decoded == std::vector<float>(source.begin(), source.end()),
            QStringLiteral("float32 decoder compatibility drifted"));
}

int CountColorfulPixels(const slicer::render::ImageOut& image)
{
    int count{0};
    for (std::size_t offset = 0U; offset + 3U < image.rgba8.size(); offset += 4U)
    {
        const auto red = image.rgba8[offset];
        const auto green = image.rgba8[offset + 1U];
        const auto blue = image.rgba8[offset + 2U];
        const int minimum = (std::min)({red, green, blue});
        const int maximum = (std::max)({red, green, blue});
        if (maximum - minimum > 35)
        {
            ++count;
        }
    }
    return count;
}

int CountRedHighlightPixels(const slicer::render::ImageOut& image)
{
    int count{0};
    for (std::size_t offset = 0U; offset + 3U < image.rgba8.size(); offset += 4U)
    {
        if (image.rgba8[offset] > 220U
            && image.rgba8[offset] > image.rgba8[offset + 1U] * 2U
            && image.rgba8[offset] > image.rgba8[offset + 2U] * 2U)
        {
            ++count;
        }
    }
    return count;
}

void Identity(float matrix[16])
{
    std::fill(matrix, matrix + 16, 0.0F);
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0F;
}

double Percentile95(QVector<double> samples)
{
    std::sort(samples.begin(), samples.end());
    const int index = (std::min)(samples.size() - 1,
        static_cast<int>(std::ceil(samples.size() * 0.95)) - 1);
    return samples.at(index);
}

double RunPerformanceGate()
{
#ifndef NDEBUG
    return 0.0;
#else
    constexpr std::uint32_t kColumns{224U};
    constexpr std::uint32_t kRows{224U};
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uv;
    positions.reserve((kColumns + 1U) * (kRows + 1U) * 3U);
    normals.reserve(positions.capacity());
    uv.reserve((kColumns + 1U) * (kRows + 1U) * 2U);
    for (std::uint32_t y = 0U; y <= kRows; ++y)
    {
        for (std::uint32_t x = 0U; x <= kColumns; ++x)
        {
            positions.insert(positions.end(), {
                static_cast<float>(x) * 100.0F / kColumns,
                static_cast<float>(y) * 100.0F / kRows,
                std::sin(static_cast<float>(x + y) * 0.05F) * 0.2F});
            normals.insert(normals.end(), {0.0F, 0.0F, 1.0F});
            uv.insert(uv.end(), {
                static_cast<float>(x) / kColumns,
                static_cast<float>(y) / kRows});
        }
    }
    std::vector<std::uint32_t> indices;
    indices.reserve(kColumns * kRows * 6U);
    for (std::uint32_t y = 0U; y < kRows; ++y)
    {
        for (std::uint32_t x = 0U; x < kColumns; ++x)
        {
            const std::uint32_t a = y * (kColumns + 1U) + x;
            const std::uint32_t b = a + 1U;
            const std::uint32_t c = a + kColumns + 1U;
            const std::uint32_t d = c + 1U;
            indices.insert(indices.end(), {a, b, d, a, d, c});
        }
    }
    CpuRasterBackend backend;
    slicer::render::MeshDesc mesh;
    mesh.meshIdentity = "performance-mesh";
    mesh.vertexCount = static_cast<std::uint32_t>(positions.size() / 3U);
    mesh.triangleCount = static_cast<std::uint32_t>(indices.size() / 3U);
    mesh.position = positions.data();
    mesh.normal = normals.data();
    mesh.texcoord0 = uv.data();
    mesh.index = indices.data();
    mesh.submeshes.push_back({0U,
        static_cast<std::uint32_t>(indices.size()), "performance-material"});
    Require(mesh.triangleCount >= 100000U,
            QStringLiteral("performance fixture is below 100k triangles"));
    Require(backend.UploadMesh(mesh), QStringLiteral("performance mesh upload"));
    const std::array<std::uint8_t, 16> pixels{
        220U, 60U, 60U, 255U, 60U, 210U, 90U, 255U,
        60U, 100U, 220U, 255U, 230U, 210U, 60U, 255U};
    slicer::render::TextureDesc texture{
        "performance-texture", 2U, 2U, pixels.data(), true};
    Require(backend.UploadTexture(texture), QStringLiteral("texture upload"));
    slicer::render::MaterialDesc material;
    material.appearanceIdentity = "performance-appearance";
    material.materialId = "performance-material";
    material.baseColorTextureIdentity = texture.textureIdentity;
    Require(backend.UploadMaterial(material), QStringLiteral("material upload"));
    slicer::render::FrameDesc frame;
    frame.viewportWidthPx = 320U;
    frame.viewportHeightPx = 320U;
    frame.decor.showGrid = false;
    frame.decor.showBuildVolume = false;
    frame.decor.showAxes = false;
    slicer::render::InstanceDraw instance;
    instance.instanceId = "performance-instance";
    instance.meshIdentity = mesh.meshIdentity;
    instance.appearanceIdentity = material.appearanceIdentity;
    instance.localBoundsMm[2] = instance.localBoundsMm[3] = 100.0F;
    Identity(instance.worldMatrix);
    frame.instances.push_back(instance);
    CameraController camera;
    camera.Fit({0.0F, 0.0F, -0.2F, 100.0F, 100.0F, 0.2F}, 320U, 320U);
    QVector<double> frameTimes;
    QElapsedTimer duration;
    duration.start();
    while (duration.elapsed() < 30000)
    {
        camera.Orbit(0.05F, 0.01F);
        frame.camera = camera.BuildCamera();
        QElapsedTimer timer;
        timer.start();
        const auto result = backend.RenderFrame(frame);
        Require(result.ok, QString::fromStdString(result.errorCode));
        frameTimes.push_back(static_cast<double>(timer.nsecsElapsed()) / 1.0e6);
    }
    const double p5Fps = 1000.0 / Percentile95(frameTimes);
    Require(p5Fps >= 30.0,
            QStringLiteral("UI-M8 P5 below 30 FPS: %1").arg(p5Fps));
    return p5Fps;
#endif
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QString modulePath = ArgumentValue(
        application.arguments(), QStringLiteral("--module"));
    Require(!modulePath.isEmpty(), QStringLiteral("--module is required"));
    const QString fixturePath = QDir(QStringLiteral(SLICESOFT_SOURCE_DIR))
        .filePath(QStringLiteral(
            "samples/models/textured/fixtures/policy_textured_small.obj"));
    Require(QFileInfo::exists(fixturePath), QStringLiteral("fixture missing"));
    ModuleClient client;
    QString error;
    Require(client.Open(modulePath, QByteArrayLiteral("{}"), &error), error);
    const QJsonObject imported = ExecuteJson(client, {
        {QStringLiteral("capability"), QStringLiteral("model.import")},
        {QStringLiteral("modelPath"), fixturePath},
        {QStringLiteral("options"), QJsonObject{
             {QStringLiteral("computeBBox"), true},
             {QStringLiteral("extractMaterials"), true}}}});
    Require(imported.value(QStringLiteral("ok")).toBool(),
            QStringLiteral("model import failed"));
    SceneInteractionController controller(client);
    Require(controller.Initialize(BuildScene(fixturePath), &error), error);
    const QJsonObject floatMesh = FirstMesh(QueryViewData(
        client, controller.SceneHandle(), controller.SceneRevision()));
    const QJsonObject halfMesh = FirstMesh(QueryViewData(
        client, controller.SceneHandle(), controller.SceneRevision(),
        QStringLiteral("float16")));
    VerifyFloat32DecoderCompatibility();
    const QJsonObject floatBuffers = floatMesh.value(
        QStringLiteral("buffers")).toObject();
    const QJsonObject halfBuffers = halfMesh.value(
        QStringLiteral("buffers")).toObject();
    Require(floatBuffers.value(QStringLiteral("position")).toObject().value(
                QStringLiteral("format")).toString()
            == QStringLiteral("float32x3"),
            QStringLiteral("missing format must preserve float32 compatibility"));
    Require(halfBuffers.value(QStringLiteral("position")).toObject().value(
                QStringLiteral("format")).toString()
            == QStringLiteral("float16x3")
            && halfBuffers.value(QStringLiteral("normal")).toObject().value(
                QStringLiteral("format")).toString()
                == QStringLiteral("float16x3")
            && halfBuffers.value(QStringLiteral("texcoord0")).toObject().value(
                QStringLiteral("format")).toString()
                == QStringLiteral("float16x2"),
            QStringLiteral("float16 request did not close all attributes"));
    const qint64 floatBytes = static_cast<qint64>(floatMesh.value(
        QStringLiteral("totalBytes")).toDouble());
    const qint64 halfBytes = static_cast<qint64>(halfMesh.value(
        QStringLiteral("totalBytes")).toDouble());
    Require(floatBytes > 0 && halfBytes * 100 <= floatBytes * 60,
            QStringLiteral("float16 mesh blob did not shrink by at least 40%"));
    Require(floatMesh.value(QStringLiteral("meshIdentity")).toString()
            != halfMesh.value(QStringLiteral("meshIdentity")).toString(),
            QStringLiteral("mesh identity omitted attribute encoding"));
    CpuRasterBackend backend;
    SceneRenderPolicy renderer(client, backend);
    ThreeDFrame frame;
    Require(renderer.Refresh(controller.SceneHandle(),
        controller.SceneRevision(), &frame, &error), error);
    Require(frame.descriptor.instances.size() == 1U,
            QStringLiteral("three_d instance count mismatch"));
    Require(renderer.MeshUploadCount() == 1U
            && renderer.TextureUploadCount() >= 1U,
            QStringLiteral("three_d resources did not close"));
    Require(frame.descriptor.decor.buildVolumeMm[0] == 300.0F
            && frame.descriptor.decor.buildVolumeMm[1] == 100.0F,
            QStringLiteral("authoritative build volume was lost"));
    CameraController camera;
    camera.Fit(frame.worldBounds, 640U, 360U);
    slicer::render::ImageOut image;
    Require(renderer.Render(frame, camera, 640U, 360U, &image).ok,
            QStringLiteral("textured three_d frame failed"));
    Require(CountColorfulPixels(image) > 20,
            QStringLiteral("three_d frame lost real texture colors"));
    const quint64 firstReads = renderer.BlobReadCount();
    ThreeDFrame cached;
    Require(renderer.Refresh(controller.SceneHandle(),
        controller.SceneRevision(), &cached, &error), error);
    Require(renderer.BlobReadCount() == firstReads
            && renderer.MeshUploadCount() == 1U,
            QStringLiteral("same identity bypassed three_d cache"));
    client.ResetCallCount();
    const std::array<CameraPreset, 7> presets{
        CameraPreset::Top, CameraPreset::Bottom, CameraPreset::Front,
        CameraPreset::Back, CameraPreset::Left, CameraPreset::Right,
        CameraPreset::Isometric};
    for (int index = 0; index < 120; ++index)
    {
        camera.Orbit(0.5F, index % 2 == 0 ? 0.1F : -0.1F);
        camera.Pan(0.001F, -0.001F);
        camera.ZoomAtCursor(index % 2 == 0 ? 0.01F : -0.01F, 0.2F, -0.1F);
        Require(renderer.Render(cached, camera, 320U, 180U, &image).ok,
                QStringLiteral("local camera render failed"));
    }
    for (const CameraPreset preset : presets)
    {
        camera.SetPreset(preset);
        Require(renderer.Render(cached, camera, 320U, 180U, &image).ok,
                QStringLiteral("camera preset render failed"));
    }
    camera.SetProjection(slicer::render::Projection::Perspective);
    Require(renderer.Render(cached, camera, 320U, 180U, &image).ok,
            QStringLiteral("perspective render failed"));
    Require(client.CallCount() == 0U,
            QStringLiteral("UI-M7 camera crossed the DLL boundary"));
    ThreeDFrame outside = cached;
    outside.descriptor.instances.front().outOfBuildVolume = true;
    slicer::render::ImageOut highlighted;
    Require(renderer.Render(outside, camera, 640U, 360U, &highlighted).ok,
            QStringLiteral("out-of-bounds render failed"));
    Require(CountRedHighlightPixels(highlighted)
                > CountRedHighlightPixels(image),
            QStringLiteral("out-of-bounds highlight was not visible"));
    const double p5Fps = RunPerformanceGate();
    QTextStream(stdout)
        << "14E-04c three_d contract: PASS meshUploads="
        << renderer.MeshUploadCount()
        << " textureUploads=" << renderer.TextureUploadCount()
        << " blobReads=" << renderer.BlobReadCount()
        << " floatBytes=" << floatBytes
        << " halfBytes=" << halfBytes
        << " cameraCalls=" << client.CallCount()
        << " p5Fps=" << p5Fps << Qt::endl;
    return 0;
}
