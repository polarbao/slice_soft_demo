#include "slicer_core/output/tiff/TiffWriterFactory.h"
#include "slicer_core/output/tiff/TiffRowManifest.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtTiffIo.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtLegacyPackageMetadata.h"
#include "slicer_core/tiff_io.h"
#include "slicer_core/preview/TiffLayerSource.h"

#include <tiffio.h>
#include <chrono>
#include <iostream>
#include <memory>
#include "RewritePackage.h"

using namespace slicer_core;
namespace
{
void Require(const bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

using Handle = std::unique_ptr<TIFF, decltype(&TIFFClose)>;
Handle Open(const std::filesystem::path& path, const char* mode = "r")
{
#ifdef _WIN32
    Handle file{TIFFOpenW(path.wstring().c_str(), mode), TIFFClose};
#else
    Handle file{TIFFOpen(path.string().c_str(), mode), TIFFClose};
#endif
    Require(file != nullptr, "independent TIFF open failed");
    return file;
}

// LibTIFF raw scanlines/tiles do not use our row-order parser or mapping helper.
std::vector<std::uint8_t> Raw(const std::filesystem::path& path, const TiffImageSpec& spec)
{
    auto file = Open(path);
    std::uint16_t orientation = 0;
    Require(TIFFGetFieldDefaulted(file.get(), TIFFTAG_ORIENTATION, &orientation) == 1
        && orientation == 1U, "raw TIFF must use Orientation=1");
    const std::size_t stride = static_cast<std::size_t>(spec.width) * spec.samples_per_pixel;
    std::vector<std::uint8_t> result(stride * spec.height);
    if (TIFFIsTiled(file.get()))
    {
        std::vector<std::uint8_t> tile(static_cast<std::size_t>(TIFFTileSize(file.get())));
        for (std::uint32_t y = 0; y < spec.height; y += spec.tile_height)
            for (std::uint32_t x = 0; x < spec.width; x += spec.tile_width)
            {
                Require(TIFFReadTile(file.get(), tile.data(), x, y, 0, 0) > 0, "raw tile decode failed");
                for (std::uint32_t row = 0; row < spec.tile_height && y + row < spec.height; ++row)
                    std::copy_n(tile.data() + row * spec.tile_width * spec.samples_per_pixel,
                        std::min(spec.tile_width, spec.width - x) * spec.samples_per_pixel,
                        result.data() + (y + row) * stride + x * spec.samples_per_pixel);
            }
    }
    else
        for (std::uint32_t row = 0; row < spec.height; ++row)
            Require(TIFFReadScanline(file.get(), result.data() + row * stride, row, 0) == 1,
                "raw scanline decode failed");
    return result;
}

template<class Fn> void Reject(Fn&& action)
{
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Require(rejected, "invalid metadata was accepted");
}

void CheckFile(const std::filesystem::path& path, const TiffImageSpec& spec,
    const std::vector<std::uint8_t>& pixels)
{
    const auto raw = Raw(path, spec);
    const std::size_t stride = static_cast<std::size_t>(spec.width) * spec.samples_per_pixel;
    for (std::uint32_t row = 0; row < spec.height; ++row)
    {
        const auto source = spec.row_order == TiffRowOrder::MaxYFirst ? spec.height - row - 1U : row;
        Require(std::equal(raw.begin() + row * stride, raw.begin() + (row + 1U) * stride,
            pixels.begin() + source * stride), "raw rows/X/channel order mismatch");
    }
    if (spec.samples_per_pixel == 6U)
    {
        const auto decoded = read_rgbwsv_tiff(path);
        const auto stats = read_rgbwsv_tiff_stats(path);
        Require(decoded.pixels == pixels && decoded.spec.row_order == spec.row_order,
            "six-channel canonical roundtrip mismatch");
        Require(stats.channel_checksums == decoded.channel_checksums
            && stats.spec.row_order == spec.row_order, "stats-only metadata/checksum mismatch");
        if (spec.storage_mode == TiffStorageMode::Stripped)
            Require(stats.pixels.empty(), "stats-only unexpectedly materialized pixels");
    }
    else
    {
        const auto decoded = ReadRgbwsvtTiff(path);
        Require(decoded.pixels == pixels && decoded.spec.row_order == spec.row_order,
            "seven-channel canonical roundtrip mismatch");
    }
}

void Matrix(const std::filesystem::path& directory)
{
    int cases = 0;
    for (const auto backend : {TiffWriterBackend::LibTiff, TiffWriterBackend::Handwritten})
    for (const auto channels : {6U, 7U})
    for (const auto storage : {TiffStorageMode::Stripped, TiffStorageMode::Tiled})
    for (const auto compression : {TiffCompressionMode::None, TiffCompressionMode::PackBits})
    for (const auto order : {TiffRowOrder::MinYFirst, TiffRowOrder::MaxYFirst})
    {
        if (channels == 7U && backend == TiffWriterBackend::Handwritten) continue;
        TiffImageSpec spec;
        spec.width = 19; spec.height = 21; spec.rows_per_strip = 8;
        spec.tile_width = 16; spec.tile_height = 16;
        spec.samples_per_pixel = static_cast<std::uint16_t>(channels);
        spec.storage_mode = storage; spec.compression_mode = compression; spec.row_order = order;
        std::vector<std::uint8_t> pixels(spec.width * spec.height * channels);
        for (std::size_t i = 0; i < pixels.size(); ++i)
            pixels[i] = static_cast<std::uint8_t>((i * 31 + i / 17) % 256);
        const auto path = directory / (std::to_string(cases++) + ".tif");
        CreateTiffWriter(backend)->Write(path, spec, pixels);
        CheckFile(path, spec, pixels);
    }
    Require(cases == 24, "matrix incomplete");
}

void ProductionAndRejection(const std::filesystem::path& directory)
{
    RgbwsvtProductionLayer seven;
    seven.widthPx = 5; seven.heightPx = 7;
    seven.channels.assign(5U * 7U * 7U, 255U);
    seven.channels[6U] = 0U;
    (void)WriteRgbwsvtLegacyProductionLayerTiff(directory / "seven.tif", {}, seven);
    TiffImageSpec sevenSpec;
    sevenSpec.width = 5; sevenSpec.height = 7; sevenSpec.samples_per_pixel = 7;
    sevenSpec.row_order = TiffRowOrder::MaxYFirst;
    CheckFile(directory / "seven.tif", sevenSpec, seven.channels);
    TiffImageSpec spec;
    spec.width = 5; spec.height = 7; spec.row_order = TiffRowOrder::MaxYFirst;
    // Asymmetric markers under four quarter-turns and explicit X/Y mirrors.
    for (int turn = 0; turn < 4; ++turn)
    for (int mirror = 0; mirror < 4; ++mirror)
    {
        std::vector<std::uint8_t> pixels(spec.width * spec.height * 6, 255);
        int x = 1, y = 0;
        for (int i = 0; i < turn; ++i) { const int nextX = -y; y = x; x = nextX; }
        if (mirror & 1) x = -x;
        if (mirror & 2) y = -y;
        pixels[(static_cast<std::size_t>(y + 3) * spec.width + x + 2) * 6 + 3] = 0;
        const auto path = directory / "production.tif";
        WriteRgbwsvProductionLayerTiff(path, {}, {5, 7, pixels});
        CheckFile(path, spec, pixels);
    }
    ValidateTiffRowOrder(Json::object({{"rowOrder", "max_y_first"}}), spec);
    Reject([&] { ValidateTiffRowOrder(Json::object({}), spec); });
    Reject([&] { ValidateTiffRowOrder(Json::object({{"rowOrder", "unknown"}}), spec); });
    Reject([&] { ValidateTiffRowOrder(Json::object({{"rowOrder", 1}}), spec); });
    for (const auto description : {"RGBWSV;rowOrder=bad", "RGBWSVT;rowOrder=max_y_first"})
    {
        { auto file = Open(directory / "production.tif", "r+");
          Require(TIFFSetField(file.get(), TIFFTAG_IMAGEDESCRIPTION, description) == 1, "set description failed");
          Require(TIFFRewriteDirectory(file.get()) == 1, "rewrite directory failed"); }
        Reject([&] { (void)read_rgbwsv_tiff(directory / "production.tif"); });
        Reject([&] { (void)read_rgbwsv_tiff_stats(directory / "production.tif"); });
    }
    { auto file = Open(directory / "production.tif", "r+");
      Require(TIFFSetField(file.get(), TIFFTAG_IMAGEDESCRIPTION, "RGBWSV;rowOrder=max_y_first") == 1, "set failed");
      Require(TIFFSetField(file.get(), TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT) == 1, "orientation set failed");
      Require(TIFFRewriteDirectory(file.get()) == 1, "rewrite failed"); }
    Reject([&] { (void)read_rgbwsv_tiff(directory / "production.tif"); });
}

void PackageBinding(const std::filesystem::path& directory)
{
    RgbwsvProductionPackageWriteRequest request;
    request.packageDir = directory / "package";
    request.sourceConfigPath = "fixture/profile.json";
    request.sourceModelPath = "fixture/model.obj";
    request.sourceFormat = "obj";
    request.requestedPipelineMode = "global_surface_shell";
    request.effectivePipelineMode = "global_surface_shell";
    request.productionAcceptance = "admitted";
    request.grid.widthPx = 5; request.grid.heightPx = 7; request.grid.layerCount = 1;
    request.preview.enabled = true;
    request.preview.interval = 1;
    request.preview.outputpolicy = "tiff_native_with_diagnostics";
    RgbwsvProductionLayer layer;
    layer.widthPx = 5; layer.heightPx = 7; layer.zMm = 0.005;
    layer.channels.assign(5U * 7U * 6U, 255U);
    layer.channels[3] = 0;
    request.layers.push_back(layer);
    Require(WriteRgbwsvProductionPackage(request).strictProtocolValidated, "production package failed");
    TiffLayerSource reader;
    const auto package = reader.IndexPackage(request.packageDir / "manifest.json");
    Require(package.rowOrder == TiffRowOrder::MaxYFirst, "production manifest rowOrder missing");
    Require(reader.LoadLayer(package.layers.front()).buffer->pixels == layer.channels,
        "package preview buffer is not canonical");
    std::ifstream ppm(request.packageDir / "preview/white_w_000000.ppm", std::ios::binary);
    std::string magic; int width = 0, height = 0, maximum = 0;
    ppm >> magic >> width >> height >> maximum;
    Require(magic == "P6" && width == 5 && height == 7 && maximum == 255, "diagnostic PPM missing");
    ppm.get();
    std::vector<std::uint8_t> rgb(5 * 7 * 3);
    ppm.read(reinterpret_cast<char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    for (std::size_t pixel = 0; pixel < 35; ++pixel)
    {
        const bool occupied = rgb[pixel * 3] != 255 || rgb[pixel * 3 + 1] != 255 || rgb[pixel * 3 + 2] != 255;
        Require(occupied == (pixel == 30), "diagnostic preview row orientation mismatch");
    }
    std::ifstream input(request.packageDir / "manifest.json");
    const auto original = Json::parse(input);
    input.close();
    for (const auto value : {"min_y_first", "unknown"})
    {
        auto root = original.as_object();
        auto tiff = original.at("tiff").as_object();
        tiff["rowOrder"] = value;
        root["tiff"] = Json{std::move(tiff)};
        { std::ofstream output(request.packageDir / "manifest.json"); output << Json{std::move(root)}.dump(2); }
        Reject([&] { (void)validate_slice_package(request.packageDir); });
        Reject([&] {
            TiffLayerSource invalid;
            const auto index = invalid.IndexPackage(request.packageDir / "manifest.json");
            (void)invalid.LoadLayer(index.layers.front());
        });
    }
}
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 4 && std::string_view{argv[1]} == "--rewrite-legacy-package")
        {
            RewriteLegacyPackageForAudit(argv[2], argv[3]);
            return 0;
        }
        const auto directory = std::filesystem::temp_directory_path() / ("slicesoft_row_layout_"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);
        Matrix(directory);
        ProductionAndRejection(directory);
        PackageBinding(directory);
        std::cout << "PASS: 24 storage cases, 16 production orientations, invalid metadata\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
