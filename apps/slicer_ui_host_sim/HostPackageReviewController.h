#pragma once

#include "ModuleClient.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>

class QTemporaryDir;

/** @brief 以冻结 RGBWSVT 超集顺序保存打印像素计数。 */
struct hostchannelcounts
{
    std::array<quint64, 7> values{};
};

/** @brief 宿主持有的单个生产层描述符视图。 */
struct hostlayerdescriptor
{
    int layerindex{0};
    double zmm{0.0};
    int widthpx{0};
    int heightpx{0};
    hostchannelcounts printpixels;
    hostchannelcounts emptypixels;
    QString storagemode;
    QString tiffpath;
};

/** @brief 参考宿主 UI 使用的已验证包摘要。 */
struct hostpackagereview
{
    QString packagedirectory;
    QString packageidentity;
    QString schema;
    QString productionacceptance;
    QString polarity;
    QStringList channels;
    QStringList verificationerrors;
    int layercount{0};
    int widthpx{0};
    int heightpx{0};
    int dpix{0};
    int dpiy{0};
    int bitdepth{0};
    int instancecount{0};
    QString profileversion;
    QString profilehash;
    bool valid{false};
    QVector<hostlayerdescriptor> layers;
};

/** @brief 通过 package.read_report 返回的命名报告。 */
struct hostpackagereport
{
    QString name;
    QString schema;
    QString sourcepath;
    QJsonObject data;
};

/**
 * @brief 仅通过公共 SPI 调用读取生产包结果。
 */
class HostPackageReviewController final : public QObject
{
public:
    using LoadCallback = std::function<void(bool, const QString&)>;

    /**
     * @brief 为已加载模块创建包审查控制器。
     * @param client 公共模块 ABI 客户端。
     */
    explicit HostPackageReviewController(ModuleClient& client);

    /** @brief 释放由宿主持有的预览缓存目录。 */
    ~HostPackageReviewController();

    /**
     * @brief 验证并加载摘要以及所有层描述符。
     * @param packageDirectory 已发布的 RGBWSV Package 目录。
     * @param error 接收失败即拒绝的验证原因。
     * @return 当包和冻结协议字段有效时为 true。
     */
    bool Load(const QString& packageDirectory, QString* error);

    /**
     * @brief 验证并加载包而不阻塞 Qt UI 线程。
     * @param packageDirectory 已发布的 RGBWSV Package 目录。
     * @param callback 加载结束后在此控制器的 Qt 线程上运行。
     * @param error 接收本地提交错误。
     * @return 已接受后台加载任务时返回 true。
     */
    bool LoadAsync(
        const QString& packageDirectory,
        LoadCallback callback,
        QString* error);

    /** @brief 报告后台包加载是否处于活动状态。 */
    [[nodiscard]] bool IsLoading() const;

    /**
     * @brief 通过模块 ABI 从生产 TIFF 渲染一层。
     * @param layerIndex 从零开始的生产层索引。
     * @param channels 冻结 RGBWSV 拼写的一个或多个通道。
     * @param outputPath 接收宿主缓存预览图像路径。
     * @param error 接收渲染或合同错误。
     * @return 当发布可读预览图像时为 true。
     */
    bool RenderPreview(
        int layerIndex,
        const QStringList& channels,
        QString* outputPath,
        QString* error);

    /**
     * @brief 通过公共 SPI 读取一份在 manifest 中登记的报告。
     * @param reportName 稳定的报告映射键，例如 slice。
     * @param report 接收结构化报告。
     * @param error 接收缺失报告或合同错误。
     * @return 当指定的报告可用时为 true。
     */
    bool ReadReport(
        const QString& reportName,
        hostpackagereport* report,
        QString* error);

    /**
     * @brief 返回最近加载的 Package 审查结果。
     * @return 宿主持有的不可变 Package 数据。
     */
    [[nodiscard]] const hostpackagereview& Review() const;

private:
    bool ExecuteObject(
        const QJsonObject& request,
        QJsonObject* response,
        QString* error);
    bool LoadVerification(QString* error);
    bool LoadSummary(QString* error);
    bool LoadLayers(QString* error);

    ModuleClient& m_client;
    std::unique_ptr<QTemporaryDir> m_previewCache;
    hostpackagereview m_review;
    std::thread m_loadThread;
    std::atomic_bool m_loading{false};
};
