#include "TextureWhitePreflightService.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QImage>
#include <QObject>
#include <QTemporaryDir>
#include <QTimer>

#include <iostream>
#include <optional>

namespace
{

bool ExpectTrue(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

QString WriteTexture(
    const QString& directory,
    const QString& name,
    const QColor& color)
{
    QImage image(32, 32, QImage::Format_RGBA8888);
    image.fill(color);
    const QString path = QDir(directory).filePath(name);
    if (!image.save(path, "PNG"))
    {
        return {};
    }
    return path;
}

std::optional<TextureWhitePreflightResult> WaitForGeneration(
    TextureWhitePreflightService& service,
    const quint64 generation)
{
    std::optional<TextureWhitePreflightResult> result;
    QEventLoop loop;
    QObject connectionContext;
    QObject::connect(
        &service,
        &TextureWhitePreflightService::SigPreflightFinished,
        &connectionContext,
        [&](const TextureWhitePreflightResult& candidate)
        {
            if (candidate.generation == generation)
            {
                result = candidate;
                loop.quit();
            }
        });
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();
    return result;
}

TextureWhitePreflightRequest MakeRequest(
    const QString& texturePath,
    const quint64 revision,
    const QStringList& capabilities = {})
{
    TextureWhitePreflightRequest request;
    request.sceneid = QStringLiteral("stage15-preflight-scene");
    request.scenerevision = revision;
    request.contenthash = QStringLiteral("content-%1").arg(revision);
    request.texturepaths = QStringList{texturePath};
    request.profilecapabilities = capabilities;
    return request;
}

bool PureWhiteWithoutCapabilityWarns(const QString& whitePath)
{
    TextureWhitePreflightService service;
    const quint64 generation = service.RequestScan(
        MakeRequest(whitePath, 1U));
    const auto result = WaitForGeneration(service, generation);
    return ExpectTrue(result.has_value(), "white scan must finish")
        && ExpectTrue(result->containsstrictwhite, "strict white must be found")
        && ExpectTrue(result->HasWarning(), "unsupported Profile must warn")
        && ExpectTrue(result->conservative, "source scan must be conservative")
        && ExpectTrue(!result->blocking, "source scan must not block slicing")
        && ExpectTrue(
            result->warningmessage.contains(
                QStringLiteral("textured_nail_rgb_white_ondemand_lower_support")),
            "warning must identify the replacement Profile");
}

bool NonWhiteTextureDoesNotWarn(const QString& nonWhitePath)
{
    TextureWhitePreflightService service;
    const quint64 generation = service.RequestScan(
        MakeRequest(nonWhitePath, 2U));
    const auto result = WaitForGeneration(service, generation);
    return ExpectTrue(result.has_value(), "non-white scan must finish")
        && ExpectTrue(
            !result->containsstrictwhite,
            "non-white texture must not contain strict white")
        && ExpectTrue(!result->HasWarning(), "non-white texture must not warn");
}

bool CapableProfileDoesNotWarn(const QString& whitePath)
{
    TextureWhitePreflightService service;
    const quint64 generation = service.RequestScan(
        MakeRequest(
            whitePath,
            3U,
            {QStringLiteral("unprintable_white_underbase")}));
    const auto result = WaitForGeneration(service, generation);
    return ExpectTrue(result.has_value(), "capable Profile scan must finish")
        && ExpectTrue(result->containsstrictwhite, "white evidence must remain")
        && ExpectTrue(
            result->profilesupportswhitecarrier,
            "capability must be recognized")
        && ExpectTrue(!result->HasWarning(), "capable Profile must not warn");
}

bool CacheAndLatestIdentityAreStable(const QString& whitePath)
{
    TextureWhitePreflightService service;
    int discardedCount{0};
    QObject::connect(
        &service,
        &TextureWhitePreflightService::SigPreflightDiscarded,
        &service,
        [&](const quint64)
        {
            ++discardedCount;
        });

    const quint64 firstGeneration = service.RequestScan(
        MakeRequest(whitePath, 4U));
    const quint64 secondGeneration = service.RequestScan(
        MakeRequest(whitePath, 5U));
    Q_UNUSED(firstGeneration);
    const auto latest = WaitForGeneration(service, secondGeneration);
    QCoreApplication::processEvents();
    const TextureWhitePreflightCacheDiagnostics afterSingleFlight =
        service.CacheDiagnostics();
    if (!ExpectTrue(latest.has_value(), "latest request must finish")
        || !ExpectTrue(
            latest->scenerevision == 5U,
            "latest scene revision must win")
        || !ExpectTrue(discardedCount >= 1, "stale generation must be discarded")
        || !ExpectTrue(
            afterSingleFlight.decodecount == 1U,
            "concurrent duplicate must decode once"))
    {
        return false;
    }

    const quint64 cachedGeneration = service.RequestScan(
        MakeRequest(whitePath, 6U));
    const auto cached = WaitForGeneration(service, cachedGeneration);
    const TextureWhitePreflightCacheDiagnostics afterCache =
        service.CacheDiagnostics();
    return ExpectTrue(cached.has_value(), "cached request must finish")
        && ExpectTrue(
            !cached->assets.isEmpty() && cached->assets.front().cachehit,
            "repeated request must report a cache hit")
        && ExpectTrue(
            afterCache.decodecount == 1U,
            "cache hit must not decode again")
        && ExpectTrue(
            afterCache.cacheentries == 1,
            "exact identity cache must contain one entry");
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid())
    {
        std::cerr << "FAIL: unable to create temporary directory\n";
        return 1;
    }
    const QString whitePath = WriteTexture(
        directory.path(),
        QStringLiteral("white.png"),
        QColor(255, 255, 255));
    const QString nonWhitePath = WriteTexture(
        directory.path(),
        QStringLiteral("non_white.png"),
        QColor(254, 255, 255));
    if (whitePath.isEmpty() || nonWhitePath.isEmpty())
    {
        std::cerr << "FAIL: unable to write texture fixtures\n";
        return 1;
    }

    const bool passed = PureWhiteWithoutCapabilityWarns(whitePath)
        && NonWhiteTextureDoesNotWarn(nonWhitePath)
        && CapableProfileDoesNotWarn(whitePath)
        && CacheAndLatestIdentityAreStable(whitePath);
    if (!passed)
    {
        return 1;
    }
    std::cout << "texture_white_preflight_service_unit_tests: PASS\n";
    return 0;
}
