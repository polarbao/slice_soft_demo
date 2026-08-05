#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/PackageDtos.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace slicer_core::api {

/** @brief Qt-free read-only facade for RGBWSV packages. */
class PackageQueryFacade
{
public:
    virtual ~PackageQueryFacade() = default;

    /** @brief Reads package summary. @param package_dir Package root. @return Summary or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<PackageSummary> GetSummary(
        const std::filesystem::path& package_dir) const noexcept = 0;

    /** @brief Reads one layer descriptor. @param package_dir Package root. @param layer_index Layer index. @return Descriptor or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<LayerDescriptor> GetLayerDescriptor(
        const std::filesystem::path& package_dir,
        int layer_index) const noexcept = 0;

    /** @brief Renders a display preview from production TIFF. @param request Preview request. @param cancel_token Cancellation source. @return Preview or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<PreviewResult> RenderLayerPreview(
        const PreviewRequest& request,
        const ICancelToken& cancel_token) const noexcept = 0;

    /** @brief Strictly verifies a package. @param package_dir Package root. @param cancel_token Cancellation source. @return Verification or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<VerifyResult> Verify(
        const std::filesystem::path& package_dir,
        const ICancelToken& cancel_token) const noexcept = 0;

    /** @brief Reads a named report without interpreting UI policy. @param package_dir Package root. @param name Report name. @return UTF-8 report or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<std::string> ReadReport(
        const std::filesystem::path& package_dir,
        std::string_view name) const noexcept = 0;
};

}  // namespace slicer_core::api
