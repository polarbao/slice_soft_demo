#include "slicer_core/api/artifacts/PackageArtifactSafety.h"
#include "slicer_core/json_value.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"
#include "slicer_core/pipeline/GlobalSurfaceShellProductionPackage.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/tiff_io.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr int kWidth{6};
constexpr int kHeight{4};
constexpr int kLayerCount{2};
constexpr std::size_t kChannelCount{6U};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::filesystem::path MakeTestDirectory(const std::string& name)
{
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / ("slicesoft_rgbwsv_writer_" + name + "_" + std::to_string(suffix));
    std::filesystem::create_directories(directory);
    return directory;
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        throw std::runtime_error("failed to read test JSON: " + path.string());
    }
    return slicer_core::Json::parse(input);
}

void WriteJson(
    const std::filesystem::path& path,
    const slicer_core::Json& document)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        throw std::runtime_error("failed to write test JSON: " + path.string());
    }
    output << document.dump(2) << '\n';
}

void MutateManifestCompression(
    const std::filesystem::path& packageDir,
    const std::string& compression)
{
    const std::filesystem::path manifestPath = packageDir / "manifest.json";
    slicer_core::Json::Object root = ReadJson(manifestPath).as_object();
    slicer_core::Json::Object tiff = root.at("tiff").as_object();
    tiff["compression"] = compression;
    root["tiff"] = slicer_core::Json{std::move(tiff)};
    WriteJson(manifestPath, slicer_core::Json{std::move(root)});
}

void RemoveManifestCompression(const std::filesystem::path& packageDir)
{
    const std::filesystem::path manifestPath = packageDir / "manifest.json";
    slicer_core::Json::Object root = ReadJson(manifestPath).as_object();
    slicer_core::Json::Object tiff = root.at("tiff").as_object();
    tiff.erase("compression");
    root["tiff"] = slicer_core::Json{std::move(tiff)};
    WriteJson(manifestPath, slicer_core::Json{std::move(root)});
}

void MutateManifestWhiteSemantics(
    const std::filesystem::path& packageDir,
    const slicer_core::Json& semantics)
{
    const std::filesystem::path manifestPath = packageDir / "manifest.json";
    slicer_core::Json::Object root = ReadJson(manifestPath).as_object();
    root["whiteSemantics"] = semantics;
    WriteJson(manifestPath, slicer_core::Json{std::move(root)});
}

std::size_t ChannelIndex(
    const int x,
    const int y,
    const std::size_t channel)
{
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(kWidth)
            + static_cast<std::size_t>(x))
        * kChannelCount
        + channel;
}

slicer_core::RgbwsvProductionLayer MakeLayer(const int layerIndex)
{
    slicer_core::RgbwsvProductionLayer layer;
    layer.layerIndex = layerIndex;
    layer.zMm = (static_cast<double>(layerIndex) + 0.5) * 0.01;
    layer.widthPx = kWidth;
    layer.heightPx = kHeight;
    layer.channels.assign(
        static_cast<std::size_t>(kWidth * kHeight) * kChannelCount,
        255U);

    layer.channels.at(ChannelIndex(1, 1, 0U)) =
        static_cast<std::uint8_t>(20 + layerIndex);
    layer.channels.at(ChannelIndex(1, 1, 1U)) = 40U;
    layer.channels.at(ChannelIndex(1, 1, 2U)) = 60U;
    layer.channels.at(ChannelIndex(2, 1, 3U)) = 0U;
    layer.channels.at(ChannelIndex(3, 1, 4U)) = 0U;
    layer.channels.at(ChannelIndex(4, 1, 5U)) = 0U;
    return layer;
}

