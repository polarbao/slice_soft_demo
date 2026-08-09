#include "HostTextureWhitePreflightService.h"

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
    return image.save(path, "PNG") ? path : QString{};
}

std::optional<hosttexturewhitepreflightresult> WaitForGeneration(
    HostTextureWhitePreflightService& service,
    const quint64 generation)
{
    std::optional<hosttexturewhitepreflightresult> result;
    QEventLoop loop;
    QObject connectionContext;
    QObject::connect(
        &service,
        &HostTextureWhitePreflightService::SigPreflightFinished,
        &connectionContext,
        [&](const hosttexturewhitepreflightresult& candidate)
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

hosttexturewhitepreflightrequest MakeRequest(
    const QString& texturePath,
    const quint64 revision,
    const bool supportsCarrier = false)
{
    hosttexturewhitepreflightrequest request;
    request.sceneid = QStringLiteral("hostflow-he06-scene");
    request.scenerevision = revision;
    request.contenthash = QStringLiteral("content-%1").arg(revision);
    request.profileid = QStringLiteral("hostflow-he06-profile");
    request.texturepaths = QStringList{texturePath};
    request.profilesupportswhitecarrier = supportsCarrier;
    return request;
}

bool ExactWhiteSemanticsArePreserved(
    const QString& whitePath,
    const QString& nonWhitePath)
{
    HostTextureWhitePreflightService service;
    const auto white = WaitForGeneration(
        service, service.RequestScan(MakeRequest(whitePath, 1U)));
    const auto nonWhite = WaitForGeneration(
        service, service.RequestScan(MakeRequest(nonWhitePath, 2U)));
    const auto supported = WaitForGeneration(
        service, service.RequestScan(MakeRequest(whitePath, 3U, true)));
    return ExpectTrue(white.has_value(), "white scan must finish")
        && ExpectTrue(white->containsstrictwhite, "strict white must be found")
        && ExpectTrue(white->HasWarning(), "unsupported Profile must warn")
        && ExpectTrue(!white->blocking, "preflight must remain non-blocking")
        && ExpectTrue(nonWhite.has_value(), "non-white scan must finish")
        && ExpectTrue(!nonWhite->HasWarning(), "RGB 254 must not warn")
        && ExpectTrue(supported.has_value(), "carrier scan must finish")
        && ExpectTrue(
            supported->containsstrictwhite && !supported->HasWarning(),
            "carrier support must suppress only the warning");
}

bool CacheAndIdentityDiscardAreStable(const QString& whitePath)
{
    HostTextureWhitePreflightService service;
    int discardedCount{0};
    QObject::connect(
        &service,
        &HostTextureWhitePreflightService::SigPreflightDiscarded,
        &service,
        [&](const quint64)
        {
            ++discardedCount;
        });
    service.RequestScan(MakeRequest(whitePath, 4U));
    const quint64 latestGeneration = service.RequestScan(
        MakeRequest(whitePath, 5U));
    const auto latest = WaitForGeneration(service, latestGeneration);
    QCoreApplication::processEvents();
    const hosttexturewhitecachediagnostics afterSingleFlight =
        service.CacheDiagnostics();
    if (!ExpectTrue(latest.has_value(), "latest identity must finish")
        || !ExpectTrue(latest->scenerevision == 5U, "latest revision must win")
        || !ExpectTrue(discardedCount >= 1, "stale result must be discarded")
        || !ExpectTrue(
            afterSingleFlight.decodecount == 1U,
            "single-flight must decode once"))
    {
        return false;
    }

    const auto cached = WaitForGeneration(
        service, service.RequestScan(MakeRequest(whitePath, 6U)));
    const hosttexturewhitecachediagnostics afterCache =
        service.CacheDiagnostics();
    return ExpectTrue(cached.has_value(), "cached scan must finish")
        && ExpectTrue(
            !cached->assets.isEmpty() && cached->assets.front().cachehit,
            "repeated source identity must hit cache")
        && ExpectTrue(afterCache.decodecount == 1U, "cache must avoid decode")
        && ExpectTrue(afterCache.cacheentries == 1, "cache must be stable");
}

bool MissingTextureRemainsNonBlocking(const QString& missingPath)
{
    HostTextureWhitePreflightService service;
    const auto result = WaitForGeneration(
        service, service.RequestScan(MakeRequest(missingPath, 7U)));
    return ExpectTrue(result.has_value(), "missing texture scan must finish")
        && ExpectTrue(
            !result->assets.isEmpty() && !result->assets.front().error.isEmpty(),
            "missing texture must retain diagnostic evidence")
        && ExpectTrue(!result->blocking, "missing texture preflight must not block");
}
}

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
        directory.path(), QStringLiteral("white.png"), QColor(255, 255, 255));
    const QString nonWhitePath = WriteTexture(
        directory.path(), QStringLiteral("non_white.png"), QColor(254, 255, 255));
    if (whitePath.isEmpty() || nonWhitePath.isEmpty())
    {
        std::cerr << "FAIL: unable to create texture fixtures\n";
        return 1;
    }

    const bool passed = ExactWhiteSemanticsArePreserved(
        whitePath, nonWhitePath)
        && CacheAndIdentityDiscardAreStable(whitePath)
        && MissingTextureRemainsNonBlocking(
            QDir(directory.path()).filePath(QStringLiteral("missing.png")));
    if (!passed)
    {
        return 1;
    }
    std::cout << "hostflow_he06_texture_white_preflight_tests: PASS\n";
    return 0;
}
