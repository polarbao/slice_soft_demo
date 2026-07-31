#include "slicer_core/json_value.h"
#include "slicer_core/system/ProcessMemoryStats.h"
#include "slicer_core/tiff_io.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using BenchmarkClock = std::chrono::steady_clock;

struct Options
{
    std::filesystem::path outputpath;
    std::filesystem::path workdirectory{
        "output/benchmarks/03d_01/files"};
    std::uint32_t width{1024U};
    std::uint32_t height{2048U};
    std::uint32_t rowsperstrip{64U};
    std::uint32_t tilewidth{256U};
    std::uint32_t tileheight{256U};
    int warmupiterations{1};
    int measurementiterations{5};
};

struct Measurement
{
    double milliseconds{0.0};
    std::uint64_t byteswritten{0U};
};

struct CaseResult
{
    std::string storagemode;
    std::vector<Measurement> measurements;
    double p50milliseconds{0.0};
    double p95milliseconds{0.0};
    double minimummilliseconds{0.0};
    double maximummilliseconds{0.0};
    std::uint64_t byteswritten{0U};
    std::uint64_t writerstagingbytesestimate{0U};
    bool decodedpixelsexact{false};
    slicer_core::ProcessMemoryStats memory;
};

std::string RequireValue(
    const int argc,
    char** argv,
    int& index,
    const std::string& argument)
{
    if (index + 1 >= argc)
    {
        throw std::runtime_error(argument + " requires a value");
    }
    ++index;
    return argv[index];
}

std::uint32_t ParsePositiveU32(
    const std::string& text,
    const std::string& argument)
{
    const unsigned long value = std::stoul(text);
    if (value == 0UL
        || value > static_cast<unsigned long>(
            std::numeric_limits<std::uint32_t>::max()))
    {
        throw std::runtime_error(
            argument + " must be a positive uint32");
    }
    return static_cast<std::uint32_t>(value);
}

