#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/PackageDtos.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace slicer_core::api {

/** @brief 提供 RGBWSV 生产包只读查询能力的无 Qt Facade。 */
class PackageQueryFacade
{
public:
    virtual ~PackageQueryFacade() = default;

    /** @brief 读取生产包摘要。 @param package_dir Package 根目录。 @return 摘要或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<PackageSummary> GetSummary(
        const std::filesystem::path& package_dir) const noexcept = 0;

    /** @brief 读取一个层描述符。 @param package_dir Package 根目录。 @param layer_index 层索引。 @return 描述符或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<LayerDescriptor> GetLayerDescriptor(
        const std::filesystem::path& package_dir,
        int layer_index) const noexcept = 0;

    /** @brief 从生产 TIFF 渲染显示预览。 @param request 预览请求。 @param cancel_token 取消源。 @return 预览或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<PreviewResult> RenderLayerPreview(
        const PreviewRequest& request,
        const ICancelToken& cancel_token) const noexcept = 0;

    /** @brief 严格验证生产包。 @param package_dir Package 根目录。 @param cancel_token 取消源。 @return 验证结果或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<VerifyResult> Verify(
        const std::filesystem::path& package_dir,
        const ICancelToken& cancel_token) const noexcept = 0;

    /** @brief 不解释 UI 策略地读取具名报告。 @param package_dir Package 根目录。 @param name 报告名。 @return 结构化报告或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<PackageReport> ReadReport(
        const std::filesystem::path& package_dir,
        std::string_view name) const noexcept = 0;
};

}  // namespace slicer_core::api