slicer_core::RgbwsvProductionPackageWriteRequest MakeRequest(
    const std::filesystem::path& packageDir,
    const std::string& storageMode = "stripped")
{
    slicer_core::RgbwsvProductionPackageWriteRequest request;
    request.packageDir = packageDir;
    request.sourceConfigPath = "fixture/global.json";
    request.sourceModelPath = "fixture/model.obj";
    request.sourceFormat = "obj";
    request.requestedPipelineMode = "global_surface_shell";
    request.effectivePipelineMode = "global_surface_shell";
    request.productionAcceptance = "admitted";
    request.grid.widthPx = kWidth;
    request.grid.heightPx = kHeight;
    request.grid.layerCount = kLayerCount;
    request.grid.dpiX = 600;
    request.grid.dpiY = 600;
    request.grid.pixelSizeXmm = 25.4 / 600.0;
    request.grid.pixelSizeYmm = 25.4 / 600.0;
    request.grid.layerThicknessMm = 0.01;
    request.storage.storageMode = storageMode;
    request.storage.rowsPerStrip = 2;
    request.storage.tileWidth = 16;
    request.storage.tileHeight = 16;
    request.preview.outputpolicy = "tiff_native_with_diagnostics";
    request.preview.enabled = true;
    request.preview.format = "ppm";
    request.preview.interval = 1;
    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        request.layers.push_back(MakeLayer(layerIndex));
    }
    return request;
}

bool AdmittedGlobalPackagePassesRipAndReports()
{
    const std::filesystem::path directory = MakeTestDirectory("global");
    const std::filesystem::path packageDir = directory / "package";
    const auto result = slicer_core::WriteRgbwsvProductionPackage(
        MakeRequest(packageDir));
    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(packageDir);
    const slicer_core::Json manifest = ReadJson(packageDir / "manifest.json");
    const slicer_core::Json sliceReport =
        ReadJson(packageDir / "reports" / "slice_report.json");
    const slicer_core::Json previewReport =
        ReadJson(packageDir / "reports" / "preview_report.json");

    return ExpectTrue(result.productionOutputWritten, "writer reports production output")
        && ExpectTrue(!result.fallbackApplied, "writer applies no fallback")
        && ExpectTrue(result.layerCount == kLayerCount, "writer reports all layers")
        && ExpectTrue(
            !result.jobId.empty() && !result.attemptId.empty(),
            "writer reports the effective publication identity")
        && ExpectTrue(
            result.stagingRemoved
                && result.backupRemoved
                && result.leaseReleased,
            "writer reports no owned publication residue")
        && ExpectTrue(rip.schema == "p0.rgbwsv.2", "RIP accepts current schema")
        && ExpectTrue(rip.bit_depth == 8, "RIP accepts uint8")
        && ExpectTrue(
            rip.compression == "none",
            "RIP reports the default uncompressed payload")
        && ExpectTrue(rip.layer_count == kLayerCount, "RIP accepts complete layer list")
        && ExpectTrue(
            manifest.at("requestedPipelineMode").as_string()
                == "global_surface_shell",
            "manifest records requested mode")
        && ExpectTrue(
            manifest.at("effectivePipelineMode").as_string()
                == "global_surface_shell",
            "manifest records effective mode")
        && ExpectTrue(
            manifest.at("productionAcceptance").as_string() == "admitted",
            "manifest records admitted production")
        && ExpectTrue(
            manifest.at("productionOutputWritten").as_bool(),
            "manifest records TIFF output")
        && ExpectTrue(
            !manifest.at("fallbackApplied").as_bool(),
            "manifest records no fallback")
        && ExpectTrue(
            manifest.at("layers").size() == kLayerCount,
            "manifest root layer list is complete")
        && ExpectTrue(
            manifest.at("tiff").at("layers").size() == kLayerCount,
            "manifest TIFF layer list is complete")
        && ExpectTrue(
            manifest.at("tiff").at("compression").as_string() == "none",
            "manifest explicitly records the default compression")
        && ExpectTrue(
            sliceReport.at("productionTiffLayerCount").as_int() == kLayerCount,
            "slice report records production TIFF count")
        && ExpectTrue(
            sliceReport.at("productionOutputWritten").as_bool(),
            "slice report records production output")
        && ExpectTrue(
            previewReport.at("generated").size() == 8U,
            "preview report contains four views per layer")
        && ExpectTrue(
            previewReport.at("outputPolicy").as_string()
                == "tiff_native_with_diagnostics",
            "preview report records explicit diagnostic policy")
        && ExpectTrue(
            previewReport.at("automaticDiagnosticImages").as_bool(),
            "preview report records automatic diagnostic images")
        && ExpectTrue(
            std::filesystem::exists(packageDir / "preview" / "rgb_000000.ppm"),
            "RGB preview exists")
        && ExpectTrue(
            std::filesystem::exists(packageDir / "preview" / "support_s_000001.ppm"),
            "support preview exists");
}

