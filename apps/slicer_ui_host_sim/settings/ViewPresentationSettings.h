#pragma once

#include "../camera/ViewModeSwitch.h"
#include "../render/IRenderBackend.h"

#include <QString>

/**
 * @brief 在会话配置文件中持久化纯显示视图偏好。
 */
class ViewPresentationSettings final
{
public:
    /**
     * @brief 创建绑定到会话配置路径的设置。
     * @param sessionConfigPath 用于往返持久化的 JSON 文件。
     */
    explicit ViewPresentationSettings(const QString& sessionConfigPath);

    /** @brief 加载设置；文件不存在时加载合同默认值。 */
    bool Load(QString* error);

    /** @brief 原子保存当前显示偏好。 */
    bool Save(QString* error) const;

    /** @brief 返回配置的默认展示模式。 */
    [[nodiscard]] HostViewMode DefaultViewMode() const;

    /** @brief 更新配置的默认展示模式。 */
    void SetDefaultViewMode(HostViewMode mode);

    /** @brief 返回配置的三维投影。 */
    [[nodiscard]] slicer::render::Projection ThreeDProjection() const;

    /** @brief 更新三维投影偏好。 */
    void SetThreeDProjection(slicer::render::Projection projection);

    /** @brief 返回绑定的会话配置路径。 */
    [[nodiscard]] QString SessionConfigPath() const;

private:
    QString m_sessionConfigPath;
    HostViewMode m_defaultViewMode{HostViewMode::Top};
    slicer::render::Projection m_threeDProjection{
        slicer::render::Projection::Orthographic};
};
