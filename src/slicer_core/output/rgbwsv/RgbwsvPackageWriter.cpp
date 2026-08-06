#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"

#include "slicer_core/api/artifacts/PackageArtifactSafety.h"
#include "slicer_core/json_value.h"
#include "slicer_core/reports/ReportWriter.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/tiff_io.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

constexpr std::size_t kChannelCount{6U};
constexpr std::array<const char*, kChannelCount> kChannelOrder{
    "R", "G", "B", "W", "S", "V"};
constexpr std::array<std::uint8_t, 3> kEmptyPreviewColor{255U, 255U, 255U};
constexpr std::array<std::uint8_t, 3> kWhitePreviewColor{0U, 174U, 239U};
constexpr std::array<std::uint8_t, 3> kSupportPreviewColor{64U, 240U, 80U};
constexpr std::array<std::uint8_t, 3> kVarnishPreviewColor{127U, 127U, 127U};
constexpr std::size_t kCancellationCheckStride{4096U};
using WriterClock = std::chrono::steady_clock;

#include "slicer_core/output/rgbwsv/RgbwsvPackageCancellation.h"

double ElapsedMilliseconds(const WriterClock::time_point& start)
{
    return std::chrono::duration<double, std::milli>(
               WriterClock::now() - start)
        .count();
}

struct LayerChannelStats
{
    std::array<std::uint64_t, kChannelCount> printPixels{};
    std::array<std::uint64_t, kChannelCount> emptyPixels{};
};

bool HasFixedChannelOrder(const std::array<std::string, 6>& channelOrder)
{
    for (std::size_t index{0U}; index < kChannelOrder.size(); ++index)
    {
        if (channelOrder.at(index) != kChannelOrder.at(index))
        {
            return false;
        }
    }
    return true;
}

std::string LayerNumber(const int layerIndex)
{
    std::ostringstream stream;
    stream << std::setw(6) << std::setfill('0') << layerIndex;
    return stream.str();
}

api::artifacts::PackageArtifactIdentity MakeWriterArtifactIdentity(
    const RgbwsvProductionPackageWriteRequest& request,
    const std::filesystem::path& packageDir)
{
    if (request.jobId.empty() != request.attemptId.empty())
    {
        throw std::invalid_argument(
            "RGBWSV package jobId and attemptId must be provided together");
    }
    if (!request.jobId.empty())
    {
        return api::artifacts::MakePackageArtifactIdentity(
            packageDir,
            request.jobId,
            request.attemptId);
    }

    static std::atomic<std::uint64_t> directAttemptCounter{0U};
    const std::string correlation =
        packageDir.generic_string()
        + ":"
        + std::to_string(
            WriterClock::now().time_since_epoch().count())
        + ":"
        + std::to_string(directAttemptCounter.fetch_add(1U));
    return api::artifacts::MakePackageArtifactIdentity(
        packageDir,
        "direct",
        api::artifacts::MakePackageAttemptId(correlation));
}

