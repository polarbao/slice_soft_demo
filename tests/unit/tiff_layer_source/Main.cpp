#include "slicer_core/json_value.h"
#include "slicer_core/preview/TiffLayerSource.h"
#include "slicer_core/tiff_io.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr int kWidth{4};
constexpr int kHeight{3};
constexpr std::size_t kChannelCount{6U};

struct FixtureOptions
{
    std::string schema{"p0.rgbwsv.2"};
    std::string storage{"stripped"};
    std::string polarity{"black_is_print"};
    int bitDepth{8};
    int widthPx{kWidth};
    int dpiX{635};
    int dpiY{600};
    bool missingMiddleLayer{false};
    bool escapingMiddleLayer{false};
};

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
        / ("slicesoft_tiff_layer_source_" + name + "_"
           + std::to_string(suffix));
    std::filesystem::create_directories(directory);
    return directory;
}

std::vector<std::uint8_t> MakePixels(const int layerIndex)
{
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(kWidth * kHeight) * kChannelCount,
        255U);
    pixels.at(0U) = static_cast<std::uint8_t>(layerIndex);
    pixels.at(3U) = 0U;
    pixels.at(4U) = 0U;
    pixels.at(5U) = 0U;
    return pixels;
}

void WriteJson(
    const std::filesystem::path& path,
    const slicer_core::Json& document)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        throw std::runtime_error("failed to write fixture JSON");
    }
    output << document.dump(2) << '\n';
}

slicer_core::Json MakeManifest(
    const FixtureOptions& options,
    const std::vector<int>& layerIndices)
{
    slicer_core::Json::Array layers;
    for (const int layerIndex : layerIndices)
    {
        std::string path =
            "layers/layer_" + std::to_string(layerIndex) + ".tiff";
        if (options.escapingMiddleLayer && layerIndex == 20)
        {
            path = "../escaped.tiff";
        }
        layers.push_back(slicer_core::Json::object({
            {"index", layerIndex},
            {"zMm", static_cast<double>(layerIndex) * 0.01},
            {"path", path},
            {"widthPx", options.widthPx},
            {"heightPx", kHeight},
        }));
    }

    slicer_core::Json::Object tiff{
        {"channelOrder", slicer_core::Json::array({"R", "G", "B", "W", "S", "V"})},
        {"channelCount", 6},
        {"bitDepth", options.bitDepth},
        {"sampleFormat", "uint"},
        {"planarConfig", "contiguous"},
        {"storageMode", options.storage},
        {"storage", options.storage},
        {"tiled", options.storage == "tiled"},
        {"polarity", options.polarity},
        {"printValue", 0},
        {"emptyValue", 255},
        {"layers", slicer_core::Json{layers}},
    };
    if (options.storage == "tiled")
    {
        tiff["tileSize"] = slicer_core::Json::array({16, 16});
    }
    else
    {
        tiff["rowsPerStrip"] = 2;
    }

    return slicer_core::Json::object({
        {"schema", options.schema},
        {"grid",
         slicer_core::Json::object({
             {"widthPx", options.widthPx},
             {"heightPx", kHeight},
             {"layerCount", static_cast<int>(layerIndices.size())},
             {"dpiX", options.dpiX},
             {"dpiY", options.dpiY},
             {"pixelSizeXmm", 25.4 / static_cast<double>(options.dpiX)},
             {"pixelSizeYmm", 25.4 / static_cast<double>(options.dpiY)},
             {"layerThicknessMm", 0.01},
         })},
        {"tiff", slicer_core::Json{std::move(tiff)}},
        {"layers", slicer_core::Json{layers}},
    });
}

std::filesystem::path MakePackage(
    const std::string& name,
    const FixtureOptions& options = {})
{
    const std::filesystem::path packageDir =
        MakeTestDirectory(name) / "package";
    std::filesystem::create_directories(packageDir / "layers");
    const std::vector<int> layerIndices{10, 20, 40};

    for (const int layerIndex : layerIndices)
    {
        if (options.missingMiddleLayer && layerIndex == 20)
        {
            continue;
        }
        slicer_core::TiffImageSpec spec;
        spec.width = kWidth;
        spec.height = kHeight;
        spec.rows_per_strip = 2;
        spec.tile_width = 16;
        spec.tile_height = 16;
        spec.storage_mode = options.storage == "tiled"
            ? slicer_core::TiffStorageMode::Tiled
            : slicer_core::TiffStorageMode::Stripped;
        slicer_core::write_rgbwsv_tiff(
            packageDir / "layers"
                / ("layer_" + std::to_string(layerIndex) + ".tiff"),
            spec,
            MakePixels(layerIndex));
    }

    WriteJson(
        packageDir / "manifest.json",
        MakeManifest(options, layerIndices));
    return packageDir;
}

