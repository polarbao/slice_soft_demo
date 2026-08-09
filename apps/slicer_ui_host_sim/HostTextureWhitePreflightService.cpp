#include "HostTextureWhitePreflightService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>
#include <QSet>
#include <QThreadPool>

#include <functional>
#include <future>
#include <utility>

namespace
{
class FunctionRunnable final : public QRunnable
{
public:
    explicit FunctionRunnable(std::function<void()> task)
        : m_task(std::move(task))
    {
    }

    void run() override
    {
        m_task();
    }

private:
    std::function<void()> m_task;
};

QString NormalizeTexturePath(const QString& texturePath)
{
    QFileInfo info(texturePath);
    QString normalized = info.canonicalFilePath();
    if (normalized.isEmpty())
    {
        normalized = info.absoluteFilePath();
    }
    normalized = QDir::fromNativeSeparators(QDir::cleanPath(normalized));
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

QString BuildCacheKey(
    const QString& normalizedPath,
    const qint64 fileSize,
    const qint64 modifiedMsecs,
    const QString& contentHash)
{
    return normalizedPath
        + QStringLiteral("|") + QString::number(fileSize)
        + QStringLiteral("|") + QString::number(modifiedMsecs)
        + QStringLiteral("|") + contentHash;
}

bool ContainsStrictWhite(const QImage& source)
{
    const QImage image = source.convertToFormat(QImage::Format_RGBA8888);
    for (int y{0}; y < image.height(); ++y)
    {
        const uchar* row = image.constScanLine(y);
        for (int x{0}; x < image.width(); ++x)
        {
            const int offset = x * 4;
            if (row[offset] == 255U
                && row[offset + 1] == 255U
                && row[offset + 2] == 255U)
            {
                return true;
            }
        }
    }
    return false;
}
}

struct HostTextureWhitePreflightService::CallbackState
{
    QMutex mutex;
    QPointer<HostTextureWhitePreflightService> service;
};

struct HostTextureWhitePreflightService::CacheState
{
    QMutex mutex;
    QHash<QString, hosttexturewhiteassetresult> cache;
    QHash<QString, std::shared_future<hosttexturewhiteassetresult>> inflight;
    quint64 decodecount{0U};
};

bool hosttexturewhitepreflightresult::HasWarning() const
{
    return shouldwarn && !warningmessage.isEmpty();
}

HostTextureWhitePreflightService::HostTextureWhitePreflightService(
    QObject* parent)
    : QObject(parent),
      m_callbackState(std::make_shared<CallbackState>()),
      m_cacheState(std::make_shared<CacheState>())
{
    m_callbackState->service = this;
}

HostTextureWhitePreflightService::~HostTextureWhitePreflightService()
{
    QMutexLocker lock(&m_callbackState->mutex);
    m_callbackState->service = nullptr;
}

quint64 HostTextureWhitePreflightService::RequestScan(
    const hosttexturewhitepreflightrequest& request)
{
    ++m_generation;
    m_sceneId = request.sceneid;
    m_sceneRevision = request.scenerevision;
    m_contentHash = request.contenthash;
    m_profileId = request.profileid;
    m_running = true;
    const quint64 generation = m_generation;
    emit SigPreflightStarted(generation);

    const std::shared_ptr<CallbackState> callbackState = m_callbackState;
    const std::shared_ptr<CacheState> cacheState = m_cacheState;
    auto* runnable = new FunctionRunnable(
        [callbackState, cacheState, generation, request]()
        {
            hosttexturewhitepreflightresult result;
            result.generation = generation;
            result.sceneid = request.sceneid;
            result.scenerevision = request.scenerevision;
            result.contenthash = request.contenthash;
            result.profileid = request.profileid;
            result.replacementprofileid = request.replacementprofileid;
            result.replacementprofiledisplayname =
                request.replacementprofiledisplayname;
            result.profilesupportswhitecarrier =
                request.profilesupportswhitecarrier;

            QSet<QString> scannedPaths;
            for (const QString& texturePath : request.texturepaths)
            {
                const QString normalizedPath =
                    NormalizeTexturePath(texturePath);
                if (normalizedPath.isEmpty()
                    || scannedPaths.contains(normalizedPath))
                {
                    continue;
                }
                scannedPaths.insert(normalizedPath);
                hosttexturewhiteassetresult asset = ScanAsset(
                    cacheState, texturePath);
                result.containsstrictwhite = result.containsstrictwhite
                    || asset.containsstrictwhite;
                result.assets.push_back(std::move(asset));
            }

            result.shouldwarn = result.containsstrictwhite
                && !result.profilesupportswhitecarrier;
            if (result.shouldwarn)
            {
                result.warningmessage = QStringLiteral(
                    "源贴图包含纯白 RGB(255,255,255)，当前 Profile "
                    "不具备按需补白能力，black_is_print 下这些模型像素可能"
                    "无法形成可打印材料。该判断扫描整张源贴图，未被 UV 使用的"
                    "像素也可能触发保守告警；建议改用「%1」(%2)。")
                    .arg(result.replacementprofiledisplayname,
                         result.replacementprofileid);
            }

            QMutexLocker lock(&callbackState->mutex);
            HostTextureWhitePreflightService* service =
                callbackState->service.data();
            if (service == nullptr)
            {
                return;
            }
            QMetaObject::invokeMethod(
                service,
                [service, result = std::move(result)]() mutable
                {
                    service->OnWorkerCompleted(std::move(result));
                },
                Qt::QueuedConnection);
        });
    QThreadPool::globalInstance()->start(runnable);
    return generation;
}

void HostTextureWhitePreflightService::Cancel()
{
    const quint64 discardedGeneration = m_generation;
    ++m_generation;
    m_running = false;
    emit SigPreflightDiscarded(discardedGeneration);
}

bool HostTextureWhitePreflightService::IsRunning() const
{
    return m_running;
}

hosttexturewhitecachediagnostics
HostTextureWhitePreflightService::CacheDiagnostics() const
{
    QMutexLocker lock(&m_cacheState->mutex);
    hosttexturewhitecachediagnostics diagnostics;
    diagnostics.cacheentries = m_cacheState->cache.size();
    diagnostics.inflightentries = m_cacheState->inflight.size();
    diagnostics.decodecount = m_cacheState->decodecount;
    return diagnostics;
}

hosttexturewhiteassetresult HostTextureWhitePreflightService::ScanAsset(
    const std::shared_ptr<CacheState>& cacheState,
    const QString& texturePath)
{
    hosttexturewhiteassetresult result;
    const QFileInfo info(texturePath);
    result.normalizedpath = NormalizeTexturePath(texturePath);
    if (!info.exists() || !info.isFile())
    {
        result.error = QStringLiteral("纹理文件不存在或不是普通文件：")
            + texturePath;
        return result;
    }

    result.filesize = info.size();
    result.modifiedmsecs = info.lastModified().toMSecsSinceEpoch();
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        result.error = QStringLiteral("无法读取纹理文件：")
            + info.absoluteFilePath();
        return result;
    }
    const QByteArray bytes = file.readAll();
    result.contenthash = QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    const QString cacheKey = BuildCacheKey(
        result.normalizedpath,
        result.filesize,
        result.modifiedmsecs,
        result.contenthash);

