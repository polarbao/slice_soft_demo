#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

/** @brief 单次宿主纹理纯白扫描的标识与 Profile 上下文。 */
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

/** @brief 缓存单个源纹理的严格纯白证据。 */
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

/** @brief 非阻塞预检结果绑定到一个场景/Profile 标识。 */
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
     * @brief 报告此结果是否包含用户可见的警告。
     * @return 仅适用于没有载体支持的纯白证据。
     */
    [[nodiscard]] bool HasWarning() const;
};

/** @brief 为确定性宿主测试公开的缓存计数器。 */
struct hosttexturewhitecachediagnostics
{
    qsizetype cacheentries{0};
    qsizetype inflightentries{0};
    quint64 decodecount{0U};
};

/**
 * @brief 异步扫描导入的源纹理，查找严格 RGB 纯白像素。
 *
 * 该服务遵循冻结的 Stage 15 严格纯白语义，只发出保守提示，
 * 绝不阻断切片或改变 Profile。
 */
class HostTextureWhitePreflightService final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 创建由宿主持有的白色预检服务。
     * @param parent 可选的 QObject 所有者。
     */
    explicit HostTextureWhitePreflightService(QObject* parent = nullptr);
    ~HostTextureWhitePreflightService() override;

    /**
     * @brief 开始扫描指定的场景与 Profile 标识。
     * @param request 不可变的源纹理和标识上下文。
     * @return 单调递增的请求代次。
     */
    quint64 RequestScan(const hosttexturewhitepreflightrequest& request);

    /** @brief 取消最新请求的逻辑交付。 */
    void Cancel();

    /**
     * @brief 报告最新的请求是否仍在运行。
     * @return 结果被接受或请求被取消前返回 true。
     */
    [[nodiscard]] bool IsRunning() const;

    /**
     * @brief 返回缓存和解码计数器。
     * @return 线程安全的诊断快照。
     */
    [[nodiscard]] hosttexturewhitecachediagnostics CacheDiagnostics() const;

signals:
    /** @brief 新的预检代次开始时发出。 */
    void SigPreflightStarted(quint64 generation);

    /** @brief 仅针对最新匹配的场景/Profile 标识发出。 */
    void SigPreflightFinished(const hosttexturewhitepreflightresult& result);

    /** @brief 过期或已取消的代次被丢弃时发出。 */
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
