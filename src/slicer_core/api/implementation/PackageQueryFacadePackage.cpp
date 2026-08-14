#include "slicer_core/api/implementation/PackageQueryFacadeInternal.h"

#include "slicer_core/TiffReadApi.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// 文件职责：验证 manifest、层描述符和 TIFF 统计，并生成只读生产包摘要；
// 边界：任何 schema、通道或极性不一致均失败即拒绝，不尝试兼容性修补。
namespace slicer_core::api::implementation::detail
{
namespace
{

void RequireStringField(
    const Json& object,
    const std::string& field,
    const std::string& context)
{
    if (!object.contains(field)
        || !object.at(field).is_string()
        || object.at(field).as_string().empty())
    {
        throw std::runtime_error(
            context + "." + field + " must be a non-empty string");
    }
}

void RequireObjectField(
    const Json& object,
    const std::string& field,
    const std::string& context)
{
    if (!object.contains(field) || !object.at(field).is_object())
    {
        throw std::runtime_error(
            context + "." + field + " must be an object");
    }
}

void RequireNumberPairField(
    const Json& object,
    const std::string& field,
    const std::string& context)
{
    if (!object.contains(field)
        || !object.at(field).is_array()
        || object.at(field).size() != 2U
        || !object.at(field).at(0U).is_number()
        || !object.at(field).at(1U).is_number())
    {
        throw std::runtime_error(
            context + "." + field + " must be a two-number array");
    }
}

std::vector<StructuredJsonObject> ReadPerInstance(const Json& manifest)
{
    if (!manifest.contains("perInstance")
        || !manifest.at("perInstance").is_array())
    {
        throw std::runtime_error(
            "manifest.perInstance is required by capability v1.2");
    }

    std::vector<StructuredJsonObject> result;
    const Json& entries = manifest.at("perInstance");
    result.reserve(entries.size());
    for (std::size_t index = 0U; index < entries.size(); ++index)
    {
        const Json& entry = entries.at(index);
        const std::string context =
            "manifest.perInstance[" + std::to_string(index) + "]";
        if (!entry.is_object())
        {
            throw std::runtime_error(context + " must be an object");
        }
        RequireStringField(entry, "instanceId", context);
        RequireStringField(entry, "modelId", context);
        RequireNumberPairField(entry, "layerRange", context);
        RequireObjectField(entry, "printPixels", context);
        RequireObjectField(entry, "emptyPixels", context);
        RequireObjectField(entry, "bboxMm", context);
        RequireObjectField(entry, "transformApplied", context);
        result.push_back(StructuredJsonObject{entry.dump(0)});
    }
    return result;
}

StructuredJsonObject ReadProfileEcho(const Json& manifest)
{
    if (!manifest.contains("profileEcho")
        || !manifest.at("profileEcho").is_object())
    {
        throw std::runtime_error(
            "manifest.profileEcho is required by capability v1.2");
    }
    const Json& profileEcho = manifest.at("profileEcho");
    RequireStringField(
        profileEcho,
        "profileVersion",
        "manifest.profileEcho");
    RequireStringField(
        profileEcho,
        "profileHash",
        "manifest.profileEcho");
    return StructuredJsonObject{profileEcho.dump(0)};
}

std::string ReadFileBytes(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        return {};
    }
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

std::string FileMetadataIdentity(const std::filesystem::path& path)
{
    std::error_code error;
    const std::uintmax_t bytes = std::filesystem::file_size(path, error);
    if (error)
    {
        return {};
    }
    const auto modified = std::filesystem::last_write_time(path, error);
    if (error)
    {
        return {};
    }
    return ComputeSha256(
        path.generic_string() + "|" + std::to_string(bytes) + "|"
        + std::to_string(modified.time_since_epoch().count()));
}

}  // namespace

bool PackageQueryFacadeService::IsSnapshotCurrent(
    const VerifiedPackageSnapshot& snapshot)
{
    const std::string manifestBytes = ReadFileBytes(
        snapshot.package.manifestPath);
    if (manifestBytes.empty()
        || ComputeSha256(manifestBytes) != snapshot.package.manifestHash)
    {
        return false;
    }
    return std::all_of(
        snapshot.package.layers.begin(),
        snapshot.package.layers.end(),
        [](const ProductionLayerRef& layer)
        {
            return FileMetadataIdentity(layer.path) == layer.checksum;
        });
}

PackageQueryFacadeService::VerifiedPackageSnapshot
PackageQueryFacadeService::EnsureVerifiedPackageLocked(
    const std::filesystem::path& packageDir,
    const bool forceValidation) const
{
    if (!forceValidation && m_verifiedPackage.has_value()
        && m_verifiedPackage->packageDirectory == packageDir
        && IsSnapshotCurrent(*m_verifiedPackage))
    {
        return *m_verifiedPackage;
    }

    VerifiedPackageSnapshot snapshot;
    snapshot.packageDirectory = packageDir;
    snapshot.validation = validate_slice_package(packageDir);
    snapshot.package = m_layerSource.IndexPackage(
        packageDir / "manifest.json");
    m_verifiedPackage = snapshot;
    return snapshot;
}

ApiResult<PackageSummary> PackageQueryFacadeService::GetSummary(
    const std::filesystem::path& packageDir) const noexcept
{
    try
    {
        const std::filesystem::path absolutePackage =
            RequirePackageDirectory(packageDir);
        VerifiedPackageSnapshot snapshot;
        {
            std::scoped_lock lock{m_layerMutex};
            snapshot = EnsureVerifiedPackageLocked(
                absolutePackage, false);
        }
        const Json manifest = ParseObjectFile(
            absolutePackage / "manifest.json");

        PackageSummary summary;
        summary.package_dir = absolutePackage;
        summary.package_identity = snapshot.package.packageIdentity;
        summary.schema = snapshot.validation.schema;
        summary.layer_count = snapshot.validation.layer_count;
        summary.grid.width_px = snapshot.validation.width_px;
        summary.grid.height_px = snapshot.validation.height_px;
        summary.grid.dpi_x = snapshot.validation.dpi_x;
        summary.grid.dpi_y = snapshot.validation.dpi_y;
        summary.channels = snapshot.validation.channel_order;
        summary.bit_depth = snapshot.validation.bit_depth;
        summary.polarity = "black_is_print";
        summary.per_instance = ReadPerInstance(manifest);
        summary.profile_echo = ReadProfileEcho(manifest);
        return ApiResult<PackageSummary>::Success(std::move(summary));
    }
    catch (const ValidationError& error)
    {
        return ApiResult<PackageSummary>::Failure(
            MapValidationError(error));
    }
    catch (const TiffLayerError& error)
    {
        return ApiResult<PackageSummary>::Failure(
            MapTiffLayerError(error));
    }
    catch (const std::filesystem::filesystem_error& error)
    {
        return ApiResult<PackageSummary>::Failure(MakeError(
            kInputError,
            "package path could not be accessed",
            error.what()));
    }
    catch (const std::runtime_error& error)
    {
        return ApiResult<PackageSummary>::Failure(MakeError(
            kContractError,
            "package summary metadata is incomplete",
            error.what()));
    }
    catch (const std::exception& error)
    {
        return UnexpectedFailure<PackageSummary>("package summary", error);
    }
    catch (...)
    {
        return ApiResult<PackageSummary>::Failure(MakeError(
            kInternalError,
            "unknown package summary failure"));
    }
}

ApiResult<LayerDescriptor> PackageQueryFacadeService::GetLayerDescriptor(
    const std::filesystem::path& packageDir,
    const int layerIndex) const noexcept
{
    try
    {
        const std::filesystem::path absolutePackage =
            RequirePackageDirectory(packageDir);

        ProductionLayerRef layer;
        RipLayerChecksum validatedLayer;
        {
            std::scoped_lock lock{m_layerMutex};
            const VerifiedPackageSnapshot snapshot =
                EnsureVerifiedPackageLocked(absolutePackage, false);
            const std::optional<ProductionLayerRef> found =
                m_layerSource.FindLayer(layerIndex);
            if (!found.has_value())
            {
                return ApiResult<LayerDescriptor>::Failure(MakeError(
                    kInputError,
                    "layer index is not listed by the package",
                    std::to_string(layerIndex)));
            }
            layer = *found;
            const auto validated = std::find_if(
                snapshot.validation.layer_checksums.begin(),
                snapshot.validation.layer_checksums.end(),
                [layerIndex](const RipLayerChecksum& candidate)
                {
                    return candidate.index == layerIndex;
                });
            if (validated == snapshot.validation.layer_checksums.end())
            {
                return ApiResult<LayerDescriptor>::Failure(MakeError(
                    kContractError,
                    "verified package is missing layer statistics",
                    std::to_string(layerIndex)));
            }
            validatedLayer = *validated;
        }

        LayerDescriptor descriptor;
        descriptor.layer_index = layer.layerIndex;
        descriptor.z_mm = layer.zMm;
        descriptor.width_px = static_cast<int>(layer.width);
        descriptor.height_px = static_cast<int>(layer.height);
        descriptor.tiff_path = layer.path;
        descriptor.storage_mode = tiff_storage_mode_string(layer.storage);
        for (std::size_t channel = 0U;
             channel < descriptor.print_pixels.size();
             ++channel)
        {
            descriptor.print_pixels.at(channel) =
                validatedLayer.channel_stats.at(channel).print_pixels;
            descriptor.empty_pixels.at(channel) =
                validatedLayer.channel_stats.at(channel).empty_pixels;
        }
        return ApiResult<LayerDescriptor>::Success(std::move(descriptor));
    }
    catch (const ValidationError& error)
    {
        return ApiResult<LayerDescriptor>::Failure(
            MapValidationError(error));
    }
    catch (const TiffLayerError& error)
    {
        return ApiResult<LayerDescriptor>::Failure(
            MapTiffLayerError(error));
    }
    catch (const std::filesystem::filesystem_error& error)
    {
        return ApiResult<LayerDescriptor>::Failure(MakeError(
            kInputError,
            "package path could not be accessed",
            error.what()));
    }
    catch (const std::exception& error)
    {
        return UnexpectedFailure<LayerDescriptor>(
            "layer descriptor query",
            error);
    }
    catch (...)
    {
        return ApiResult<LayerDescriptor>::Failure(MakeError(
            kInternalError,
            "unknown layer descriptor failure"));
    }
}

ApiResult<VerifyResult> PackageQueryFacadeService::Verify(
    const std::filesystem::path& packageDir,
    const ICancelToken& cancelToken) const noexcept
{
    try
    {
        if (cancelToken.IsCancelRequested())
        {
            return ApiResult<VerifyResult>::Failure(MakeError(
                kCancelledError,
                "package verification was cancelled"));
        }
        const std::filesystem::path absolutePackage =
            RequirePackageDirectory(packageDir);
        VerifiedPackageSnapshot snapshot;
        {
            std::scoped_lock lock{m_layerMutex};
            snapshot = EnsureVerifiedPackageLocked(
                absolutePackage, true);
        }
        if (cancelToken.IsCancelRequested())
        {
            return ApiResult<VerifyResult>::Failure(MakeError(
                kCancelledError,
                "package verification was cancelled"));
        }

        VerifyResult result;
        result.valid = true;
        result.layer_count = snapshot.validation.layer_count;
        result.per_layer_checksum.reserve(
            snapshot.validation.layer_checksums.size());
        for (const RipLayerChecksum& checksum :
             snapshot.validation.layer_checksums)
        {
            result.per_layer_checksum.push_back(checksum.channels);
        }
        return ApiResult<VerifyResult>::Success(std::move(result));
    }
    catch (const ValidationError& error)
    {
        if (error.code() == ValidationErrorCode::PackageNotFound
            || error.code() == ValidationErrorCode::ManifestMissing)
        {
            return ApiResult<VerifyResult>::Failure(
                MapValidationError(error));
        }
        VerifyResult result;
        result.valid = false;
        result.errors.push_back(PackageValidationError{
            validation_error_code_string(error.code()),
            error.what()});
        return ApiResult<VerifyResult>::Success(std::move(result));
    }
    catch (const TiffLayerError& error)
    {
        return ApiResult<VerifyResult>::Failure(
            MapTiffLayerError(error));
    }
    catch (const std::filesystem::filesystem_error& error)
    {
        return ApiResult<VerifyResult>::Failure(MakeError(
            kInputError,
            "package path could not be accessed",
            error.what()));
    }
    catch (const std::exception& error)
    {
        return UnexpectedFailure<VerifyResult>(
            "package verification",
            error);
    }
    catch (...)
    {
        return ApiResult<VerifyResult>::Failure(MakeError(
            kInternalError,
            "unknown package verification failure"));
    }
}

}  // namespace slicer_core::api::implementation::detail
