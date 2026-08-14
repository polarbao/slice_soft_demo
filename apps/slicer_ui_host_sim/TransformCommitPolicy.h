#pragma once

#include <QJsonObject>
#include <QString>

/**
 * @brief 宿主本地瞬态变换与提交请求策略。
 *
 * 瞬态方法不接收 ModuleClient，因此指针移动期间
 * 不会跨越 DLL 边界。
 */
class TransformCommitPolicy final
{
public:
    /** @brief 创建尚未激活的瞬态策略。 */
    TransformCommitPolicy();

    /**
     * @brief 为单个实例启动本地瞬态平移。
     * @param instanceId 稳定的场景实例标识。
     * @return 瞬态状态初始化成功时返回 true。
     */
    bool Begin(const QString& instanceId);

    /**
     * @brief 替换当前的本地平移预览。
     * @param deltaXMm 沿设备 X 的平移（以毫米为单位）。
     * @param deltaYMm 沿设备 Y 的平移（以毫米为单位）。
     * @param deltaZMm 沿设备 Z 的平移（以毫米为单位）。
     * @return 活动瞬态状态已更新时返回 true。
     */
    bool UpdateTranslation(
        double deltaXMm,
        double deltaYMm,
        double deltaZMm);

    /**
     * @brief 丢弃本地瞬态。
     * @return 该函数不返回值。
     */
    void Reset();

    /**
     * @brief 报告瞬态变换是否处于活动状态。
     * @return 本地反馈正等待提交或丢弃时返回 true。
     */
    bool IsActive() const;

    /**
     * @brief 构建一个原子 scene.apply_operation 请求。
     * @param sceneHandle 引导阶段建立的模块持有场景句柄。
     * @param sceneRevision 当前和预期的权威场景修订。
     * @param operationId 当前请求载荷的唯一幂等标识。
     * @return 冻结提交通道请求，或不活动时为空对象。
     */
    QJsonObject BuildRequest(
        quint64 sceneHandle,
        quint64 sceneRevision,
        const QString& operationId) const;

    /**
     * @brief 检测冻结的 SceneRevisionStale 公共错误。
     * @param response 终端 scene.apply_operation 结果。
     * @return 仅当结果为 PM-SLICER-LAYOUT-0022 时返回 true。
     */
    static bool IsStale(const QJsonObject& response);

private:
    QString m_instanceId;
    double m_deltaXMm{0.0};
    double m_deltaYMm{0.0};
    double m_deltaZMm{0.0};
    bool m_active{false};
};