bool PackBitsPackageUsesTheDeclaredCompression()
{
    const std::filesystem::path directory = MakeTestDirectory("packbits");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.storage.compression = "packbits";

    const auto result = slicer_core::WriteRgbwsvProductionPackage(request);
    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(packageDir);
    const slicer_core::Json manifest = ReadJson(packageDir / "manifest.json");
    const slicer_core::TiffReadResult layer = slicer_core::read_rgbwsv_tiff(
        packageDir / "layers" / "layer_000000.tiff");

    return ExpectTrue(result.productionOutputWritten, "PackBits package is written")
        && ExpectTrue(
            manifest.at("tiff").at("compression").as_string() == "packbits",
            "manifest declares PackBits")
        && ExpectTrue(rip.compression == "packbits", "RIP reports PackBits")
        && ExpectTrue(
            layer.spec.compression_mode == slicer_core::TiffCompressionMode::PackBits,
            "TIFF Compression tag is PackBits")
        && ExpectTrue(
            layer.pixels == MakeLayer(0).channels,
            "PackBits package preserves exact RGBWSV bytes");
}

bool CompressionManifestFailuresAreStable()
{
    const std::filesystem::path mismatchDirectory =
        MakeTestDirectory("compression_mismatch");
    const std::filesystem::path mismatchPackage =
        mismatchDirectory / "package";
    auto packBitsRequest = MakeRequest(mismatchPackage);
    packBitsRequest.storage.compression = "packbits";
    (void)slicer_core::WriteRgbwsvProductionPackage(packBitsRequest);
    MutateManifestCompression(mismatchPackage, "none");

    bool mismatchRejected{false};
    try
    {
        (void)slicer_core::validate_slice_package(mismatchPackage);
    }
    catch (const slicer_core::ValidationError& error)
    {
        mismatchRejected = ExpectTrue(
            error.code()
                == slicer_core::ValidationErrorCode::TiffCompressionMismatch,
            "manifest/TIFF compression mismatch has a stable error code");
    }

    const std::filesystem::path invalidDirectory =
        MakeTestDirectory("compression_invalid");
    const std::filesystem::path invalidPackage = invalidDirectory / "package";
    (void)slicer_core::WriteRgbwsvProductionPackage(MakeRequest(invalidPackage));
    MutateManifestCompression(invalidPackage, "deflate");

    bool invalidRejected{false};
    try
    {
        (void)slicer_core::validate_slice_package(invalidPackage);
    }
    catch (const slicer_core::ValidationError& error)
    {
        invalidRejected = ExpectTrue(
            error.code()
                == slicer_core::ValidationErrorCode::TiffCompressionInvalid,
            "unsupported manifest compression has a stable error code");
    }
    return mismatchRejected && invalidRejected;
}

bool HistoricalManifestWithoutCompressionDefaultsToNone()
{
    const std::filesystem::path directory =
        MakeTestDirectory("compression_omitted");
    const std::filesystem::path packageDir = directory / "package";
    (void)slicer_core::WriteRgbwsvProductionPackage(MakeRequest(packageDir));
    RemoveManifestCompression(packageDir);

    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(packageDir);
    return ExpectTrue(
        rip.compression == "none",
        "historical p0.rgbwsv.2 manifest without compression remains readable");
}

bool WhiteSemanticsUsesManifestAuthority()
{
    const std::filesystem::path directory =
        MakeTestDirectory("white_semantics");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.profileWhiteSemantics = "opaque";
    request.manifestWhiteSemantics = "opaque";
    (void)slicer_core::WriteRgbwsvProductionPackage(request);

    const slicer_core::Json manifest =
        ReadJson(packageDir / "manifest.json");
    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(packageDir);
    return ExpectTrue(
               manifest.at("whiteSemantics").as_string() == "opaque",
               "manifest records the authoritative white semantics")
        && ExpectTrue(
               rip.white_semantics.has_value()
                   && *rip.white_semantics == "opaque",
               "strict RIP reports manifest white semantics");
}

