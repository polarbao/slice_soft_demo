#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

/**
 * @brief Identity and Profile context for one asynchronous texture scan.
 */
struct TextureWhitePreflightRequest
{
    QString sceneid;
    quint64 scenerevision{0U};
    QString contenthash;
    QStringList texturepaths;
    QStringList profilecapabilities;
    QString replacementprofileid{
        QStringLiteral("textured_nail_rgb_white_ondemand_lower_support")};
    QString replacementprofiledisplayname{
        QStringLiteral("彩色纹理甲片 - 全实体 RGB + 按需补白 + 下表面支撑")};
};

/**
 * @brief Cached evidence for one source texture asset.
 */
struct TextureWhiteAssetScanResult
{
    QString normalizedpath;
    qint64 filesize{0};
    qint64 modifiedmsecs{0};
    QString contenthash;
    bool containsstrictwhite{false};
    bool cachehit{false};
    QString error;
};

/**
 * @brief Result bound to the scene identity that requested the scan.
 */
struct TextureWhitePreflightResult
{
    quint64 generation{0U};
    QString sceneid;
    quint64 scenerevision{0U};
    QString contenthash;
    QVector<TextureWhiteAssetScanResult> assets;
    QString replacementprofileid;
    QString replacementprofiledisplayname;
    QString warningmessage;
    bool containsstrictwhite{false};
    bool profilesupportswhitecarrier{false};
    bool conservative{true};
    bool shouldwarn{false};
    bool blocking{false};

    /**
     * @brief Report whether the result contains a non-blocking warning.
     * @return True only for pure-white texture evidence without Profile support.
     */
    bool HasWarning() const;
};

/**
 * @brief Observable cache counters used by diagnostics and deterministic tests.
 */
struct TextureWhitePreflightCacheDiagnostics
{
    qsizetype cacheentries{0};
    qsizetype inflightentries{0};
    quint64 decodecount{0U};
};

/**
 * @brief Asynchronously scan texture assets for exact RGB white texels.
 *
 * The service never blocks slicing. Results are conservative because source
 * textures may contain white texels that are not referenced by model UVs.
 */
class TextureWhitePreflightService final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Create a texture-white preflight service.
     * @param parent QObject owner.
     */
    explicit TextureWhitePreflightService(QObject* parent = nullptr);
    ~TextureWhitePreflightService() override;

    /**
     * @brief Start a scan for the supplied scene identity.
     * @param request Scene, content, texture, and Profile capability context.
     * @return Monotonic request generation.
     */
    quint64 RequestScan(const TextureWhitePreflightRequest& request);

    /**
     * @brief Cancel logical delivery of the newest request.
     */
    void Cancel();

    /**
     * @brief Report whether the newest request is running.
     * @return True until the newest worker result is accepted or cancelled.
     */
    bool IsRunning() const;

    /**
     * @brief Return the newest request generation.
     * @return Monotonic generation value.
     */
    quint64 Generation() const;

    /**
     * @brief Return cache and decode counters.
     * @return Snapshot protected by the shared cache lock.
     */
    TextureWhitePreflightCacheDiagnostics CacheDiagnostics() const;

signals:
    void SigPreflightStarted(quint64 generation);
    void SigPreflightFinished(const TextureWhitePreflightResult& result);
    void SigPreflightDiscarded(quint64 generation);

private:
    struct CallbackState;
    struct CacheState;

    static TextureWhiteAssetScanResult ScanAsset(
        const std::shared_ptr<CacheState>& cacheState,
        const QString& texturePath);
    void OnWorkerCompleted(TextureWhitePreflightResult result);

    std::shared_ptr<CallbackState> m_callbackState;
    std::shared_ptr<CacheState> m_cacheState;
    quint64 m_generation{0U};
    QString m_sceneId;
    quint64 m_sceneRevision{0U};
    QString m_contentHash;
    bool m_running{false};
};
