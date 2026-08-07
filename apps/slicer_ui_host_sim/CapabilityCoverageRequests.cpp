#include "CapabilityCoverageRequests.h"

#include "HostRequestBuilder.h"

#include <QDir>
#include <QFileInfo>
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
    if (fixture->modelid.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("model.import 未返回 modelId。");
        }
        return false;
    }

    if (!BuildProfile(
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
