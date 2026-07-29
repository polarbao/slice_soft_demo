#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"

#include "slicer_core/json_value.h"
#include "slicer_core/reports/ReportWriter.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/tiff_io.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
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
using WriterClock = std::chrono::steady_clock;

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

std::filesystem::path MakeSiblingTemporaryDirectory(
    const std::filesystem::path& packageDir,
    const std::string& suffix)
{
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    return packageDir.parent_path()
        / (packageDir.filename().string() + "." + suffix + "."
           + std::to_string(stamp));
}

void ValidateStorage(const RgbwsvProductionStorageSpec& storage)
{
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

void ValidateRequest(const RgbwsvProductionPackageWriteRequest& request)
{
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
    return spec;
}

LayerChannelStats CalculateLayerStats(
    const std::span<const std::uint8_t> channels)
{
    LayerChannelStats stats;
    for (std::size_t offset{0U}; offset < channels.size(); ++offset)
    {
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

void WritePpm(
    const std::filesystem::path& path,
    const int widthPx,
    const int heightPx,
    const std::vector<std::array<std::uint8_t, 3>>& pixels)
{
    std::ofstream output{path, std::ios::binary};
    if (!output)
    {
        throw std::runtime_error(
            "failed to write RGBWSV preview: " + path.string());
    }
    output << "P6\n" << widthPx << ' ' << heightPx << "\n255\n";
    for (const auto& pixel : pixels)
    {
        output.write(
            reinterpret_cast<const char*>(pixel.data()),
            static_cast<std::streamsize>(pixel.size()));
    }
}

void WritePng(
    const std::filesystem::path& path,
    const int widthPx,
    const int heightPx,
    const std::vector<std::array<std::uint8_t, 3>>& pixels)
{
    std::vector<std::uint8_t> raw;
    raw.reserve(
        static_cast<std::size_t>(heightPx)
        * (static_cast<std::size_t>(widthPx) * 3U + 1U));
    for (int y{0}; y < heightPx; ++y)
    {
        raw.push_back(0U);
        for (int x{0}; x < widthPx; ++x)
        {
            const auto& pixel = pixels.at(
                static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(widthPx)
                + static_cast<std::size_t>(x));
            raw.insert(raw.end(), pixel.begin(), pixel.end());
        }
    }

    std::ofstream output{path, std::ios::binary};
    if (!output)
    {
        throw std::runtime_error(
            "failed to write RGBWSV preview: " + path.string());
    }
    const std::array<std::uint8_t, 8> signature{
        137U, 80U, 78U, 71U, 13U, 10U, 26U, 10U};
    output.write(
        reinterpret_cast<const char*>(signature.data()),
        static_cast<std::streamsize>(signature.size()));

    std::vector<std::uint8_t> header;
    AppendBigEndianUint32(header, static_cast<std::uint32_t>(widthPx));
    AppendBigEndianUint32(header, static_cast<std::uint32_t>(heightPx));
    header.insert(header.end(), {8U, 2U, 0U, 0U, 0U});
    WritePngChunk(output, {'I', 'H', 'D', 'R'}, header);
    WritePngChunk(output, {'I', 'D', 'A', 'T'}, EncodeStoredZlibBlocks(raw));
    WritePngChunk(output, {'I', 'E', 'N', 'D'}, {});
}

void WritePreviewImage(
    const std::filesystem::path& path,
    const int widthPx,
    const int heightPx,
    const std::vector<std::array<std::uint8_t, 3>>& pixels,
    const std::string& format)
{
    if (format == "png")
    {
        WritePng(path, widthPx, heightPx, pixels);
        return;
    }
    WritePpm(path, widthPx, heightPx, pixels);
}

std::vector<std::array<std::uint8_t, 3>> BuildRgbPreview(
    const RgbwsvProductionLayer& layer)
{
    const std::size_t pixelCount =
        static_cast<std::size_t>(layer.widthPx)
        * static_cast<std::size_t>(layer.heightPx);
    std::vector<std::array<std::uint8_t, 3>> result(
        pixelCount,
        kEmptyPreviewColor);
    for (std::size_t pixelIndex{0U}; pixelIndex < pixelCount; ++pixelIndex)
    {
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
    const std::array<std::uint8_t, 3>& printColor)
{
    const std::size_t pixelCount =
        static_cast<std::size_t>(layer.widthPx)
        * static_cast<std::size_t>(layer.heightPx);
    std::vector<std::array<std::uint8_t, 3>> result(
        pixelCount,
        kEmptyPreviewColor);
    for (std::size_t pixelIndex{0U}; pixelIndex < pixelCount; ++pixelIndex)
    {
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
        BuildRgbPreview(layer),
        BuildChannelPreview(layer, 3U, kWhitePreviewColor),
        BuildChannelPreview(layer, 4U, kSupportPreviewColor),
        BuildChannelPreview(layer, 5U, kVarnishPreviewColor)};

    for (std::size_t index{0U}; index < images.size(); ++index)
    {
        const std::string relativePath = "preview/" + names.at(index);
        WritePreviewImage(
            stagingDir / relativePath,
            layer.widthPx,
            layer.heightPx,
            images.at(index),
            request.preview.format);
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

void PublishStagedPackage(
    const std::filesystem::path& stagingDir,
    const std::filesystem::path& packageDir,
    std::filesystem::path& backupDir)
{
    if (std::filesystem::exists(packageDir))
    {
        backupDir = MakeSiblingTemporaryDirectory(packageDir, "backup");
        std::filesystem::rename(packageDir, backupDir);
    }

    try
    {
        std::filesystem::rename(stagingDir, packageDir);
    }
    catch (...)
    {
        if (!backupDir.empty()
            && !std::filesystem::exists(packageDir)
            && std::filesystem::exists(backupDir))
        {
            std::error_code restoreError;
            std::filesystem::rename(
                backupDir,
                packageDir,
                restoreError);
            if (restoreError)
            {
                throw std::runtime_error(
                    "failed to publish staged RGBWSV package and restore the previous package: "
                    + restoreError.message());
            }
        }
        throw;
    }

    if (!backupDir.empty())
    {
        std::error_code cleanupError;
        std::filesystem::remove_all(backupDir, cleanupError);
        if (!cleanupError)
        {
            backupDir.clear();
        }
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

    const std::filesystem::path packageDir =
        std::filesystem::absolute(request.packageDir).lexically_normal();
    const std::filesystem::path stagingDir =
        MakeSiblingTemporaryDirectory(packageDir, "staging");
    std::filesystem::path backupDir;

    try
    {
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

            const WriterClock::time_point layerReportStart =
                WriterClock::now();
            const LayerChannelStats stats =
                CalculateLayerStats(layer.channels);
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
        const Json sliceReport = Json::object({
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
        });
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
        ValidatePersistedSceneExtension(stagingDir, request);
        (void)validate_slice_package(stagingDir);
        PublishStagedPackage(stagingDir, packageDir, backupDir);
        profile.packagepublishms =
            ElapsedMilliseconds(publishStart);
        profile.totalms = ElapsedMilliseconds(totalStart);

        RgbwsvProductionPackageWriteResult result;
        result.productionOutputWritten = true;
        result.fallbackApplied = false;
        result.strictProtocolValidated = true;
        result.layerCount = request.grid.layerCount;
        result.packageDir = packageDir;
        result.replacedPackageBackupDir = backupDir;
        result.profile = profile;
        return result;
    }
    catch (...)
    {
        std::error_code cleanupError;
        std::filesystem::remove_all(stagingDir, cleanupError);
        throw;
    }
}

}  // namespace slicer_core
