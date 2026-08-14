#pragma once

#include "slicer_module/CapabilityJsonAdapter.h"
#include "slicer_core/api/SceneViewDtos.h"

#include <memory>

namespace slicesoft::module
{

/** @brief 序列化冻结 ViewData DTO，并持有有界二进制对象存储。 */
class SceneViewDataAdapter final
{
public:
    /** @brief 创建空的有界 ViewData 二进制对象存储。 */
    SceneViewDataAdapter();

    /** @brief 释放所有保留的 ViewData 二进制对象。 */
    ~SceneViewDataAdapter();

    /** @brief 不引入额外 DTO，直接序列化提供者输出。 @param data 提供者结果。 @return v1.2 响应对象。 */
    [[nodiscard]] slicer_core::Json Serialize(
        const slicer_core::api::SceneViewData& data);

    /** @brief 读取一个冻结 ViewData 二进制对象分块。 @param request 读取请求。 @return 二进制输出。 */
    [[nodiscard]] CapabilityOutput ReadBlob(const slicer_core::Json& request);

private:
    class Implementation;
    std::unique_ptr<Implementation> m_implementation;
};

}  // namespace slicesoft::module