Options ParseOptions(const int argc, char** argv)
{
    Options parsed;
    for (int index{1}; index < argc; ++index)
    {
        const std::string argument{argv[index]};
        if (argument == "--output")
        {
            parsed.outputpath =
                RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--work-dir")
        {
            parsed.workdirectory =
                RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--width")
        {
            parsed.width = ParsePositiveU32(
                RequireValue(argc, argv, index, argument),
                argument);
        }
        else if (argument == "--height")
        {
            parsed.height = ParsePositiveU32(
                RequireValue(argc, argv, index, argument),
                argument);
        }
        else if (argument == "--rows-per-strip")
        {
            parsed.rowsperstrip = ParsePositiveU32(
                RequireValue(argc, argv, index, argument),
                argument);
        }
        else if (argument == "--tile-width")
        {
            parsed.tilewidth = ParsePositiveU32(
                RequireValue(argc, argv, index, argument),
                argument);
        }
        else if (argument == "--tile-height")
        {
            parsed.tileheight = ParsePositiveU32(
                RequireValue(argc, argv, index, argument),
                argument);
        }
        else if (argument == "--warmup")
        {
            parsed.warmupiterations =
                std::stoi(RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--iterations")
        {
            parsed.measurementiterations =
                std::stoi(RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--help" || argument == "-h")
        {
            std::cout
                << "tiff_writer_benchmark --output <report.json> "
                << "[--work-dir <directory>] [--width <pixels>] "
                << "[--height <pixels>] [--rows-per-strip <rows>] "
                << "[--tile-width <pixels>] [--tile-height <pixels>] "
                << "[--warmup <count>] [--iterations <count>]\n";
            std::exit(0);
        }
        else
        {
            throw std::runtime_error(
                "unknown argument: " + argument);
        }
    }

    if (parsed.outputpath.empty())
    {
        throw std::runtime_error("--output is required");
    }
    if (parsed.warmupiterations < 0)
    {
        throw std::runtime_error("--warmup must be non-negative");
    }
    if (parsed.measurementiterations <= 0)
    {
        throw std::runtime_error("--iterations must be positive");
    }
    return parsed;
}

std::string BuildTypeName()
{
#ifdef NDEBUG
    return "Release";
#else
    return "Debug";
#endif
}

std::vector<std::uint8_t> MakePixels(
    const std::uint32_t width,
    const std::uint32_t height)
{
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width)
            * height
            * slicer_core::rgbwsv_channel_count,
        255U);
    for (std::uint32_t y{0}; y < height; ++y)
    {
        for (std::uint32_t x{0}; x < width; ++x)
        {
            const std::size_t base =
                (static_cast<std::size_t>(y) * width + x)
                * slicer_core::rgbwsv_channel_count;
            pixels.at(base) =
                static_cast<std::uint8_t>((x * 13U + y * 3U) % 251U);
            pixels.at(base + 1U) =
                static_cast<std::uint8_t>((x * 5U + y * 17U) % 253U);
            pixels.at(base + 2U) =
                static_cast<std::uint8_t>((x * 7U + y * 11U) % 255U);
            pixels.at(base + 3U) =
                ((x + y) % 3U == 0U) ? 0U : 255U;
            pixels.at(base + 4U) =
                ((x + y) % 5U == 0U) ? 127U : 255U;
            pixels.at(base + 5U) =
                ((x + y) % 7U == 0U) ? 64U : 255U;
        }
    }
    return pixels;
}

slicer_core::TiffImageSpec MakeSpec(
    const Options& parsed,
    const slicer_core::TiffStorageMode storageMode)
{
    slicer_core::TiffImageSpec spec;
    spec.width = parsed.width;
    spec.height = parsed.height;
    spec.rows_per_strip = parsed.rowsperstrip;
    spec.tile_width = parsed.tilewidth;
    spec.tile_height = parsed.tileheight;
    spec.storage_mode = storageMode;
    return spec;
}

std::uint64_t CurrentWriterStagingBytesEstimate(
    const slicer_core::TiffImageSpec& spec)
{
    if (spec.storage_mode == slicer_core::TiffStorageMode::Stripped)
    {
        return static_cast<std::uint64_t>(spec.width)
            * spec.height
            * spec.samples_per_pixel;
    }
    const std::uint64_t tilesX =
        (static_cast<std::uint64_t>(spec.width)
         + spec.tile_width
         - 1U)
        / spec.tile_width;
    const std::uint64_t tilesY =
        (static_cast<std::uint64_t>(spec.height)
         + spec.tile_height
         - 1U)
        / spec.tile_height;
    return tilesX
        * tilesY
        * spec.tile_width
        * spec.tile_height
        * spec.samples_per_pixel;
}

double Percentile(
    std::vector<double> values,
    const double percentile)
{
    if (values.empty())
    {
        throw std::runtime_error(
            "cannot calculate percentile of empty measurements");
    }
    std::sort(values.begin(), values.end());
    const std::size_t rank = std::max<std::size_t>(
        1U,
        static_cast<std::size_t>(
            std::ceil(percentile * values.size())));
    return values.at(std::min(rank, values.size()) - 1U);
}

Measurement WriteMeasuredFile(
    const std::filesystem::path& path,
    const slicer_core::TiffImageSpec& spec,
    const std::vector<std::uint8_t>& pixels)
{
    const BenchmarkClock::time_point started =
        BenchmarkClock::now();
    slicer_core::write_rgbwsv_tiff(path, spec, pixels);
    const double milliseconds =
        std::chrono::duration<double, std::milli>(
            BenchmarkClock::now() - started)
            .count();
    return {
        milliseconds,
        std::filesystem::file_size(path)};
}

CaseResult RunCase(
    const Options& parsed,
    const slicer_core::TiffStorageMode storageMode,
    const std::vector<std::uint8_t>& pixels)
{
    const std::string storageName =
        slicer_core::tiff_storage_mode_string(storageMode);
    const slicer_core::TiffImageSpec spec =
        MakeSpec(parsed, storageMode);
    for (int index{0}; index < parsed.warmupiterations; ++index)
    {
        const std::filesystem::path path =
            parsed.workdirectory
            / ("warmup_" + storageName + "_"
               + std::to_string(index) + ".tiff");
        WriteMeasuredFile(path, spec, pixels);
        std::filesystem::remove(path);
    }

    CaseResult result;
    result.storagemode = storageName;
    result.writerstagingbytesestimate =
        CurrentWriterStagingBytesEstimate(spec);
    std::vector<double> durations;
    std::vector<std::filesystem::path> measurementPaths;
    durations.reserve(
        static_cast<std::size_t>(parsed.measurementiterations));
    measurementPaths.reserve(
        static_cast<std::size_t>(parsed.measurementiterations));
    for (int index{0};
         index < parsed.measurementiterations;
         ++index)
    {
        const std::filesystem::path path =
            parsed.workdirectory
            / ("measurement_" + storageName + "_"
               + std::to_string(index) + ".tiff");
        const Measurement current =
            WriteMeasuredFile(path, spec, pixels);
        result.measurements.push_back(current);
        durations.push_back(current.milliseconds);
        measurementPaths.push_back(path);
        result.byteswritten = current.byteswritten;
    }

    result.memory = slicer_core::CaptureProcessMemoryStats();
    for (const std::filesystem::path& path : measurementPaths)
    {
        const slicer_core::TiffReadResult decoded =
            slicer_core::read_rgbwsv_tiff(path);
        if (decoded.spec.storage_mode != storageMode
            || decoded.pixels != pixels)
        {
            throw std::runtime_error(
                storageName
                + " benchmark output failed exact decode validation");
        }
        std::filesystem::remove(path);
    }
    result.decodedpixelsexact = true;
    result.p50milliseconds = Percentile(durations, 0.50);
    result.p95milliseconds = Percentile(durations, 0.95);
    const auto bounds =
        std::minmax_element(durations.begin(), durations.end());
    result.minimummilliseconds = *bounds.first;
    result.maximummilliseconds = *bounds.second;
    return result;
}

slicer_core::Json BuildCaseJson(
    const CaseResult& result,
    const Options& parsed)
{
    slicer_core::Json::Array samples;
    samples.reserve(result.measurements.size());
    for (const Measurement& item : result.measurements)
    {
        samples.push_back(slicer_core::Json::object({
            {"milliseconds", item.milliseconds},
            {"bytesWritten", item.byteswritten}}));
    }
    return slicer_core::Json::object({
        {"backend", "handwritten"},
        {"storageMode", result.storagemode},
        {"rowsPerStrip",
         static_cast<std::uint64_t>(parsed.rowsperstrip)},
        {"tileWidth",
         static_cast<std::uint64_t>(parsed.tilewidth)},
        {"tileHeight",
         static_cast<std::uint64_t>(parsed.tileheight)},
        {"warmupIterations", parsed.warmupiterations},
        {"measurementIterations", parsed.measurementiterations},
        {"samples", slicer_core::Json{std::move(samples)}},
        {"p50Ms", result.p50milliseconds},
        {"p95Ms", result.p95milliseconds},
        {"minimumMs", result.minimummilliseconds},
        {"maximumMs", result.maximummilliseconds},
        {"bytesWritten", result.byteswritten},
        {"decodedPixelsExact", result.decodedpixelsexact},
        {"writerStagingBytesEstimate",
         result.writerstagingbytesestimate},
        {"memory", slicer_core::Json::object({
            {"available", result.memory.available},
            {"samplePoint", "after_measurement_writes_before_decode"},
            {"peakScope", "process_cumulative"},
            {"workingSetBytes", result.memory.working_set_bytes},
            {"peakWorkingSetBytes",
             result.memory.peak_working_set_bytes}})}});
}

slicer_core::Json BuildReport(
    const Options& parsed,
    const std::vector<std::uint8_t>& pixels,
    const CaseResult& stripped,
    const CaseResult& tiled)
{
    return slicer_core::Json::object({
        {"schema", "slicesoft.tiff_writer_benchmark.03d.1"},
        {"stage", "03D-01"},
        {"backend", "handwritten"},
        {"buildType", BuildTypeName()},
        {"scope", "writer_only"},
        {"input", slicer_core::Json::object({
            {"width", static_cast<std::uint64_t>(parsed.width)},
            {"height", static_cast<std::uint64_t>(parsed.height)},
            {"samplesPerPixel", 6},
            {"bitsPerSample", 8},
            {"pixelBytes",
             static_cast<std::uint64_t>(pixels.size())},
            {"bufferGeneratedBeforeTiming", true}})},
        {"contract", slicer_core::Json::object({
            {"channelOrder", slicer_core::Json::array({
                "R", "G", "B", "W", "S", "V"})},
            {"planarConfig", "contiguous"},
            {"compression", "none"},
            {"polarity", "black_is_print"},
            {"printValue", 0},
            {"emptyValue", 255},
            {"errors", slicer_core::Json::array({
                "invalid TIFF dimensions",
                "P0 03B TIFF writer only supports RGBWSV uint8 contiguous pixels",
                "P0 00B TIFF writer only supports RGBWSV uint8 contiguous pixels",
                "pixel buffer size does not match TIFF dimensions"})}})},
        {"cases", slicer_core::Json::array({
            BuildCaseJson(stripped, parsed),
            BuildCaseJson(tiled, parsed)})}});
}

void WriteReport(
    const std::filesystem::path& outputPath,
    const slicer_core::Json& report)
{
    if (!outputPath.parent_path().empty())
    {
        std::filesystem::create_directories(
            outputPath.parent_path());
    }
    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error(
            "failed to write TIFF benchmark report: "
            + outputPath.string());
    }
    output << report.dump(2) << '\n';
}

int RunBenchmark(const Options& parsed)
{
    std::filesystem::create_directories(
        parsed.workdirectory);
    const std::vector<std::uint8_t> pixels =
        MakePixels(parsed.width, parsed.height);
    const CaseResult stripped = RunCase(
        parsed,
        slicer_core::TiffStorageMode::Stripped,
        pixels);
    const CaseResult tiled = RunCase(
        parsed,
        slicer_core::TiffStorageMode::Tiled,
        pixels);
    WriteReport(
        parsed.outputpath,
        BuildReport(parsed, pixels, stripped, tiled));

    std::cout
        << "tiff_writer_benchmark: baseline collected\n"
        << "  backend: handwritten\n"
        << "  buildType: " << BuildTypeName() << '\n'
        << "  iterations: "
        << parsed.measurementiterations << '\n'
        << "  stripped p50/p95 ms: "
        << stripped.p50milliseconds << " / "
        << stripped.p95milliseconds << '\n'
        << "  tiled p50/p95 ms: "
        << tiled.p50milliseconds << " / "
        << tiled.p95milliseconds << '\n'
        << "  report: "
        << parsed.outputpath.generic_string() << '\n';
    return 0;
}

}  // namespace

int main(const int argc, char** argv)
{
    try
    {
        return RunBenchmark(ParseOptions(argc, argv));
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "tiff_writer_benchmark error: "
            << error.what() << '\n';
        return 1;
    }
}
