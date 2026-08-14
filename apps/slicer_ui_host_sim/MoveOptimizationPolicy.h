#pragma once

#include "render/TopViewRenderPolicy.h"

#include <QString>

/**
 * @brief 持有基于缓存俯视 ViewData 的宿主本地移动预测。
 *
 * 该策略按设计不持有 ModuleClient 引用，因此指针移动期间
 * 可直接更新矩阵并重绘，无需跨越 DLL 边界。
 */
class MoveOptimizationPolicy final
{
public:
    /**
     * @brief 从权威帧开始本地运动预览。
     * @param frame 当前解码帧。
     * @param instanceId 接收瞬态运动的实例。
     * @return 实例存在且预览状态已激活时返回 true。
     */
    bool Begin(const TopViewFrame& frame, const QString& instanceId);

    /**
     * @brief 将绝对本地平移增量应用到预览。
     * @param deltaXMm 沿设备 X 的平移（以毫米为单位）。
     * @param deltaYMm 沿设备 Y 的平移（以毫米为单位）。
     * @param deltaZMm 沿设备 Z 的平移（以毫米为单位）。
     * @return 活动实例矩阵已更新时返回 true。
     */
    bool UpdateTranslation(
        double deltaXMm,
        double deltaYMm,
        double deltaZMm);

    /**
     * @brief 权威提交成功后采用本地预览。
     * @param sceneRevision 新的权威修订号。
     * @param viewDataIdentity 提交返回的 ViewData 标识。
     * @return 当升级活动预览时为 true。
     */
    bool AcceptCommit(
        quint64 sceneRevision,
        const QString& viewDataIdentity);

    /** @brief 放弃局部运动并恢复起始帧。 */
    void Rollback();

    /**
     * @brief 报告本地运动是否活跃。
     * @return Begin 之后、AcceptCommit 或 Rollback 之前返回 true。
     */
    bool IsActive() const;

    /**
     * @brief 返回当前本地预览帧。
     * @return 适合在不调用模块时重绘的帧。
     */
    const TopViewFrame& Frame() const;

private:
    TopViewFrame m_baseline;
    TopViewFrame m_preview;
    QString m_instanceId;
    std::array<double, 16> m_baselineMatrix{};
    int m_instanceIndex{-1};
    bool m_active{false};
};