bool ProfileWhiteSemanticsProvidesTheDefault()
{
    const std::filesystem::path directory =
        MakeTestDirectory("white_semantics_default");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.profileWhiteSemantics = "transparent";
    (void)slicer_core::WriteRgbwsvProductionPackage(request);

    const slicer_core::Json manifest =
        ReadJson(packageDir / "manifest.json");
    return ExpectTrue(
        manifest.at("whiteSemantics").as_string() == "transparent",
        "Profile white semantics seeds the manifest default");
}

bool WhiteSemanticsMismatchFailsClosed()
{
    const std::filesystem::path directory =
        MakeTestDirectory("white_semantics_mismatch");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.profileWhiteSemantics = "opaque";
    request.manifestWhiteSemantics = "transparent";
    try
    {
        (void)slicer_core::WriteRgbwsvProductionPackage(request);
    }
    catch (const std::exception& error)
    {
        return ExpectTrue(
                   std::string{error.what()}.find("conflicts")
                       != std::string::npos,
                   "white semantics mismatch reports a stable conflict")
            && ExpectTrue(
                   !std::filesystem::exists(packageDir),
                   "white semantics mismatch publishes no package");
    }
    return ExpectTrue(false, "white semantics mismatch must fail closed");
}

bool HistoricalManifestWithoutWhiteSemanticsRemainsReadable()
{
    const std::filesystem::path directory =
        MakeTestDirectory("white_semantics_omitted");
    const std::filesystem::path packageDir = directory / "package";
    (void)slicer_core::WriteRgbwsvProductionPackage(MakeRequest(packageDir));
    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(packageDir);
    return ExpectTrue(
        !rip.white_semantics.has_value(),
        "historical manifest without whiteSemantics remains readable");
}

bool InvalidManifestWhiteSemanticsFailsClosed()
{
    const std::filesystem::path directory =
        MakeTestDirectory("white_semantics_invalid");
    const std::filesystem::path packageDir = directory / "package";
    (void)slicer_core::WriteRgbwsvProductionPackage(MakeRequest(packageDir));
    MutateManifestWhiteSemantics(packageDir, slicer_core::Json{"mixed"});
    try
    {
        (void)slicer_core::validate_slice_package(packageDir);
    }
    catch (const slicer_core::ValidationError& error)
    {
        return ExpectTrue(
            error.code()
                == slicer_core::ValidationErrorCode::WhiteSemanticsInvalid,
            "invalid manifest whiteSemantics has a stable error code");
    }
    return ExpectTrue(false, "invalid manifest whiteSemantics must fail closed");
}

bool InvalidCompressionWritesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("invalid_compression_request");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.storage.compression = "deflate";
    try
    {
        (void)slicer_core::WriteRgbwsvProductionPackage(request);
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "unsupported compression publishes no package");
    }
    return ExpectTrue(false, "unsupported compression must fail closed");
}

bool TiledPackageUsesTheSameProtocol()
{
    const std::filesystem::path directory = MakeTestDirectory("tiled");
    const std::filesystem::path packageDir = directory / "package";
    const auto result = slicer_core::WriteRgbwsvProductionPackage(
        MakeRequest(packageDir, "tiled"));
    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(packageDir);

    return ExpectTrue(result.productionOutputWritten, "tiled package is written")
        && ExpectTrue(rip.storage_mode == "tiled", "RIP accepts tiled storage")
        && ExpectTrue(rip.layer_count == kLayerCount, "tiled layer list is complete");
}

bool LegacyPackageUsesTheSameWriterContract()
{
    const std::filesystem::path directory = MakeTestDirectory("legacy");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.requestedPipelineMode = "legacy";
    request.effectivePipelineMode = "legacy";

    const auto result =
        slicer_core::WriteRgbwsvProductionPackage(request);
    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(packageDir);
    const slicer_core::Json manifest = ReadJson(packageDir / "manifest.json");

    return ExpectTrue(
               result.productionOutputWritten,
               "legacy package uses the shared writer")
        && ExpectTrue(
            rip.schema == "p0.rgbwsv.2",
            "legacy package keeps the fixed protocol")
        && ExpectTrue(
            manifest.at("effectivePipelineMode").as_string() == "legacy",
            "legacy package records its effective mode");
}

