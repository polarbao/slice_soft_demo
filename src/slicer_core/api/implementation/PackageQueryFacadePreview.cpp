#include "slicer_core/api/implementation/PackageQueryFacadeInternal.h"

#include <array>
#include <filesystem>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace slicer_core::api::implementation::detail
{
namespace
{

constexpr const char* kPreviewSemanticVersion{"1"};

MaterialPreviewMode SingleChannelMode(const std::string& channel)
{
    if (channel == "R")
    {
        return MaterialPreviewMode::Red;
    }
    if (channel == "G")
    {
        return MaterialPreviewMode::Green;
    }
    if (channel == "B")
    {
        return MaterialPreviewMode::Blue;
    }
    if (channel == "W")
    {
        return MaterialPreviewMode::White;
    }
    if (channel == "S")
    {
        return MaterialPreviewMode::Support;
    }
    if (channel == "V")
    {
        return MaterialPreviewMode::Varnish;
    }
    throw std::runtime_error("unsupported RGBWSV channel: " + channel);
}

std::set<std::string> ValidateChannels(
    const std::vector<std::string>& channels)
{
    if (channels.empty())
    {
        throw std::runtime_error("preview channels must not be empty");
    }
    std::set<std::string> unique;
    for (const std::string& channel : channels)
    {
        static const std::set<std::string> kSupported{
            "R", "G", "B", "W", "S", "V"};
        if (!kSupported.contains(channel))
        {
            throw std::runtime_error(
                "unsupported RGBWSV channel: " + channel);
        }
        if (!unique.insert(channel).second)
        {
            throw std::runtime_error(
                "preview channel is duplicated: " + channel);
        }
    }
    return unique;
}

MaterialPreviewMode ResolvePreviewMode(const PreviewRequest& request)
{
    const std::set<std::string> channels =
        ValidateChannels(request.channels);
    if (request.mode == "single_channel")
    {
        if (channels.size() != 1U)
        {
            throw std::runtime_error(
                "single_channel preview requires exactly one channel");
        }
        return SingleChannelMode(*channels.begin());
    }
    if (request.mode != "composite")
    {
        throw std::runtime_error(
            "preview mode must be single_channel or composite");
    }

    if (channels == std::set<std::string>{"R", "G", "B"})
    {
        return MaterialPreviewMode::Rgb;
    }
    if (channels == std::set<std::string>{"R", "G", "B", "W"})
    {
        return MaterialPreviewMode::RgbWhite;
    }
    if (channels == std::set<std::string>{"R", "G", "B", "S"})
    {
        return MaterialPreviewMode::RgbSupport;
    }
    if (channels == std::set<std::string>{"R", "G", "B", "V"})
    {
        return MaterialPreviewMode::RgbVarnish;
    }
    if (channels
        == std::set<std::string>{"R", "G", "B", "W", "S", "V"})
    {
        return MaterialPreviewMode::RgbSupportWhiteVarnish;
    }
    throw std::runtime_error(
        "unsupported composite channel combination");
}

std::string CanonicalChannels(const std::vector<std::string>& channels)
{
    static constexpr std::array<std::string_view, 6> kOrder{
        "R", "G", "B", "W", "S", "V"};
    const std::set<std::string> requested = ValidateChannels(channels);
    std::string result;
    for (const std::string_view channel : kOrder)
    {
        if (requested.contains(std::string{channel}))
        {
            result.append(channel);
        }
    }
    return result;
}

}  // namespace

ApiResult<PreviewResult> PackageQueryFacadeService::RenderLayerPreview(
    const PreviewRequest& request,
    const ICancelToken& cancelToken) const noexcept
{
    try
    {
        if (cancelToken.IsCancelRequested())
        {
            return ApiResult<PreviewResult>::Failure(MakeError(
                kCancelledError,
                "preview rendering was cancelled"));
        }
        const MaterialPreviewMode previewMode =
            ResolvePreviewMode(request);
        const std::filesystem::path absolutePackage =
            RequirePackageDirectory(request.package_dir);

        TiffLayerLoadResult loaded;
        ProductionLayerRef layer;
        ProductionPackageIndex package;
        {
            std::scoped_lock lock{m_layerMutex};
            package = m_layerSource.IndexPackage(
                absolutePackage / "manifest.json");
            const std::optional<ProductionLayerRef> found =
                m_layerSource.FindLayer(request.layer_index);
            if (!found.has_value())
            {
                return ApiResult<PreviewResult>::Failure(MakeError(
                    kInputError,
                    "layer index is not listed by the package",
                    std::to_string(request.layer_index)));
            }
            layer = *found;
            TiffLayerLoadControl control;
            control.cancellationRequested = [&cancelToken]()
            {
                return cancelToken.IsCancelRequested();
            };
            loaded = m_layerSource.LoadLayer(layer, control);
        }

        MaterialPreviewRequest composeRequest;
        composeRequest.mode = previewMode;
        MaterialPreviewResult preview = MaterialPreviewComposer::Compose(
            *loaded.buffer,
            composeRequest);
        preview = ResizePreview(
            std::move(preview),
            request.max_width_px);
        if (cancelToken.IsCancelRequested())
        {
            return ApiResult<PreviewResult>::Failure(MakeError(
                kCancelledError,
                "preview rendering was cancelled"));
        }

        const std::filesystem::path outputPath =
            std::filesystem::absolute(request.output_path)
                .lexically_normal();
        try
        {
            WritePreviewImage(outputPath, preview);
        }
        catch (const std::exception& error)
        {
            return ApiResult<PreviewResult>::Failure(MakeError(
                kOutputError,
                "failed to write layer preview",
                error.what()));
        }

        PreviewResult result;
        result.output_path = outputPath;
        result.width_px = static_cast<int>(preview.width);
        result.height_px = static_cast<int>(preview.height);
        result.cache_key =
            "pkg:" + package.packageIdentity
            + "|manifest:" + package.manifestHash
            + "|layer:" + std::to_string(layer.layerIndex)
            + "|mode:" + request.mode
            + "|ch:" + CanonicalChannels(request.channels)
            + "|w:" + std::to_string(request.max_width_px)
            + "|sem:" + kPreviewSemanticVersion;
        return ApiResult<PreviewResult>::Success(std::move(result));
    }
    catch (const TiffLayerError& error)
    {
        return ApiResult<PreviewResult>::Failure(
            MapTiffLayerError(error));
    }
    catch (const ValidationError& error)
    {
        return ApiResult<PreviewResult>::Failure(
            MapValidationError(error));
    }
    catch (const std::filesystem::filesystem_error& error)
    {
        return ApiResult<PreviewResult>::Failure(MakeError(
            kInputError,
            "preview input path could not be accessed",
            error.what()));
    }
    catch (const MaterialPreviewError& error)
    {
        return ApiResult<PreviewResult>::Failure(MakeError(
            kContractError,
            "production TIFF preview could not be composed",
            error.what()));
    }
    catch (const std::runtime_error& error)
    {
        return ApiResult<PreviewResult>::Failure(MakeError(
            kContractError,
            "layer preview request is invalid",
            error.what()));
    }
    catch (const std::exception& error)
    {
        return UnexpectedFailure<PreviewResult>(
            "layer preview rendering",
            error);
    }
    catch (...)
    {
        return ApiResult<PreviewResult>::Failure(MakeError(
            kInternalError,
            "unknown layer preview failure"));
    }
}

}  // namespace slicer_core::api::implementation::detail
