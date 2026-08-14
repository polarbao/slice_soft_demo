#pragma once

#include "slicer_module/CapabilityJsonAdapter.h"

#include <memory>
#include <string>

namespace slicesoft::module
{

class ModelCapabilityAdapter;

/** @brief 将场景、碰撞和 ViewData 能力接入 SceneFacade。 */
class SceneCapabilityAdapter final
{
public:
    /** @brief 将场景路由绑定到已导入模型。 @param models 模型适配器。 */
    explicit SceneCapabilityAdapter(ModelCapabilityAdapter& models);

    /** @brief 释放场景会话和 ViewData 二进制对象。 */
    ~SceneCapabilityAdapter();

    /** @brief 执行一个场景侧能力。 @param capability 冻结 ID。 @param request DTO 对象。 @return 终态输出。 */
    [[nodiscard]] CapabilityOutput Execute(
        const std::string& capability,
        const slicer_core::Json& request);

private:
    class Implementation;
    std::unique_ptr<Implementation> m_implementation;
};

}  // namespace slicesoft::module