bool AdmittedGlobalAdapterWritesThroughTheSharedWriter()
{
    const std::filesystem::path directory =
        MakeTestDirectory("global_adapter");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.layers.clear();

    slicer_core::GlobalSurfaceShellProductionLayerAdapterResult adapter;
    adapter.available = true;
    adapter.fullClosurePass = true;
    adapter.status = "ready_for_writer";
    adapter.widthPx = kWidth;
    adapter.heightPx = kHeight;
    adapter.layerCount = kLayerCount;
    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        slicer_core::GlobalSurfaceShellProductionLayer layer;
        layer.output = MakeLayer(layerIndex);
        adapter.layers.push_back(std::move(layer));
    }

    const auto result =
        slicer_core::WriteGlobalSurfaceShellProductionPackage(
            request,
            adapter);
    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(packageDir);

    return ExpectTrue(
               result.productionOutputWritten,
               "admitted adapter writes through the shared writer")
        && ExpectTrue(
            rip.layer_count == kLayerCount,
            "adapter package contains every layer");
}

bool BlockedGlobalAdapterWritesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("blocked_adapter");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.layers.clear();

    slicer_core::GlobalSurfaceShellProductionLayerAdapterResult adapter;
    adapter.status = "blocked";
    try
    {
        (void)slicer_core::WriteGlobalSurfaceShellProductionPackage(
            request,
            adapter);
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "blocked adapter publishes no package");
    }
    return ExpectTrue(false, "blocked adapter must fail closed");
}

bool ProtocolMismatchAdapterWritesNothing()
{
    const std::filesystem::path directory =
        MakeTestDirectory("protocol_mismatch_adapter");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.layers.clear();

    slicer_core::GlobalSurfaceShellProductionLayerAdapterResult adapter;
    adapter.available = true;
    adapter.fullClosurePass = true;
    adapter.status = "ready_for_writer";
    adapter.widthPx = kWidth;
    adapter.heightPx = kHeight;
    adapter.layerCount = kLayerCount;
    adapter.protocol.bit_depth = 16;
    for (int layerIndex{0}; layerIndex < kLayerCount; ++layerIndex)
    {
        slicer_core::GlobalSurfaceShellProductionLayer layer;
        layer.output = MakeLayer(layerIndex);
        adapter.layers.push_back(std::move(layer));
    }

    try
    {
        (void)slicer_core::WriteGlobalSurfaceShellProductionPackage(
            request,
            adapter);
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "protocol mismatch adapter publishes no package");
    }
    return ExpectTrue(false, "protocol mismatch adapter must fail closed");
}

bool PngPreviewUsesConfiguredFormat()
{
    const std::filesystem::path directory = MakeTestDirectory("png");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.preview.format = "png";
    const auto result = slicer_core::WriteRgbwsvProductionPackage(request);
    const slicer_core::Json previewReport =
        ReadJson(packageDir / "reports" / "preview_report.json");

    std::ifstream image{
        packageDir / "preview" / "rgb_000000.png",
        std::ios::binary};
    std::vector<std::uint8_t> signature(8U, 0U);
    image.read(
        reinterpret_cast<char*>(signature.data()),
        static_cast<std::streamsize>(signature.size()));
    const std::vector<std::uint8_t> expected{
        137U, 80U, 78U, 71U, 13U, 10U, 26U, 10U};

    return ExpectTrue(result.productionOutputWritten, "PNG package is written")
        && ExpectTrue(
            previewReport.at("format").as_string() == "png",
            "preview report records PNG")
        && ExpectTrue(signature == expected, "PNG signature is valid");
}

