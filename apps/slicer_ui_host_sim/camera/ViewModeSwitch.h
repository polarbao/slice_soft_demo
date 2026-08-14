#pragma once

/** @brief 冻结 UI 合同允许的宿主本地展示模式。 */
enum class HostViewMode
{
    Top,
    ThreeD
};

/**
 * @brief 在不持有或修改场景状态的前提下切换展示模式。
 */
class ViewModeSwitch final
{
public:
    /** @brief 使用给定初始模式创建切换器。 */
    explicit ViewModeSwitch(HostViewMode initialMode = HostViewMode::Top);

    /** @brief 选择当前本地展示模式。 */
    void SetMode(HostViewMode mode);

    /** @brief 返回当前本地展示模式。 */
    [[nodiscard]] HostViewMode Mode() const;

private:
    HostViewMode m_mode{HostViewMode::Top};
};