bool ErrorCodeMatches(
    const std::function<void()>& action,
    const slicer_core::TiffLayerErrorCode expected)
{
    try
    {
        action();
    }
    catch (const slicer_core::TiffLayerError& error)
    {
        return ExpectTrue(error.Code() == expected, "stable error code matches");
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: unexpected exception: " << error.what() << '\n';
        return false;
    }
    std::cerr << "FAILED: expected TiffLayerError\n";
    return false;
}

bool StrippedPackageIndexesAndLoadsByRealLayerIndex()
{
    const std::filesystem::path packageDir = MakePackage("stripped");
    slicer_core::TiffLayerSource source;
    const slicer_core::ProductionPackageIndex package =
        source.IndexPackage(packageDir / "manifest.json");
    const auto layer = source.FindLayer(20);
    if (!ExpectTrue(layer.has_value(), "real non-contiguous layer index is found"))
    {
        return false;
    }

    const slicer_core::TiffLayerLoadResult first =
        source.LoadLayer(*layer);
    const slicer_core::TiffLayerLoadResult second =
        source.LoadLayer(*layer);

    return ExpectTrue(package.layers.size() == 3U, "all manifest layers are indexed")
        && ExpectTrue(package.dpiX == 635 && package.dpiY == 600, "independent DPI is preserved")
        && ExpectTrue(layer->layerIndex == 20, "manifest layer index is authoritative")
        && ExpectTrue(layer->zMm == 0.2, "manifest zMm is preserved")
        && ExpectTrue(first.buffer != nullptr, "layer buffer is decoded")
        && ExpectTrue(first.buffer->width == kWidth, "decoded width matches")
        && ExpectTrue(first.buffer->height == kHeight, "decoded height matches")
        && ExpectTrue(first.buffer->pixels == MakePixels(20), "all RGBWSV bytes are preserved")
        && ExpectTrue(!first.cacheHit, "first load decodes TIFF")
        && ExpectTrue(second.cacheHit, "second load uses cache")
        && ExpectTrue(first.buffer == second.buffer, "cache returns the same immutable buffer")
        && ExpectTrue(source.FindLayer(21) == std::nullopt, "missing layer is not cross-layer substituted");
}

bool TiledPackageLoadsThroughTheSameSource()
{
    FixtureOptions options;
    options.storage = "tiled";
    const std::filesystem::path packageDir =
        MakePackage("tiled", options);
    slicer_core::TiffLayerSource source;
    const slicer_core::ProductionPackageIndex package =
        source.IndexPackage(packageDir / "manifest.json");
    const auto layer = source.FindLayer(40);

    return ExpectTrue(package.storage == slicer_core::TiffStorageMode::Tiled, "tiled storage is indexed")
        && ExpectTrue(layer.has_value(), "tiled layer is found")
        && ExpectTrue(source.LoadLayer(*layer).buffer->pixels == MakePixels(40), "tiled layer is decoded");
}