    std::shared_future<hosttexturewhiteassetresult> sharedResult;
    std::shared_ptr<std::promise<hosttexturewhiteassetresult>> promise;
    {
        QMutexLocker lock(&cacheState->mutex);
        const auto cached = cacheState->cache.constFind(cacheKey);
        if (cached != cacheState->cache.cend())
        {
            result = cached.value();
            result.cachehit = true;
            return result;
        }
        const auto inFlight = cacheState->inflight.constFind(cacheKey);
        if (inFlight != cacheState->inflight.cend())
        {
            sharedResult = inFlight.value();
        }
        else
        {
            promise = std::make_shared<
                std::promise<hosttexturewhiteassetresult>>();
            sharedResult = promise->get_future().share();
            cacheState->inflight.insert(cacheKey, sharedResult);
        }
    }

    if (!promise)
    {
        result = sharedResult.get();
        result.cachehit = true;
        return result;
    }

    const QImage image = QImage::fromData(bytes);
    if (image.isNull())
    {
        result.error = QStringLiteral("无法解码纹理文件：")
            + info.absoluteFilePath();
    }
    else
    {
        result.containsstrictwhite = ContainsStrictWhite(image);
    }

    {
        QMutexLocker lock(&cacheState->mutex);
        ++cacheState->decodecount;
        cacheState->cache.insert(cacheKey, result);
        cacheState->inflight.remove(cacheKey);
    }
    promise->set_value(result);
    return result;
}

void HostTextureWhitePreflightService::OnWorkerCompleted(
    hosttexturewhitepreflightresult result)
{
    if (result.generation != m_generation
        || result.sceneid != m_sceneId
        || result.scenerevision != m_sceneRevision
        || result.contenthash != m_contentHash
        || result.profileid != m_profileId)
    {
        emit SigPreflightDiscarded(result.generation);
        return;
    }
    m_running = false;
    emit SigPreflightFinished(result);
}
