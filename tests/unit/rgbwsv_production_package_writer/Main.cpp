#include "slicer_core/json_value.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"
#include "slicer_core/pipeline/GlobalSurfaceShellProductionPackage.h"
#include "slicer_core/rip_reader.h"

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
        && ExpectTrue(rip.schema == "p0.rgbwsv.2", "RIP accepts current schema")
        && ExpectTrue(rip.bit_depth == 8, "RIP accepts uint8")
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
            sliceReport.at("productionTiffLayerCount").as_int() == kLayerCount,
            "slice report records production TIFF count")
        && ExpectTrue(
            sliceReport.at("productionOutputWritten").as_bool(),
            "slice report records production output")
        && ExpectTrue(
            previewReport.at("generated").size() == 8U,
            "preview report contains four views per layer")
        && ExpectTrue(
            std::filesystem::exists(packageDir / "preview" / "rgb_000000.ppm"),
            "RGB preview exists")
        && ExpectTrue(
            std::filesystem::exists(packageDir / "preview" / "support_s_000001.ppm"),
            "support preview exists");
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

    return ExpectTrue(result.productionOutputWritten, "replacement package is written")
        && ExpectTrue(
            !std::filesystem::exists(packageDir / "stale.txt"),
            "stale package content is removed")
        && ExpectTrue(rip.layer_count == kLayerCount, "replacement package passes RIP");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"admitted_global_package_passes_rip_and_reports", AdmittedGlobalPackagePassesRipAndReports},
        {"tiled_package_uses_the_same_protocol", TiledPackageUsesTheSameProtocol},
        {"legacy_package_uses_the_same_writer_contract", LegacyPackageUsesTheSameWriterContract},
        {"admitted_global_adapter_writes_through_the_shared_writer", AdmittedGlobalAdapterWritesThroughTheSharedWriter},
        {"blocked_global_adapter_writes_nothing", BlockedGlobalAdapterWritesNothing},
        {"protocol_mismatch_adapter_writes_nothing", ProtocolMismatchAdapterWritesNothing},
        {"png_preview_uses_configured_format", PngPreviewUsesConfiguredFormat},
        {"non_admitted_request_writes_nothing", NonAdmittedRequestWritesNothing},
        {"mode_mismatch_writes_nothing", ModeMismatchWritesNothing},
        {"broken_layer_sequence_writes_nothing", BrokenLayerSequenceWritesNothing},
        {"existing_package_is_atomically_replaced", ExistingPackageIsAtomicallyReplaced},
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