bool IsStrictPackageValid(const std::filesystem::path& packageDir)
{
    try
    {
        (void)internal::ValidateSlicePackageArtifact(packageDir);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void ValidateStorage(const RgbwsvProductionStorageSpec& storage)
{
    (void)ParseTiffCompressionMode(storage.compression);
    if (storage.storageMode == "stripped")
    {
        if (storage.rowsPerStrip <= 0)
        {
            throw std::invalid_argument(
                "RGBWSV package rowsPerStrip must be positive");
        }
        return;
    }
    if (storage.storageMode == "tiled")
    {
        if (storage.tileWidth <= 0 || storage.tileHeight <= 0)
        {
            throw std::invalid_argument(
                "RGBWSV package tile dimensions must be positive");
        }
        return;
    }
    throw std::invalid_argument(
        "RGBWSV package storageMode must be stripped or tiled");
}

void ValidatePreview(const RgbwsvProductionPreviewSpec& preview)
{
    if (preview.outputpolicy != "tiff_native"
        && preview.outputpolicy != "tiff_native_with_diagnostics")
    {
        throw std::invalid_argument(
            "RGBWSV package preview outputPolicy must be tiff_native or tiff_native_with_diagnostics");
    }
    const bool expectedEnabled =
        preview.outputpolicy == "tiff_native_with_diagnostics";
    if (preview.enabled != expectedEnabled)
    {
        throw std::invalid_argument(
            "RGBWSV package preview enabled must match outputPolicy");
    }
    if (preview.interval <= 0)
    {
        throw std::invalid_argument(
            "RGBWSV package preview interval must be positive");
    }
    if (preview.format != "ppm" && preview.format != "png")
    {
        throw std::invalid_argument(
            "RGBWSV package preview format must be ppm or png");
    }
}

std::string ExpectedProductionAcceptance(
    const RgbwsvProductionPackageWriteRequest& request)
{
    if (!request.scene.has_value())
    {
        return "admitted";
    }
    return request.scene->manifestsummary.at("productionReady").as_bool()
        ? "admitted"
        : "functional_fixture_admitted";
}

bool IsWhiteSemanticsValid(const std::optional<std::string>& value)
{
    return !value.has_value()
        || *value == "opaque"
        || *value == "transparent";
}

std::optional<std::string> ResolveWhiteSemantics(
    const RgbwsvProductionPackageWriteRequest& request)
{
    if (!IsWhiteSemanticsValid(request.manifestWhiteSemantics)
        || !IsWhiteSemanticsValid(request.profileWhiteSemantics))
    {
        throw std::invalid_argument(
            "RGBWSV package whiteSemantics must be opaque or transparent");
    }
    if (request.manifestWhiteSemantics.has_value()
        && request.profileWhiteSemantics.has_value()
        && request.manifestWhiteSemantics
            != request.profileWhiteSemantics)
    {
        throw std::runtime_error(
            "RGBWSV package whiteSemantics conflicts with Profile default");
    }
    if (request.manifestWhiteSemantics.has_value())
    {
        return request.manifestWhiteSemantics;
    }
    return request.profileWhiteSemantics;
}

void ValidateRequest(const RgbwsvProductionPackageWriteRequest& request)
{
    ThrowIfCancellationRequested(
        request.canceltoken,
        "request_validation");
    (void)ResolveWhiteSemantics(request);
    if (request.packageDir.empty())
    {
        throw std::invalid_argument(
            "RGBWSV production package directory is required");
    }
    if (request.scene.has_value())
    {
        const std::string expectedPackagePath =
            std::filesystem::absolute(request.packageDir)
                .lexically_normal()
                .generic_string();
        const MultiModelSceneReportDocument& scene =
            *request.scene;
        if (!scene.IsValid()
            || scene.report.at("requestedPipelineMode").as_string()
                != request.requestedPipelineMode
            || scene.report.at("effectivePipelineMode").as_string()
                != request.effectivePipelineMode
            || scene.report.at("package").at("path").as_string()
                != expectedPackagePath)
        {
            throw std::invalid_argument(
                "RGBWSV production package scene extension is invalid");
        }
    }
    if (request.productionAcceptance
        != ExpectedProductionAcceptance(request))
    {
        throw std::runtime_error(
            "RGBWSV production package is blocked: acceptance does not match scene readiness");
    }
    if (request.requestedPipelineMode.empty()
        || request.requestedPipelineMode != request.effectivePipelineMode)
    {
        throw std::runtime_error(
            "RGBWSV production package is blocked: requested and effective pipeline modes differ");
    }
    if (request.effectivePipelineMode != "legacy"
        && request.effectivePipelineMode != "global_surface_shell")
    {
        throw std::invalid_argument(
            "RGBWSV production package pipeline mode is unsupported");
    }
    if (request.grid.widthPx <= 0
        || request.grid.heightPx <= 0
        || request.grid.layerCount <= 0
        || !IsSupportedOutputDpi(request.grid.dpiX)
        || !IsSupportedOutputDpi(request.grid.dpiY)
        || !IsOutputPixelSizeConsistent(
            request.grid.dpiX,
            request.grid.pixelSizeXmm)
        || !IsOutputPixelSizeConsistent(
            request.grid.dpiY,
            request.grid.pixelSizeYmm)
        || request.grid.layerThicknessMm <= 0.0)
    {
        throw std::invalid_argument(
            "RGBWSV production package grid is invalid");
    }
    if (request.layers.size()
        != static_cast<std::size_t>(request.grid.layerCount))
    {
        throw std::invalid_argument(
            "RGBWSV production package layer count does not match grid");
    }
    if (request.outerVarnish.enabled
        && (request.outerVarnish.requested_thickness_mm <= 0.0
            || request.outerVarnish.radius_x_px <= 0
            || request.outerVarnish.radius_y_px <= 0
            || std::abs(
                   request.outerVarnish.pixel_size_x_mm
                   - request.grid.pixelSizeXmm) > kOutputPixelSizeToleranceMm
            || std::abs(
                   request.outerVarnish.pixel_size_y_mm
                   - request.grid.pixelSizeYmm) > kOutputPixelSizeToleranceMm
            || std::abs(
                   request.outerVarnish.effective_thickness_x_mm
                   - static_cast<double>(
                         request.outerVarnish.radius_x_px)
                       * request.grid.pixelSizeXmm)
                > kOutputPixelSizeToleranceMm
            || std::abs(
                   request.outerVarnish.effective_thickness_y_mm
                   - static_cast<double>(
                         request.outerVarnish.radius_y_px)
                       * request.grid.pixelSizeYmm)
                > kOutputPixelSizeToleranceMm))
    {
        throw std::invalid_argument(
            "RGBWSV production package outer varnish discretization is invalid");
    }

    ValidateStorage(request.storage);
    ValidatePreview(request.preview);

    const std::size_t expectedBytes =
        static_cast<std::size_t>(request.grid.widthPx)
        * static_cast<std::size_t>(request.grid.heightPx)
        * kChannelCount;
    for (int layerIndex{0}; layerIndex < request.grid.layerCount; ++layerIndex)
    {
        ThrowIfCancellationRequested(
            request.canceltoken,
            "layer_validation");
        const RgbwsvProductionLayer& layer =
            request.layers.at(static_cast<std::size_t>(layerIndex));
        if (layer.layerIndex != layerIndex
            || !std::isfinite(layer.zMm)
            || layer.widthPx != request.grid.widthPx
            || layer.heightPx != request.grid.heightPx
            || !HasFixedChannelOrder(layer.channelOrder)
            || layer.channels.size() != expectedBytes)
        {
            throw std::invalid_argument(
                "RGBWSV production package contains an invalid or non-contiguous layer");
        }
    }
}

TiffImageSpec MakeTiffSpec(
    const RgbwsvProductionStorageSpec& storage,
    const int widthPx,
    const int heightPx)
{
    TiffImageSpec spec;
    spec.width = static_cast<std::uint32_t>(widthPx);
    spec.height = static_cast<std::uint32_t>(heightPx);
    spec.samples_per_pixel = static_cast<std::uint16_t>(kChannelCount);
    spec.bits_per_sample = 8U;
    spec.planar_config = 1U;
    spec.rows_per_strip = static_cast<std::uint32_t>(storage.rowsPerStrip);
    spec.tile_width = static_cast<std::uint32_t>(storage.tileWidth);
    spec.tile_height = static_cast<std::uint32_t>(storage.tileHeight);
    spec.storage_mode = storage.storageMode == "tiled"
        ? TiffStorageMode::Tiled
        : TiffStorageMode::Stripped;
    spec.compression_mode = ParseTiffCompressionMode(storage.compression);
    return spec;
}

LayerChannelStats CalculateLayerStats(
    const std::span<const std::uint8_t> channels,
    const api::ICancelToken* cancelToken)
{
    LayerChannelStats stats;
    for (std::size_t offset{0U}; offset < channels.size(); ++offset)
    {
        if ((offset % kCancellationCheckStride) == 0U)
        {
            ThrowIfCancellationRequested(
                cancelToken,
                "layer_statistics");
        }
        const std::size_t channel = offset % kChannelCount;
        if (channels[offset] == 255U)
        {
            ++stats.emptyPixels.at(channel);
        }
        else
        {
            ++stats.printPixels.at(channel);
        }
    }
    return stats;
}

Json StringArrayToJson(const std::array<std::string, 6>& values)
{
    Json::Array result;
    result.reserve(values.size());
    for (const std::string& value : values)
    {
        result.emplace_back(value);
    }
    return Json{std::move(result)};
}

Json ChannelCountsToJson(
    const std::array<std::uint64_t, kChannelCount>& counts)
{
    Json::Object result;
    for (std::size_t index{0U}; index < kChannelOrder.size(); ++index)
    {
        result[kChannelOrder.at(index)] = counts.at(index);
    }
    return Json{std::move(result)};
}

void AppendBigEndianUint32(
    std::vector<std::uint8_t>& data,
    const std::uint32_t value)
{
    data.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    data.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

std::uint32_t UpdateCrc32(
    std::uint32_t crc,
    const std::uint8_t byte)
{
    crc ^= byte;
    for (int bit{0}; bit < 8; ++bit)
    {
        const std::uint32_t mask = 0U - (crc & 1U);
        crc = (crc >> 1U) ^ (0xedb88320U & mask);
    }
    return crc;
}

std::uint32_t CalculateChunkCrc32(
    const std::array<char, 4>& type,
    const std::vector<std::uint8_t>& payload)
{
    std::uint32_t crc{0xffffffffU};
    for (const char value : type)
    {
        crc = UpdateCrc32(crc, static_cast<std::uint8_t>(value));
    }
    for (const std::uint8_t value : payload)
    {
        crc = UpdateCrc32(crc, value);
    }
    return crc ^ 0xffffffffU;
}

std::uint32_t CalculateAdler32(const std::vector<std::uint8_t>& data)
{
    std::uint32_t a{1U};
    std::uint32_t b{0U};
    for (const std::uint8_t value : data)
    {
        a = (a + value) % 65521U;
        b = (b + a) % 65521U;
    }
    return (b << 16U) | a;
}

std::vector<std::uint8_t> EncodeStoredZlibBlocks(
    const std::vector<std::uint8_t>& raw)
{
    std::vector<std::uint8_t> result;
    result.reserve(raw.size() + raw.size() / 65535U * 5U + 16U);
    result.push_back(0x78U);
    result.push_back(0x01U);
    std::size_t offset{0U};
    while (offset < raw.size())
    {
        const std::size_t blockSize =
            std::min<std::size_t>(65535U, raw.size() - offset);
        const bool finalBlock = offset + blockSize == raw.size();
        result.push_back(finalBlock ? 0x01U : 0x00U);
        const auto length = static_cast<std::uint16_t>(blockSize);
        const auto invertedLength = static_cast<std::uint16_t>(~length);
        result.push_back(static_cast<std::uint8_t>(length & 0xffU));
        result.push_back(static_cast<std::uint8_t>((length >> 8U) & 0xffU));
        result.push_back(
            static_cast<std::uint8_t>(invertedLength & 0xffU));
        result.push_back(
            static_cast<std::uint8_t>((invertedLength >> 8U) & 0xffU));
        result.insert(
            result.end(),
            raw.begin() + static_cast<std::ptrdiff_t>(offset),
            raw.begin() + static_cast<std::ptrdiff_t>(offset + blockSize));
        offset += blockSize;
    }
    AppendBigEndianUint32(result, CalculateAdler32(raw));
    return result;
}

void WritePngChunk(
    std::ofstream& output,
    const std::array<char, 4>& type,
    const std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> encodedValue;
    AppendBigEndianUint32(
        encodedValue,
        static_cast<std::uint32_t>(payload.size()));
    output.write(
        reinterpret_cast<const char*>(encodedValue.data()),
        static_cast<std::streamsize>(encodedValue.size()));
    output.write(type.data(), static_cast<std::streamsize>(type.size()));
    if (!payload.empty())
    {
        output.write(
            reinterpret_cast<const char*>(payload.data()),
            static_cast<std::streamsize>(payload.size()));
    }
    encodedValue.clear();
    AppendBigEndianUint32(
        encodedValue,
        CalculateChunkCrc32(type, payload));
    output.write(
        reinterpret_cast<const char*>(encodedValue.data()),
        static_cast<std::streamsize>(encodedValue.size()));
}

#include "slicer_core/output/rgbwsv/RgbwsvPreviewIo.h"

std::vector<std::array<std::uint8_t, 3>> BuildRgbPreview(
    const RgbwsvProductionLayer& layer,
    const api::ICancelToken* cancelToken)
{
    const std::size_t pixelCount =
        static_cast<std::size_t>(layer.widthPx)
        * static_cast<std::size_t>(layer.heightPx);
    std::vector<std::array<std::uint8_t, 3>> result(
        pixelCount,
        kEmptyPreviewColor);
    for (std::size_t pixelIndex{0U}; pixelIndex < pixelCount; ++pixelIndex)
    {
        if ((pixelIndex % kCancellationCheckStride) == 0U)
        {
            ThrowIfCancellationRequested(
                cancelToken,
                "preview_rgb_build");
        }
        const std::size_t source = pixelIndex * kChannelCount;
        result.at(pixelIndex) = {
            layer.channels.at(source),
            layer.channels.at(source + 1U),
            layer.channels.at(source + 2U)};
    }
    return result;
}

std::vector<std::array<std::uint8_t, 3>> BuildChannelPreview(
    const RgbwsvProductionLayer& layer,
    const std::size_t channel,
    const std::array<std::uint8_t, 3>& printColor,
    const api::ICancelToken* cancelToken)
{
    const std::size_t pixelCount =
        static_cast<std::size_t>(layer.widthPx)
        * static_cast<std::size_t>(layer.heightPx);
    std::vector<std::array<std::uint8_t, 3>> result(
        pixelCount,
        kEmptyPreviewColor);
    for (std::size_t pixelIndex{0U}; pixelIndex < pixelCount; ++pixelIndex)
    {
        if ((pixelIndex % kCancellationCheckStride) == 0U)
        {
            ThrowIfCancellationRequested(
                cancelToken,
                "preview_channel_build");
        }
        if (layer.channels.at(pixelIndex * kChannelCount + channel) != 255U)
        {
            result.at(pixelIndex) = printColor;
        }
    }
    return result;
}

Json MakePreviewEntry(
    const int layerIndex,
    const std::string& channel,
    const std::string& relativePath)
{
    return Json::object({
        {"layerIndex", layerIndex},
        {"channel", channel},
        {"path", relativePath},
    });
}

void WriteLayerPreviews(
    const std::filesystem::path& stagingDir,
    const RgbwsvProductionPackageWriteRequest& request,
    const RgbwsvProductionLayer& layer,
    Json::Array& generated)
{
    if (!request.preview.enabled
        || layer.layerIndex % request.preview.interval != 0)
    {
        return;
    }
    const std::string number = LayerNumber(layer.layerIndex);
    const std::string extension = "." + request.preview.format;
    const std::array<std::string, 4> names{
        "rgb_" + number + extension,
        "white_w_" + number + extension,
        "support_s_" + number + extension,
        "varnish_v_" + number + extension};
    const std::array<std::string, 4> channels{"RGB", "W", "S", "V"};
    const std::array<std::vector<std::array<std::uint8_t, 3>>, 4> images{
        BuildRgbPreview(layer, request.canceltoken),
        BuildChannelPreview(
            layer,
            3U,
            kWhitePreviewColor,
            request.canceltoken),
        BuildChannelPreview(
            layer,
            4U,
            kSupportPreviewColor,
            request.canceltoken),
        BuildChannelPreview(
            layer,
            5U,
            kVarnishPreviewColor,
            request.canceltoken)};

    for (std::size_t index{0U}; index < images.size(); ++index)
    {
        ThrowIfCancellationRequested(
            request.canceltoken,
            "preview_image_write");
        const std::string relativePath = "preview/" + names.at(index);
        WritePreviewImage(
            stagingDir / relativePath,
            layer.widthPx,
            layer.heightPx,
            images.at(index),
            request.preview.format,
            request.canceltoken);
        generated.push_back(
            MakePreviewEntry(
                layer.layerIndex,
                channels.at(index),
                relativePath));
    }
}

Json MakeLayerEntry(
    const RgbwsvProductionLayer& layer,
    const LayerChannelStats& stats,
    const std::string& relativePath)
{
    return Json::object({
        {"index", layer.layerIndex},
        {"zMm", layer.zMm},
        {"path", relativePath},
        {"widthPx", layer.widthPx},
        {"heightPx", layer.heightPx},
        {"printPixels", ChannelCountsToJson(stats.printPixels)},
        {"emptyPixels", ChannelCountsToJson(stats.emptyPixels)},
    });
}

Json MakeTiffJson(
    const RgbwsvProtocol& protocol,
    const RgbwsvProductionStorageSpec& storage,
    const Json::Array& layers)
{
    Json::Object result;
    result["channelOrder"] = StringArrayToJson(protocol.channel_order);
    result["channelCount"] = static_cast<int>(kChannelCount);
    result["bitDepth"] = protocol.bit_depth;
    result["sampleFormat"] = "uint";
    result["planarConfig"] = "contiguous";
    result["tiled"] = storage.storageMode == "tiled";
    result["storage"] = storage.storageMode;
    result["storageMode"] = storage.storageMode;
    result["compression"] = storage.compression;
    result["polarity"] = protocol.polarity;
    result["printValue"] = static_cast<int>(protocol.print_value);
    result["emptyValue"] = static_cast<int>(protocol.empty_value);
    result["layers"] = Json{layers};
    if (storage.storageMode == "tiled")
    {
        result["tileSize"] = Json::array(
            {storage.tileWidth, storage.tileHeight});
    }
    else
    {
        result["rowsPerStrip"] = storage.rowsPerStrip;
    }
    return Json{std::move(result)};
}

Json MakeGridJson(const RgbwsvProductionGridSpec& grid)
{
    return Json::object({
        {"widthPx", grid.widthPx},
        {"heightPx", grid.heightPx},
        {"layerCount", grid.layerCount},
        {"dpiX", grid.dpiX},
        {"dpiY", grid.dpiY},
        {"dpi", Json::array({grid.dpiX, grid.dpiY})},
        {"pixelSizeXmm", grid.pixelSizeXmm},
        {"pixelSizeYmm", grid.pixelSizeYmm},
        {"pixelSizeMm", Json::array({grid.pixelSizeXmm, grid.pixelSizeYmm})},
        {"layerThicknessMm", grid.layerThicknessMm},
        {"originMm",
         Json::array({grid.originXmm, grid.originYmm, grid.originZmm})},
    });
}

Json MakeOuterVarnishJson(
    const OuterVarnishDiscretization& discretization)
{
    return Json::object({
        {"enabled", discretization.enabled},
        {"requestedThicknessMm", discretization.requested_thickness_mm},
        {"radiusXPx", discretization.radius_x_px},
        {"radiusYPx", discretization.radius_y_px},
        {"effectiveThicknessXmm",
         discretization.effective_thickness_x_mm},
        {"effectiveThicknessYmm",
         discretization.effective_thickness_y_mm},
        {"pixelSizeXmm", discretization.pixel_size_x_mm},
        {"pixelSizeYmm", discretization.pixel_size_y_mm},
        {"pixelPitchSource", "output_dpi"},
    });
}

Json ReadPersistedJson(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        throw std::runtime_error(
            "failed to reopen staged JSON: " + path.string());
    }
    return Json::parse(input);
}

void ValidatePersistedSceneExtension(
    const std::filesystem::path& stagingDir,
    const RgbwsvProductionPackageWriteRequest& request)
{
    if (!request.scene.has_value())
    {
        return;
    }

    const Json manifest =
        ReadPersistedJson(stagingDir / "manifest.json");
    const Json report = ReadPersistedJson(
        stagingDir / MultiModelSceneReportRelativePath());
    MultiModelSceneReportDocument persisted;
    persisted.manifestsummary = manifest.at("scene");
    persisted.report = report;
    if (!persisted.IsValid()
        || persisted.manifestsummary.dump(0)
            != request.scene->manifestsummary.dump(0)
        || persisted.report.dump(0)
            != request.scene->report.dump(0)
        || manifest.at("reports").at("scene").as_string()
            != MultiModelSceneReportRelativePath().generic_string())
    {
        throw std::runtime_error(
            "staged RGBWSV scene report failed persistence validation");
    }
}

bool IsTransientRenameError(const std::error_code& error)
{
    return error == std::errc::permission_denied
        || error == std::errc::device_or_resource_busy
        || error == std::errc::resource_unavailable_try_again
        || error == std::errc::operation_not_permitted;
}

void RenameWithTransientRetry(
    const std::filesystem::path& source,
    const std::filesystem::path& destination)
{
    constexpr int kMaximumAttempts{6};
    constexpr auto kInitialDelay{std::chrono::milliseconds{20}};
    std::error_code error;
    for (int attempt{0}; attempt < kMaximumAttempts; ++attempt)
    {
        error.clear();
        std::filesystem::rename(source, destination, error);
        if (!error)
        {
            return;
        }
        if (!IsTransientRenameError(error)
            || attempt + 1 == kMaximumAttempts)
        {
            throw std::filesystem::filesystem_error(
                "rename",
                source,
                destination,
                error);
        }
        std::this_thread::sleep_for(kInitialDelay * (1 << attempt));
    }
}

void PublishStagedPackage(
    const api::artifacts::PackageArtifactIdentity& identity)
{
    if (std::filesystem::exists(identity.backup_directory))
    {
        throw api::artifacts::PackageArtifactOutputError(
            "owned package backup still exists before publication");
    }
    if (std::filesystem::exists(identity.package_directory))
    {
        RenameWithTransientRetry(
            identity.package_directory,
            identity.backup_directory);
    }

    try
    {
        RenameWithTransientRetry(
            identity.staging_directory,
            identity.package_directory);
    }
    catch (...)
    {
        if (!std::filesystem::exists(identity.package_directory)
            && std::filesystem::exists(identity.backup_directory))
        {
            try
            {
                RenameWithTransientRetry(
                    identity.backup_directory,
                    identity.package_directory);
            }
            catch (const std::filesystem::filesystem_error& error)
            {
                throw std::runtime_error(
                    "failed to publish staged RGBWSV package and restore the previous package: "
                    + error.code().message());
            }
        }
        throw;
    }
}

}  // namespace

void WriteRgbwsvProductionLayerTiff(
    const std::filesystem::path& path,
    const RgbwsvProductionStorageSpec& storage,
    const RgbwsvProductionLayerView& layer)
{
    ValidateStorage(storage);
    if (layer.widthPx <= 0 || layer.heightPx <= 0)
    {
        throw std::invalid_argument(
            "RGBWSV production TIFF dimensions must be positive");
    }
    const std::size_t expectedBytes =
        static_cast<std::size_t>(layer.widthPx)
        * static_cast<std::size_t>(layer.heightPx)
        * kChannelCount;
    if (layer.channels.size() != expectedBytes)
    {
        throw std::invalid_argument(
            "RGBWSV production TIFF byte count does not match dimensions");
    }

    const TiffImageSpec spec =
        MakeTiffSpec(storage, layer.widthPx, layer.heightPx);
    write_rgbwsv_tiff(path, spec, layer.channels);
}

RgbwsvProductionPackageWriteResult WriteRgbwsvProductionPackage(
    const RgbwsvProductionPackageWriteRequest& request)
{
    const WriterClock::time_point totalStart = WriterClock::now();
    RgbwsvProductionPackageWriteProfile profile;
    ValidateRequest(request);
    const std::optional<std::string> whiteSemantics =
        ResolveWhiteSemantics(request);

    const std::filesystem::path packageDir =
        std::filesystem::absolute(request.packageDir).lexically_normal();
    const api::artifacts::PackageArtifactIdentity artifactIdentity =
        MakeWriterArtifactIdentity(request, packageDir);
    const std::filesystem::path& stagingDir =
        artifactIdentity.staging_directory;

    const api::artifacts::PackageArtifactRecoveryResult initialRecovery =
        api::artifacts::RecoverPackageArtifacts(
            artifactIdentity,
            IsStrictPackageValid);
    if (!initialRecovery.success)
    {
        if (initialRecovery.error
            == "package lease is not owned by this attempt")
        {
            throw api::artifacts::PackageArtifactLeaseConflict(
                initialRecovery.error);
        }
        throw api::artifacts::PackageArtifactOutputError(
            initialRecovery.error.empty()
                ? "failed to recover owned package artifacts"
                : initialRecovery.error);
    }

    const api::artifacts::PackageArtifactLeaseResult lease =
        api::artifacts::AcquirePackageArtifactLease(artifactIdentity);
    if (!lease.success)
    {
        if (lease.conflict)
        {
            throw api::artifacts::PackageArtifactLeaseConflict(
                lease.error);
        }
        throw api::artifacts::PackageArtifactOutputError(
            lease.error.empty()
                ? "failed to acquire package publication lease"
                : lease.error);
    }

    try
    {
        ThrowIfCancellationRequested(
            request.canceltoken,
            "staging_setup");
        std::filesystem::create_directories(stagingDir / "layers");
        std::filesystem::create_directories(stagingDir / "reports");
        if (request.preview.enabled)
        {
            std::filesystem::create_directories(stagingDir / "preview");
        }

        const RgbwsvProtocol protocol = CurrentRgbwsvProtocol();
        Json::Array layers;
        Json::Array layerStats;
        Json::Array generatedPreviews;
        std::array<std::uint64_t, kChannelCount> totalPrintPixels{};
        std::array<std::uint64_t, kChannelCount> totalEmptyPixels{};

        int writtenLayerCount{0};
        for (const RgbwsvProductionLayer& layer : request.layers)
        {
            ThrowIfCancellationRequested(
                request.canceltoken,
                "before_layer_tiff");
            const std::string relativePath =
                "layers/layer_" + LayerNumber(layer.layerIndex) + ".tiff";
            const WriterClock::time_point tiffStart =
                WriterClock::now();
            WriteRgbwsvProductionLayerTiff(
                stagingDir / relativePath,
                request.storage,
                RgbwsvProductionLayerView{
                    layer.widthPx,
                    layer.heightPx,
                    layer.channels});
            profile.tiffwritems +=
                ElapsedMilliseconds(tiffStart);
            ThrowIfCancellationRequested(
                request.canceltoken,
                "after_layer_tiff");

            const WriterClock::time_point layerReportStart =
                WriterClock::now();
            const LayerChannelStats stats =
                CalculateLayerStats(
                    layer.channels,
                    request.canceltoken);
            for (std::size_t channel{0U};
                 channel < kChannelCount;
                 ++channel)
            {
                totalPrintPixels.at(channel) +=
                    stats.printPixels.at(channel);
                totalEmptyPixels.at(channel) +=
                    stats.emptyPixels.at(channel);
            }
            const Json entry = MakeLayerEntry(layer, stats, relativePath);
            layers.push_back(entry);
            layerStats.push_back(entry);
            profile.reportbuildms +=
                ElapsedMilliseconds(layerReportStart);

            const WriterClock::time_point previewStart =
                WriterClock::now();
            WriteLayerPreviews(
                stagingDir,
                request,
                layer,
                generatedPreviews);
            ThrowIfCancellationRequested(
                request.canceltoken,
                "after_layer_preview");
            profile.previewwritems +=
                ElapsedMilliseconds(previewStart);
            ++writtenLayerCount;
            if (request.layerwritecallback)
            {
                request.layerwritecallback(
                    writtenLayerCount,
                    static_cast<int>(request.layers.size()));
            }
        }

        const WriterClock::time_point reportBuildStart =
            WriterClock::now();
        ThrowIfCancellationRequested(
            request.canceltoken,
            "report_build");
        const Json previewReport = Json::object({
            {"schema", "p0.preview_report.1"},
            {"outputPolicy", request.preview.outputpolicy},
            {"productionSource", "rgbwsv_tiff"},
            {"automaticDiagnosticImages", request.preview.enabled},
            {"enabled", request.preview.enabled},
            {"format", request.preview.format},
            {"interval", request.preview.interval},
            {"displayOnly", true},
            {"productionValuesReinterpreted", true},
            {"channels", Json::array({"RGB", "W", "S", "V"})},
            {"generated", Json{generatedPreviews}},
            {"files", Json{generatedPreviews}},
        });
        Json::Object sliceReportFields{
            {"schema", "p0.slice_report.1"},
            {"status",
             request.scene.has_value()
                     && !request.scene->manifestsummary
                             .at("productionReady")
                             .as_bool()
                 ? "functional_fixture_written"
                 : "production_written"},
            {"requestedPipelineMode", request.requestedPipelineMode},
            {"effectivePipelineMode", request.effectivePipelineMode},
            {"productionAcceptance", request.productionAcceptance},
            {"productionOutputWritten", true},
            {"fallbackApplied", false},
            {"productionTiffLayerCount", request.grid.layerCount},
            {"materialSemantics",
             Json::object({
                 {"outerVarnish",
                  MakeOuterVarnishJson(request.outerVarnish)},
             })},
            {"layerStats", Json{layerStats}},
            {"totals",
             Json::object({
                 {"printPixels", ChannelCountsToJson(totalPrintPixels)},
                 {"emptyPixels", ChannelCountsToJson(totalEmptyPixels)},
            })},
        };
        if (request.productionSettings.has_value())
        {
            sliceReportFields["productionSettings"] =
                *request.productionSettings;
        }
        const Json sliceReport{std::move(sliceReportFields)};
        Json::Object reportLinks{
            {"slice", "reports/slice_report.json"},
            {"preview", "reports/preview_report.json"},
        };
        if (request.scene.has_value())
        {
            reportLinks["scene"] =
                MultiModelSceneReportRelativePath().generic_string();
        }
        Json::Object manifestObject{
            {"schema", protocol.schema},
            {"schemaVersion", protocol.schema},
            {"requestedPipelineMode", request.requestedPipelineMode},
            {"effectivePipelineMode", request.effectivePipelineMode},
            {"productionAcceptance", request.productionAcceptance},
            {"productionOutputWritten", true},
            {"fallbackApplied", false},
            {"source",
             Json::object({
                 {"configPath", request.sourceConfigPath.generic_string()},
                 {"modelPath", request.sourceModelPath.generic_string()},
                 {"format", request.sourceFormat},
                 {"engine", request.effectivePipelineMode},
             })},
            {"grid", MakeGridJson(request.grid)},
            {"tiff", MakeTiffJson(protocol, request.storage, layers)},
            {"layers", Json{layers}},
            {"reports", Json{std::move(reportLinks)}},
            {"preview",
             Json::object({
                 {"outputPolicy", request.preview.outputpolicy},
                 {"productionSource", "rgbwsv_tiff"},
                 {"automaticDiagnosticImages", request.preview.enabled},
                 {"enabled", request.preview.enabled},
                 {"format", request.preview.format},
                 {"files", Json{generatedPreviews}},
            })},
        };
        if (whiteSemantics.has_value())
        {
            manifestObject["whiteSemantics"] = *whiteSemantics;
        }
        if (request.scene.has_value())
        {
            manifestObject["scene"] =
                request.scene->manifestsummary;
        }
        const Json manifest{std::move(manifestObject)};
        profile.reportbuildms +=
            ElapsedMilliseconds(reportBuildStart);

        const WriterClock::time_point reportWriteStart =
            WriterClock::now();
        ThrowIfCancellationRequested(
            request.canceltoken,
            "report_write");
        WriteReportJsonFile(stagingDir / "manifest.json", manifest);
        WriteReportJsonFile(
            stagingDir / "reports" / "slice_report.json",
            sliceReport);
        WriteReportJsonFile(
            stagingDir / "reports" / "preview_report.json",
            previewReport);
        if (request.scene.has_value())
        {
            WriteReportJsonFile(
                stagingDir / MultiModelSceneReportRelativePath(),
                request.scene->report);
        }
        profile.reportwritems =
            ElapsedMilliseconds(reportWriteStart);

        const WriterClock::time_point publishStart =
            WriterClock::now();
        ThrowIfCancellationRequested(
            request.canceltoken,
            "package_validation");
        ValidatePersistedSceneExtension(stagingDir, request);
        (void)internal::ValidateSlicePackageArtifact(stagingDir);
        ThrowIfCancellationRequested(
            request.canceltoken,
            "before_package_publish");
        PublishStagedPackage(artifactIdentity);
        ValidatePersistedSceneExtension(packageDir, request);
        (void)validate_slice_package(packageDir);
        const api::artifacts::PackageArtifactRecoveryResult cleanup =
            api::artifacts::RecoverPackageArtifacts(
                artifactIdentity,
                IsStrictPackageValid);
        if (!cleanup.success)
        {
            throw api::artifacts::PackageArtifactOutputError(
                cleanup.error.empty()
                    ? "failed to remove owned package publication artifacts"
                    : cleanup.error);
        }
        profile.packagepublishms =
            ElapsedMilliseconds(publishStart);
        profile.totalms = ElapsedMilliseconds(totalStart);

        RgbwsvProductionPackageWriteResult result;
        result.productionOutputWritten = true;
        result.fallbackApplied = false;
        result.strictProtocolValidated = true;
        result.layerCount = request.grid.layerCount;
        result.jobId = artifactIdentity.job_id;
        result.attemptId = artifactIdentity.attempt_id;
        result.packageDir = packageDir;
        result.stagingRemoved = cleanup.staging_removed;
        result.backupRemoved = cleanup.backup_removed;
        result.leaseReleased = cleanup.lease_removed;
        result.profile = profile;
        return result;
    }
    catch (...)
    {
        const api::artifacts::PackageArtifactRecoveryResult recovery =
            api::artifacts::RecoverPackageArtifacts(
                artifactIdentity,
                IsStrictPackageValid);
        if (!recovery.success)
        {
            throw api::artifacts::PackageArtifactOutputError(
                recovery.error.empty()
                    ? "failed to recover package artifacts after publication failure"
                    : recovery.error);
        }
        throw;
    }
}

}  // namespace slicer_core
