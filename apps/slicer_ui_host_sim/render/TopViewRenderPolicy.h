#pragma once

#include "../ModuleClient.h"

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QPolygonF>
#include <QSize>
#include <QString>
#include <QVector>

#include <array>

/** @brief 单个顶视预览携带的本地 XY 边界。 */
struct TopViewBounds final
{
    double minX{0.0};
    double minY{0.0};
    double maxX{0.0};
    double maxY{0.0};
};

/** @brief 一个宿主侧顶视实例及其已解码表面预览。 */
struct TopViewInstance final
{
    QString instanceId;
    QString previewIdentity;
    QString appearanceIdentity;
    QString textureStatus;
    TopViewBounds localBoundsMm;
    std::array<double, 16> worldMatrix{};
    QImage surfacePreview;
};

/** @brief 俯视图的纯显示构建体积与自适应网格设置。 */
struct TopViewDecor final
{
    double buildWidthMm{230.0};
    double buildHeightMm{100.0};
    double minorGridMm{1.0};
    double majorGridMm{10.0};
    bool coordinateFrameResolved{false};
};

/** @brief 按约定不可变的单场景修订本地渲染快照。 */
struct TopViewFrame final
{
    QString viewDataIdentity;
    quint64 sceneRevision{0};
    quint64 localRevision{0};
    TopViewDecor decor;
    QVector<TopViewInstance> instances;
};

/**
 * @brief 获取冻结的 top ViewData，并在不链接实现的前提下渲染。
 *
 * 预览像素按 previewIdentity 缓存。实例世界矩阵保持在缓存之外，
 * 因此宿主本地移动不会使纹理像素失效。
 */
class TopViewRenderPolicy final
{
public:
    /**
     * @brief 基于一个已加载公共模块客户端创建策略。
     * @param client 运行时加载的公共 ABI 客户端。
     */
    explicit TopViewRenderPolicy(ModuleClient& client);

    /**
     * @brief 为权威场景修订刷新 top ViewData。
     * @param sceneHandle 模块持有的场景句柄。
     * @param sceneRevision 预期的权威修订号。
     * @param frame 接收已解码预览与本地放置数据。
     * @param error 接收失败即拒绝的 DTO 或 blob 诊断。
     * @return 所有必需表面预览均可用时返回 true。
     */
    bool Refresh(
        quint64 sceneHandle,
        quint64 sceneRevision,
        TopViewFrame* frame,
        QString* error);

    /**
     * @brief 渲染宿主本地正交顶视帧。
     * @param frame 先前解码的本地帧。
     * @param canvasSize 请求的输出像素尺寸。
     * @return 纯显示 RGBA 图像；输入无效时返回空图像。
     * @note 高频保留式重绘请使用 RenderInto。
     */
    QImage Render(
        const TopViewFrame& frame,
        const QSize& canvasSize) const;

    /**
     * @brief 渲染到调用方持有的图像，以支持高帧率保留式重绘。
     * @param frame 先前解码的本地帧。
     * @param output 持久 RGBA 图像，其尺寸定义视口。
     * @return 完整纹理帧渲染成功时返回 true。
     * @note 必须从所属 UI 线程调用，且不会调用公共模块。
     */
    bool RenderInto(
        const TopViewFrame& frame,
        QImage* output) const;

    /**
     * @brief 返回已解码预览缓存条目数。
     * @return 当前预览缓存大小。
     */
    int CachedPreviewCount() const;

    /**
     * @brief 返回通过公共 ABI 获取的 blob 分块数。
     * @return 单调递增的分块读取计数。
     */
    quint64 BlobReadCount() const;

    /**
     * @brief 将一个渲染图像坐标转换为构建体积 XY 坐标。
     * @param frame 用于渲染图像的帧。
     * @param canvasSize 传给 Render 的像素尺寸。
     * @param imagePoint 渲染图像中的像素点。
     * @param worldPoint 接收构建体积毫米坐标。
     * @return 能够通过有效场景装饰映射该点时返回 true。
     */
    static bool ImageToWorld(
        const TopViewFrame& frame,
        const QSize& canvasSize,
        const QPointF& imagePoint,
        QPointF* worldPoint);

    /**
     * @brief 拾取渲染图像坐标下最上层的实例。
     * @param frame 用于渲染图像的帧。
     * @param canvasSize 传给 Render 的像素尺寸。
     * @param imagePoint 渲染图像中的像素点。
     * @return 稳定实例标识；未命中模型时返回空值。
     */
    static QString PickInstance(
        const TopViewFrame& frame,
        const QSize& canvasSize,
        const QPointF& imagePoint);

private:
    bool ExecuteJson(
        const QJsonObject& request,
        QJsonObject* result,
        QString* error);
    bool LoadSceneDecor(
        quint64 sceneHandle,
        quint64 sceneRevision,
        TopViewDecor* decor,
        QString* error);
    bool DecodeInstance(
        const QJsonObject& value,
        const QHash<QString, bool>& appearances,
        TopViewInstance* instance,
        QString* error);
    bool LoadPreview(
        const QJsonObject& descriptor,
        QImage* image,
        QString* error);
    bool ReadBlob(
        const QString& blobId,
        int chunkCount,
        qint64 totalBytes,
        QByteArray* bytes,
        QString* error);

    ModuleClient& m_client;
    QHash<QString, QImage> m_previewCache;
    mutable QString m_renderPlanIdentity;
    mutable quint64 m_renderPlanLocalRevision{0};
    mutable QSize m_renderPlanCanvasSize;
    mutable QVector<QPolygonF> m_renderPlanDestinations;
    quint64 m_blobReadCount{0};
};
