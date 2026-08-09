#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

/** @brief Identity and Profile context for one host texture-white scan. */
struct hosttexturewhitepreflightrequest
{
    QString sceneid;
    quint64 scenerevision{0U};
    QString contenthash;
    QString profileid;
    QStringList texturepaths;
    bool profilesupportswhitecarrier{false};
    QString replacementprofileid{
        QStringLiteral("textured_nail_rgb_white_ondemand_lower_support")};
    QString replacementprofiledisplayname{
        QStringLiteral("彩色纹理甲片 - 全实体 RGB + 按需补白 + 下表面支撑")};
};

/** @brief Cached exact-white evidence for one source texture. */
struct hosttexturewhiteassetresult
{
    QString normalizedpath;
    qint64 filesize{0};
    qint64 modifiedmsecs{0};
    QString contenthash;
    bool containsstrictwhite{false};
    bool cachehit{false};
    QString error;
};

/** @brief Non-blocking preflight result bound to one scene/Profile identity. */
struct hosttexturewhitepreflightresult
{
    quint64 generation{0U};
    QString sceneid;
    quint64 scenerevision{0U};
    QString contenthash;
    QString profileid;
    QVector<hosttexturewhiteassetresult> assets;
    QString replacementprofileid;
    QString replacementprofiledisplayname;
    QString warningmessage;
    bool containsstrictwhite{false};
    bool profilesupportswhitecarrier{false};
    bool conservative{true};
    bool shouldwarn{false};
    bool blocking{false};

    /**
     * @brief Reports whether this result contains a user-visible warning.
     * @return True only for exact-white evidence without carrier support.
     */
    [[nodiscard]] bool HasWarning() const;
};

/** @brief Cache counters exposed for deterministic host tests. */
struct hosttexturewhitecachediagnostics
{
    qsizetype cacheentries{0};
    qsizetype inflightentries{0};
    quint64 decodecount{0U};
};

/**
 * @brief Asynchronously scans imported source textures for exact RGB white.
 *
 * The service mirrors the frozen Stage 15 strict-white semantics. It only
 * emits conservative guidance and never blocks slicing or mutates a Profile.
 */
class HostTextureWhitePreflightService final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates a host-owned white preflight service.
     * @param parent Optional QObject owner.
     */
    explicit HostTextureWhitePreflightService(QObject* parent = nullptr);
    ~HostTextureWhitePreflightService() override;

    /**
     * @brief Starts a scan for the supplied scene and Profile identity.
     * @param request Immutable source texture and identity context.
     * @return Monotonic request generation.
     */
    quint64 RequestScan(const hosttexturewhitepreflightrequest& request);

    /** @brief Cancels logical delivery of the newest request. */
    void Cancel();

    /**
     * @brief Reports whether the newest request is still running.
     * @return True until its result is accepted or cancelled.
     */
    [[nodiscard]] bool IsRunning() const;

    /**
     * @brief Returns cache and decode counters.
     * @return Thread-safe diagnostic snapshot.
     */
    [[nodiscard]] hosttexturewhitecachediagnostics CacheDiagnostics() const;

signals:
    /** @brief Emitted when a new preflight generation starts. */
    void SigPreflightStarted(quint64 generation);

    /** @brief Emitted only for the newest matching scene/Profile identity. */
    void SigPreflightFinished(const hosttexturewhitepreflightresult& result);

    /** @brief Emitted when a stale or cancelled generation is discarded. */
    void SigPreflightDiscarded(quint64 generation);

private:
    struct CallbackState;
    struct CacheState;

    static hosttexturewhiteassetresult ScanAsset(
        const std::shared_ptr<CacheState>& cacheState,
        const QString& texturePath);
    void OnWorkerCompleted(hosttexturewhitepreflightresult result);

    std::shared_ptr<CallbackState> m_callbackState;
    std::shared_ptr<CacheState> m_cacheState;
    quint64 m_generation{0U};
    QString m_sceneId;
    quint64 m_sceneRevision{0U};
    QString m_contentHash;
    QString m_profileId;
    bool m_running{false};
};
