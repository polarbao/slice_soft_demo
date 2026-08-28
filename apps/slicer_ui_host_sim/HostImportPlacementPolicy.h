#pragma once

/** @brief 决定模型导入完成后是否需要应用当前规则排版。 */
class HostImportPlacementPolicy final
{
public:
    /**
     * @brief 判断当前非空场景是否应执行一次自动规则排版。
     * @param instanceCount 导入提交完成后的场景实例数量。
     * @return 场景至少包含一个实例时返回 true。
     */
    [[nodiscard]] static bool RequiresGridLayout(int instanceCount) noexcept;
};