bool TiffNativeWritesNoDiagnosticImages()
{
    const std::filesystem::path directory = MakeTestDirectory("tiff_native");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.preview.outputpolicy = "tiff_native";
    request.preview.enabled = false;

    const auto result =
        slicer_core::WriteRgbwsvProductionPackage(request);
    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(packageDir);
    const slicer_core::Json manifest =
        ReadJson(packageDir / "manifest.json");
    const slicer_core::Json previewReport =
        ReadJson(packageDir / "reports" / "preview_report.json");

    return ExpectTrue(result.productionOutputWritten, "TIFF-native package is written")
        && ExpectTrue(rip.layer_count == kLayerCount, "TIFF-native package passes RIP")
        && ExpectTrue(
            !std::filesystem::exists(packageDir / "preview"),
            "TIFF-native package creates no preview directory")
        && ExpectTrue(
            previewReport.at("outputPolicy").as_string() == "tiff_native",
            "preview report records TIFF-native output policy")
        && ExpectTrue(
            previewReport.at("productionSource").as_string() == "rgbwsv_tiff",
            "preview report records production TIFF source")
        && ExpectTrue(
            !previewReport.at("automaticDiagnosticImages").as_bool(),
            "preview report disables automatic diagnostic images")
        && ExpectTrue(
            previewReport.at("generated").size() == 0U,
            "preview report contains no generated diagnostic images")
        && ExpectTrue(
            manifest.at("preview").at("outputPolicy").as_string()
                == "tiff_native",
            "manifest records TIFF-native output policy")
        && ExpectTrue(
            manifest.at("preview").at("files").size() == 0U,
            "manifest contains no preview image files");
}

bool NonAdmittedRequestWritesNothing()
{
    const std::filesystem::path directory = MakeTestDirectory("not_admitted");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.productionAcceptance = "not_evaluated";
    try
    {
        (void)slicer_core::WriteRgbwsvProductionPackage(request);
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "non-admitted request publishes no package");
    }
    return ExpectTrue(false, "non-admitted request must fail closed");
}

bool ModeMismatchWritesNothing()
{
    const std::filesystem::path directory = MakeTestDirectory("mode_mismatch");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.effectivePipelineMode = "legacy";
    try
    {
        (void)slicer_core::WriteRgbwsvProductionPackage(request);
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "mode mismatch publishes no fallback package");
    }
    return ExpectTrue(false, "mode mismatch must fail closed");
}

bool BrokenLayerSequenceWritesNothing()
{
    const std::filesystem::path directory = MakeTestDirectory("broken_layers");
    const std::filesystem::path packageDir = directory / "package";
    auto request = MakeRequest(packageDir);
    request.layers.at(1).layerIndex = 3;
    try
    {
        (void)slicer_core::WriteRgbwsvProductionPackage(request);
    }
    catch (const std::exception&)
    {
        return ExpectTrue(
            !std::filesystem::exists(packageDir),
            "invalid layer sequence publishes no package");
    }
    return ExpectTrue(false, "invalid layer sequence must fail closed");
}

bool ExistingPackageIsAtomicallyReplaced()
{
    const std::filesystem::path directory = MakeTestDirectory("replace");
    const std::filesystem::path packageDir = directory / "package";
    std::filesystem::create_directories(packageDir);
    {
        std::ofstream marker{packageDir / "stale.txt"};
        marker << "stale";
    }

    const auto result = slicer_core::WriteRgbwsvProductionPackage(
        MakeRequest(packageDir));
    const slicer_core::RipValidationResult rip =
        slicer_core::validate_slice_package(packageDir);
    const auto identity =
        slicer_core::api::artifacts::MakePackageArtifactIdentity(
            std::filesystem::absolute(packageDir).lexically_normal(),
            result.jobId,
            result.attemptId);

    return ExpectTrue(result.productionOutputWritten, "replacement package is written")
        && ExpectTrue(
            result.strictProtocolValidated,
            "replacement package reuses strict staging validation evidence")
        && ExpectTrue(
            result.stagingRemoved
                && result.backupRemoved
                && result.leaseReleased,
            "replacement publication removes its owned transaction artifacts")
        && ExpectTrue(
            !std::filesystem::exists(identity.staging_directory)
                && !std::filesystem::exists(identity.backup_directory)
                && !std::filesystem::exists(identity.lease_directory),
            "replacement publication leaves no owned filesystem residue")
        && ExpectTrue(
            !std::filesystem::exists(packageDir / "stale.txt"),
            "stale package content is removed")
        && ExpectTrue(rip.layer_count == kLayerCount, "replacement package passes RIP");
}

