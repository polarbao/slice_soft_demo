#include "CapabilityCoverageRequests.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

#include <cstdlib>

namespace
{
QString NormalizePath(const QString& path)
{
    return QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
}

bool ParseObject(
    const char* text,
    QJsonObject* object,
    QString* error)
{
    if (text == nullptr || object == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("宿主请求构造器未返回 JSON 对象。");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray{text},
        &parseError);
    if (!document.isObject())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("宿主请求 JSON 无效：%1")
                         .arg(parseError.errorString());
        }
        return false;
    }
    *object = document.object();
    return true;
}
}

bool CapabilityCoverageRequests::InitializePaths(
    const QString& repositoryRoot,
    const QString& evidenceRoot,
    capabilitycoveragefixture* fixture,
    QString* error)
{
    if (fixture == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("能力覆盖 fixture 不能为空。");
        }
        return false;
    }

    const QDir repository(repositoryRoot);
    fixture->modelpath = NormalizePath(repository.filePath(QStringLiteral(
        "samples/models/openvdb/surface_shell_cube_no_uv.obj")));
    fixture->resourceroot = NormalizePath(
        QFileInfo(fixture->modelpath).absolutePath());
    fixture->packagedirectory = NormalizePath(
        QDir(evidenceRoot).filePath(QStringLiteral("package")));
    fixture->previewpath = NormalizePath(
        QDir(evidenceRoot).filePath(QStringLiteral("layer_000000_preview.ppm")));

    if (!QFileInfo(fixture->modelpath).isFile())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("14E-04b 参考模型不存在：%1")
                         .arg(fixture->modelpath);
        }
        return false;
    }
    if (!QDir().mkpath(evidenceRoot))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法创建能力覆盖证据目录：%1")
                         .arg(evidenceRoot);
        }
        return false;
    }
    return true;
}

bool CapabilityCoverageRequests::BindImportedModel(
    const QJsonObject& imported,
    capabilitycoveragefixture* fixture,
    QString* error)
{
    if (fixture == nullptr || !imported.value(QStringLiteral("ok")).toBool())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("model.import 未返回成功结果。");
        }
        return false;
    }

    fixture->modelid = imported.value(QStringLiteral("modelId")).toString();
    fixture->sourcedigest = imported.value(
        QStringLiteral("sourceDigest")).toString();
    if (fixture->modelid.isEmpty() || fixture->sourcedigest.isEmpty()
        || !ParseBounds(imported, &fixture->sourcebounds, error))
    {
        return false;
    }

    fixture->effectivebounds = fixture->sourcebounds;
    fixture->effectivebounds.min[0] += 1.0;
    fixture->effectivebounds.max[0] += 1.0;
    fixture->effectivebounds.min[1] += 2.0;
    fixture->effectivebounds.max[1] += 2.0;

    char resourceDigest[65]{};
    const QByteArray modelPath = fixture->modelpath.toUtf8();
    if (!HostComputeUntexturedObjResourceDigest(
            modelPath.constData(),
            resourceDigest,
            sizeof(resourceDigest)))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法计算参考模型资源身份。");
        }
        return false;
    }
    fixture->resourcedigest = QString::fromLatin1(resourceDigest);

    if (!BuildScene(*fixture, false, &fixture->initialscene, error)
        || !BuildScene(*fixture, true, &fixture->committedscene, error)
        || !BuildProfile(
            *fixture,
            0.2,
            &fixture->profile,
            &fixture->profilehash,
            error))
    {
        return false;
    }
    return true;
}

bool CapabilityCoverageRequests::BuildProfile(
    const capabilitycoveragefixture& fixture,
    const double layerThicknessMm,
    QJsonObject* profile,
    QString* profileHash,
    QString* error)
{
    if (profile == nullptr || profileHash == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Profile 输出不能为空。");
        }
        return false;
    }

    char hash[72]{};
    const QByteArray modelPath = fixture.modelpath.toUtf8();
    const QByteArray packageDirectory = fixture.packagedirectory.toUtf8();
    char* profileText = HostBuildProfileWithLayerThickness(
        modelPath.constData(),
        packageDirectory.constData(),
        layerThicknessMm,
        hash,
        sizeof(hash));
    const bool parsed = ParseObject(profileText, profile, error);
    std::free(profileText);
    if (!parsed)
    {
        return false;
    }
    *profileHash = QString::fromLatin1(hash);
    return true;
}

bool CapabilityCoverageRequests::BuildScene(
    const capabilitycoveragefixture& fixture,
    const bool committed,
    QJsonObject* scene,
    QString* error)
{
    const QByteArray modelPath = fixture.modelpath.toUtf8();
    const QByteArray resourceRoot = fixture.resourceroot.toUtf8();
    const QByteArray sourceDigest = fixture.sourcedigest.toUtf8();
    const QByteArray resourceDigest = fixture.resourcedigest.toUtf8();
    char* sceneText = HostBuildScene(
        modelPath.constData(),
        resourceRoot.constData(),
        sourceDigest.constData(),
        resourceDigest.constData(),
        &fixture.sourcebounds,
        committed ? &fixture.effectivebounds : &fixture.sourcebounds,
        committed ? 1U : 0U,
        committed ? 1.0 : 0.0,
        committed ? 2.0 : 0.0,
        committed ? 1U : 0U);
    const bool parsed = ParseObject(sceneText, scene, error);
    std::free(sceneText);
    return parsed;
}

bool CapabilityCoverageRequests::ParseBounds(
    const QJsonObject& imported,
    HostBounds3* bounds,
    QString* error)
{
    const QJsonObject bbox = imported.value(QStringLiteral("bboxMm")).toObject();
    const QJsonArray minimum = bbox.value(QStringLiteral("min")).toArray();
    const QJsonArray maximum = bbox.value(QStringLiteral("max")).toArray();
    if (bounds == nullptr || minimum.size() != 3 || maximum.size() != 3)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("model.import bboxMm 不是三维边界。");
        }
        return false;
    }
    for (int index = 0; index < 3; ++index)
    {
        bounds->min[index] = minimum.at(index).toDouble();
        bounds->max[index] = maximum.at(index).toDouble();
    }
    return true;
}
