#pragma once

#include "slicer_core/api/PackageQueryFacade.h"
#include "slicer_core/json_value.h"
#include "slicer_core/preview/MaterialPreviewComposer.h"
#include "slicer_core/preview/TiffLayerSource.h"
#include "slicer_core/rip_reader.h"

#include <exception>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

namespace slicer_core::api::implementation::detail
{

inline constexpr const char* kInputError{"PM-SLICER-INPUT-0001"};
inline constexpr const char* kContractError{"PM-SLICER-CONTRACT-0060"};
inline constexpr const char* kOutputError{"PM-SLICER-OUTPUT-0050"};
inline constexpr const char* kCancelledError{"PM-SLICER-CANCELLED-0070"};
inline constexpr const char* kInternalError{"PM-SLICER-INTERNAL-0099"};

ApiError MakeError(
    const char* code,
    std::string message,
    std::string detail = {});

Json ParseObjectFile(const std::filesystem::path& path);

std::filesystem::path RequirePackageDirectory(
    const std::filesystem::path& packageDir);

bool IsPathWithin(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate);

ApiError MapValidationError(const ValidationError& error);

ApiError MapTiffLayerError(const TiffLayerError& error);

MaterialPreviewResult ResizePreview(
    MaterialPreviewResult source,
    int maxWidth);

MaterialPreviewResult OrientPreviewPositiveYUp(
    MaterialPreviewResult source);

void WritePreviewImage(
    const std::filesystem::path& path,
    const MaterialPreviewResult& preview);

template <typename T>
ApiResult<T> UnexpectedFailure(
    const std::string& operation,
    const std::exception& error)
{
    return ApiResult<T>::Failure(MakeError(
        kInternalError,
        operation + " failed unexpectedly",
        error.what()));
}

class PackageQueryFacadeService final : public PackageQueryFacade
{
public:
    ApiResult<PackageSummary> GetSummary(
        const std::filesystem::path& packageDir) const noexcept override;

    ApiResult<LayerDescriptor> GetLayerDescriptor(
        const std::filesystem::path& packageDir,
        int layerIndex) const noexcept override;

    ApiResult<PreviewResult> RenderLayerPreview(
        const PreviewRequest& request,
        const ICancelToken& cancelToken) const noexcept override;

    ApiResult<VerifyResult> Verify(
        const std::filesystem::path& packageDir,
        const ICancelToken& cancelToken) const noexcept override;

    ApiResult<PackageReport> ReadReport(
        const std::filesystem::path& packageDir,
        std::string_view name) const noexcept override;

private:
    struct VerifiedPackageSnapshot
    {
        std::filesystem::path packageDirectory;
        RipValidationResult validation;
        ProductionPackageIndex package;
    };

    [[nodiscard]] VerifiedPackageSnapshot EnsureVerifiedPackageLocked(
        const std::filesystem::path& packageDir,
        bool forceValidation) const;
    [[nodiscard]] static bool IsSnapshotCurrent(
        const VerifiedPackageSnapshot& snapshot);

    mutable std::mutex m_layerMutex;
    mutable TiffLayerSource m_layerSource;
    mutable std::optional<VerifiedPackageSnapshot> m_verifiedPackage;
};

}  // namespace slicer_core::api::implementation::detail