bool ConcurrentPackageTargetIsRejectedBeforeWriting()
{
    const std::filesystem::path directory = MakeTestDirectory("lease_conflict");
    const std::filesystem::path packageDir =
        std::filesystem::absolute(directory / "package").lexically_normal();
    const auto owner =
        slicer_core::api::artifacts::MakePackageArtifactIdentity(
            packageDir,
            "job-owner",
            "attempt-owner");
    const auto lease =
        slicer_core::api::artifacts::AcquirePackageArtifactLease(owner);
    auto request = MakeRequest(packageDir);
    request.jobId = "job-second";
    request.attemptId = "attempt-second";
    bool rejected{false};
    try
    {
        (void)slicer_core::WriteRgbwsvProductionPackage(request);
    }
    catch (const slicer_core::api::artifacts::PackageArtifactLeaseConflict&)
    {
        rejected = true;
    }
    const auto released =
        slicer_core::api::artifacts::ReleasePackageArtifactLease(owner);
    return ExpectTrue(lease.success, "first writer owns the target lease")
        && ExpectTrue(rejected, "second writer fails with a target lease conflict")
        && ExpectTrue(
            !std::filesystem::exists(
                packageDir.parent_path()
                / "package.staging.job-second.attempt-second"),
            "conflicting writer creates no staging package")
        && ExpectTrue(released.success, "owner releases the target lease");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"admitted_global_package_passes_rip_and_reports", AdmittedGlobalPackagePassesRipAndReports},
        {"tiled_package_uses_the_same_protocol", TiledPackageUsesTheSameProtocol},
        {"packbits_package_uses_the_declared_compression", PackBitsPackageUsesTheDeclaredCompression},
        {"compression_manifest_failures_are_stable", CompressionManifestFailuresAreStable},
        {"historical_manifest_without_compression_defaults_to_none", HistoricalManifestWithoutCompressionDefaultsToNone},
        {"white_semantics_uses_manifest_authority", WhiteSemanticsUsesManifestAuthority},
        {"profile_white_semantics_provides_the_default", ProfileWhiteSemanticsProvidesTheDefault},
        {"white_semantics_mismatch_fails_closed", WhiteSemanticsMismatchFailsClosed},
        {"historical_manifest_without_white_semantics_remains_readable", HistoricalManifestWithoutWhiteSemanticsRemainsReadable},
        {"invalid_manifest_white_semantics_fails_closed", InvalidManifestWhiteSemanticsFailsClosed},
        {"invalid_compression_writes_nothing", InvalidCompressionWritesNothing},
        {"legacy_package_uses_the_same_writer_contract", LegacyPackageUsesTheSameWriterContract},
        {"admitted_global_adapter_writes_through_the_shared_writer", AdmittedGlobalAdapterWritesThroughTheSharedWriter},
        {"blocked_global_adapter_writes_nothing", BlockedGlobalAdapterWritesNothing},
        {"protocol_mismatch_adapter_writes_nothing", ProtocolMismatchAdapterWritesNothing},
        {"png_preview_uses_configured_format", PngPreviewUsesConfiguredFormat},
        {"tiff_native_writes_no_diagnostic_images", TiffNativeWritesNoDiagnosticImages},
        {"non_admitted_request_writes_nothing", NonAdmittedRequestWritesNothing},
        {"mode_mismatch_writes_nothing", ModeMismatchWritesNothing},
        {"broken_layer_sequence_writes_nothing", BrokenLayerSequenceWritesNothing},
        {"existing_package_is_atomically_replaced", ExistingPackageIsAtomicallyReplaced},
        {"concurrent_package_target_is_rejected_before_writing", ConcurrentPackageTargetIsRejectedBeforeWriting},
    };

    bool passed{true};
    for (const auto& test : tests)
    {
        const bool current = test.second();
        std::cout << (current ? "PASS: " : "FAIL: ") << test.first << '\n';
        passed = current && passed;
    }
    return passed ? 0 : 1;
}