bool ProtocolAndPathFailuresAreStable()
{
    FixtureOptions badSchema;
    badSchema.schema = "p0.rgbwsv.1";
    const std::filesystem::path schemaPackage =
        MakePackage("bad_schema", badSchema);

    FixtureOptions badBitDepth;
    badBitDepth.bitDepth = 16;
    const std::filesystem::path bitDepthPackage =
        MakePackage("bad_bit_depth", badBitDepth);

    FixtureOptions badPolarity;
    badPolarity.polarity = "white_is_print";
    const std::filesystem::path polarityPackage =
        MakePackage("bad_polarity", badPolarity);

    FixtureOptions pathEscape;
    pathEscape.escapingMiddleLayer = true;
    const std::filesystem::path escapePackage =
        MakePackage("path_escape", pathEscape);

    FixtureOptions missingLayer;
    missingLayer.missingMiddleLayer = true;
    const std::filesystem::path missingPackage =
        MakePackage("missing_layer", missingLayer);

    FixtureOptions wrongDimension;
    wrongDimension.widthPx = kWidth + 1;
    const std::filesystem::path dimensionPackage =
        MakePackage("wrong_dimension", wrongDimension);

    return ErrorCodeMatches(
               [&]()
               {
                   slicer_core::TiffLayerSource source;
                   static_cast<void>(source.IndexPackage(
                       schemaPackage / "manifest.json"));
               },
               slicer_core::TiffLayerErrorCode::ProtocolMismatch)
        && ErrorCodeMatches(
            [&]()
            {
                slicer_core::TiffLayerSource source;
                static_cast<void>(source.IndexPackage(
                    bitDepthPackage / "manifest.json"));
            },
            slicer_core::TiffLayerErrorCode::ProtocolMismatch)
        && ErrorCodeMatches(
            [&]()
            {
                slicer_core::TiffLayerSource source;
                static_cast<void>(source.IndexPackage(
                    polarityPackage / "manifest.json"));
            },
            slicer_core::TiffLayerErrorCode::ProtocolMismatch)
        && ErrorCodeMatches(
            [&]()
            {
                slicer_core::TiffLayerSource source;
                static_cast<void>(source.IndexPackage(
                    escapePackage / "manifest.json"));
            },
            slicer_core::TiffLayerErrorCode::PathEscape)
        && ErrorCodeMatches(
            [&]()
            {
                slicer_core::TiffLayerSource source;
                static_cast<void>(source.IndexPackage(
                    missingPackage / "manifest.json"));
            },
            slicer_core::TiffLayerErrorCode::FileMissing)
        && ErrorCodeMatches(
            [&]()
            {
                slicer_core::TiffLayerSource source;
                const auto package = source.IndexPackage(
                    dimensionPackage / "manifest.json");
                static_cast<void>(source.LoadLayer(package.layers.front()));
            },
            slicer_core::TiffLayerErrorCode::DimensionMismatch);
}

bool CancellationAndStaleGenerationDoNotPopulateCache()
{
    const std::filesystem::path packageDir = MakePackage("cancel_stale");
    slicer_core::TiffLayerSource source;
    const auto package = source.IndexPackage(packageDir / "manifest.json");

    slicer_core::TiffLayerLoadControl cancelled;
    cancelled.cancellationRequested = []()
    {
        return true;
    };
    const bool cancelPass = ErrorCodeMatches(
        [&]()
        {
            static_cast<void>(source.LoadLayer(
                package.layers.at(0U),
                cancelled));
        },
        slicer_core::TiffLayerErrorCode::Cancelled);

    slicer_core::TiffLayerLoadControl stale;
    stale.requestGeneration = 7U;
    stale.generationCurrent = [](const std::uint64_t)
    {
        return false;
    };
    const bool stalePass = ErrorCodeMatches(
        [&]()
        {
            static_cast<void>(source.LoadLayer(
                package.layers.at(1U),
                stale));
        },
        slicer_core::TiffLayerErrorCode::StaleResult);

    return cancelPass
        && stalePass
        && ExpectTrue(source.CacheStats().layerCount == 0U, "cancelled and stale loads do not populate cache");
}

bool ManifestChangeInvalidatesIndexedReferences()
{
    const std::filesystem::path packageDir = MakePackage("manifest_change");
    slicer_core::TiffLayerSource source;
    const auto package = source.IndexPackage(packageDir / "manifest.json");
    std::ofstream output{
        packageDir / "manifest.json",
        std::ios::binary | std::ios::app};
    output << '\n';
    output.close();

    return ErrorCodeMatches(
        [&]()
        {
            static_cast<void>(source.LoadLayer(package.layers.front()));
        },
        slicer_core::TiffLayerErrorCode::StaleResult);
}

bool PackageSwitchClearsPreviousCache()
{
    const std::filesystem::path firstPackage =
        MakePackage("package_switch_first");
    const std::filesystem::path secondPackage =
        MakePackage("package_switch_second");
    slicer_core::TiffLayerSource source;
    const auto first =
        source.IndexPackage(firstPackage / "manifest.json");
    static_cast<void>(source.LoadLayer(first.layers.front()));
    const bool populated =
        source.CacheStats().layerCount == 1U;

    static_cast<void>(source.IndexPackage(
        secondPackage / "manifest.json"));

    return ExpectTrue(populated, "first package enters the cache")
        && ExpectTrue(source.CacheStats().layerCount == 0U, "package switch clears previous cache");
}

}  // namespace

int main()
{
    const bool ok = StrippedPackageIndexesAndLoadsByRealLayerIndex()
        && TiledPackageLoadsThroughTheSameSource()
        && ProtocolAndPathFailuresAreStable()
        && CancellationAndStaleGenerationDoNotPopulateCache()
        && ManifestChangeInvalidatesIndexedReferences()
        && PackageSwitchClearsPreviousCache();
    if (!ok)
    {
        return 1;
    }

    std::cout << "tiff_layer_source_unit_tests: PASS\n";
    return 0;
}
