#include "slicer_core/api/implementation/PackageQueryFacadeInternal.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// 文件职责：把已验证生产 TIFF 映射为仅显示预览；
// 边界：预览选择不得改写生产通道语义，也不得替代生产包验证。
namespace slicer_core::api::implementation::detail
{
namespace
{

constexpr const char* kPreviewSemanticVersion{"2"};

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
    if (channel == "T")
    {
        return MaterialPreviewMode::Transfer;
    }
    if (channel == "V")
    {
        return MaterialPreviewMode::Varnish;
    }
    throw std::runtime_error("unsupported RGBWSVT channel: " + channel);
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
        // T 此前不在集合内，于是任何含 T 的预览请求都在此抛出，
        // 连下方专门的 T-only 分支都成了走不到的死代码——
        // 宿主结果页提供了「T（缩裹）」与七通道组合两个选项，两者必然失败。
        static const std::set<std::string> kSupported{
            "R", "G", "B", "W", "S", "V", "T"};
        if (!kSupported.contains(channel))
        {
            throw std::runtime_error(
                "unsupported RGBWSVT channel: " + channel);
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
    if (channels
        == std::set<std::string>{"R", "G", "B", "W", "S", "V", "T"})
    {
        return MaterialPreviewMode::RgbSupportWhiteVarnishTransfer;
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

std::set<std::string> ValidateRgbwsvtChannels(
    const std::vector<std::string>& channels)
{
    if (channels.empty())
    {
        throw std::runtime_error("preview channels must not be empty");
    }
    static const std::set<std::string> kSupported{
        "R", "G", "B", "W", "S", "V", "T"};
    std::set<std::string> unique;
    for (const std::string& channel : channels)
    {
        if (!kSupported.contains(channel))
        {
            throw std::runtime_error(
                "unsupported RGBWSVT channel: " + channel);
        }
        if (!unique.insert(channel).second)
        {
            throw std::runtime_error(
                "preview channel is duplicated: " + channel);
        }
    }
    return unique;
}

std::string CanonicalRgbwsvtChannels(
    const std::vector<std::string>& channels)
{
    static constexpr std::array<std::string_view, 7> kOrder{
        "R", "G", "B", "W", "S", "V", "T"};
    const std::set<std::string> requested =
        ValidateRgbwsvtChannels(channels);
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

// 取出 T 平面。此前的做法是把 T 直接丢掉（函数原名 DropTransferChannel），
// 于是七通道包在组合预览里与六通道毫无区别，缩裹完全不可见。
std::vector<std::uint8_t> ExtractTransferPlane(
    const RgbwsvtDecodedPackageLayer& source)
{
    const std::size_t pixelCount = static_cast<std::size_t>(
        source.descriptor.width) * static_cast<std::size_t>(
        source.descriptor.height);
    std::vector<std::uint8_t> plane;
    plane.reserve(pixelCount);
    for (std::size_t pixel = 0U; pixel < pixelCount; ++pixel)
    {
        plane.push_back(source.pixels.at(pixel * 7U + 6U));
    }
    return plane;
}

RgbwsvLayerBuffer SplitRgbwsvChannels(
    const RgbwsvtDecodedPackageLayer& source,
    const RgbwsvtPackageValidation& package)
{
    RgbwsvLayerBuffer result;
    result.sourceIdentity = source.descriptor.fileIdentity;
    result.layerIndex = source.descriptor.index;
    result.zMm = source.descriptor.zMm;
    result.width = source.descriptor.width;
    result.height = source.descriptor.height;
    result.dpiX = package.dpiX;
    result.dpiY = package.dpiY;
    const std::size_t pixelCount = static_cast<std::size_t>(result.width)
        * static_cast<std::size_t>(result.height);
    result.pixels.reserve(pixelCount * 6U);
    for (std::size_t pixel = 0U; pixel < pixelCount; ++pixel)
    {
        const auto begin = source.pixels.begin()
            + static_cast<std::ptrdiff_t>(pixel * 7U);
        result.pixels.insert(result.pixels.end(), begin, begin + 6);
    }
    result.decodedBytes = result.pixels.size();
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
        const std::filesystem::path absolutePackage =
            RequirePackageDirectory(request.package_dir);
        const std::string schema = ReadPackageManifestSchema(absolutePackage);
        std::string packageIdentity;
        std::string manifestHash;
        std::string canonicalChannels;
        int resolvedLayerIndex{-1};
        MaterialPreviewResult preview;
        if (schema == "p0.rgbwsvt.1")
        {
            const std::set<std::string> channels =
                ValidateRgbwsvtChannels(request.channels);
            // 组合视图含 T 曾在此被拒。该限制源于当时合成器不认识 T：
            // 组合路径把 T 丢弃（DropTransferChannel），只能靠单通道另走一条硬编码分支。
            // 现在合成器已有 RgbSupportWhiteVarnishTransfer 模式、T 平面亦透传，
            // 组合视图能正确叠加缩裹，故该守卫失效并移除——
            // 否则宿主的「全通道并集（七通道·含缩裹）」永远无法渲染。
            if (request.mode == "single_channel" && channels.size() != 1U)
            {
                throw std::runtime_error(
                    "single_channel preview requires exactly one channel");
            }
            RgbwsvtPackageValidation package;
            {
                std::scoped_lock lock{m_layerMutex};
                package = EnsureVerifiedRgbwsvtPackageLocked(
                    absolutePackage, false);
            }
            const auto found = std::find_if(
                package.layers.begin(), package.layers.end(),
                [&request](const RgbwsvtPackageLayer& candidate)
                {
                    return candidate.index == request.layer_index;
                });
            if (found == package.layers.end())
            {
                return ApiResult<PreviewResult>::Failure(MakeError(
                    kInputError,
                    "layer index is not listed by the package",
                    std::to_string(request.layer_index)));
            }
            const RgbwsvtDecodedPackageLayer decoded =
                ReadRgbwsvtPackageLayer(*found);
            {
                // T 单通道与其余模式统一走合成器：此前 T-only 另有一条硬编码
                // (255, value, 255) 渐变分支，与调色板不一致，且因 T 未列入
                // 受支持通道集而根本走不到。现由 SingleChannelMode 映射为
                // MaterialPreviewMode::Transfer，与组合视图用同一个缩裹伪彩色。
                MaterialPreviewRequest composeRequest;
                composeRequest.mode = ResolvePreviewMode(request);
                const RgbwsvLayerBuffer sixChannel =
                    SplitRgbwsvChannels(decoded, package);
                const std::vector<std::uint8_t> transferPlane =
                    ExtractTransferPlane(decoded);
                preview = MaterialPreviewComposer::Compose(
                    sixChannel, composeRequest, &transferPlane);
            }
            packageIdentity = package.packageIdentity;
            manifestHash = package.manifestHash;
            resolvedLayerIndex = found->index;
            canonicalChannels = CanonicalRgbwsvtChannels(request.channels);
        }
        else
        {
            const MaterialPreviewMode previewMode =
                ResolvePreviewMode(request);
            TiffLayerLoadResult loaded;
            ProductionLayerRef layer;
            ProductionPackageIndex package;
            {
                std::scoped_lock lock{m_layerMutex};
                package = EnsureVerifiedPackageLocked(
                    absolutePackage, false).package;
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
            preview = MaterialPreviewComposer::Compose(
                *loaded.buffer, composeRequest);
            packageIdentity = package.packageIdentity;
            manifestHash = package.manifestHash;
            resolvedLayerIndex = layer.layerIndex;
            canonicalChannels = CanonicalChannels(request.channels);
        }
        // 生产 TIFF 的第 0 行对应最小 Y；显示图像的第 0 行位于顶部。
        // 垂直翻转只改变预览表达，使结果页与工作区统一为 +Y 向上。
        preview = OrientPreviewPositiveYUp(std::move(preview));
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
            "pkg:" + packageIdentity
            + "|manifest:" + manifestHash
            + "|layer:" + std::to_string(resolvedLayerIndex)
            + "|mode:" + request.mode
            + "|ch:" + canonicalChannels
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
