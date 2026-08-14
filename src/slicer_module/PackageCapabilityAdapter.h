#pragma once

#include "slicer_module/CapabilityJsonAdapter.h"

#include <memory>
#include <string>

namespace slicesoft::module
{

/** @brief 将只读 RGBWSV 生产包能力接入 PackageQueryFacade。 */
class PackageCapabilityAdapter final
{
public:
    /** @brief 使用生产 PackageQueryFacade 创建适配器。 */
    PackageCapabilityAdapter();

    /** @brief 销毁所持 PackageQueryFacade。 */
    ~PackageCapabilityAdapter();

    /** @brief 执行 Package 查询能力。 @param capability 冻结 ID。 @param request DTO 对象。 @return 结果对象。 */
    [[nodiscard]] slicer_core::Json Execute(
        const std::string& capability,
        const slicer_core::Json& request) const;

private:
    class Implementation;
    std::unique_ptr<Implementation> m_implementation;
};

}  // namespace slicesoft::module
