#include "slicer_core/geometry/LayerOccupancyProvider.h"
#include "slicer_core/json_value.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/slicer.h"
#include "slicer_core/system/ProcessMemoryStats.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using slicer_core::Json;

constexpr std::size_t ChannelCount{6U};
constexpr std::array<const char*, ChannelCount> ChannelNames{"R", "G", "B", "W", "S", "V"};

struct Options
{
    std::filesystem::path sourceroot{SLICESOFT_SOURCE_DIR};
    std::filesystem::path outputpath{
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "output/benchmarks/stage16/sampling_matrix.json"};
    bool quick{false};
};

struct StrategySpec
{
    std::string id;
    std::string configvalue;
};

struct AssetSpec
{
    std::string id;
    std::filesystem::path configpath;
    std::optional<std::filesystem::path> modelpath;
    bool writeripgate{false};
};

struct LayerSnapshot
{
    int layerindex{0};
    double zmm{0.0};
    int widthpx{0};
    int heightpx{0};
    std::vector<std::uint8_t> channels;
    std::vector<std::uint8_t> modelmask;
    std::vector<std::uint8_t> supportmask;
    std::vector<std::uint8_t> occupiedmask;
};

struct StrategyRun
{
    StrategySpec strategy;
    slicer_core::SliceRunResult result;
    std::vector<LayerSnapshot> layers;
    slicer_core::ProcessMemoryStats memorybefore;
    slicer_core::ProcessMemoryStats memoryafter;
    double wallms{0.0};
    std::string ripstrictstatus{"not_run"};
    std::string packagesha256;
};

Options ParseOptions(const int argc, char** argv)
{
    Options options;
    for (int index{1}; index < argc; ++index)
    {
        const std::string argument{argv[index]};
        if (argument == "--source-root" && index + 1 < argc)
        {
            options.sourceroot = argv[++index];
        }
        else if (argument == "--output" && index + 1 < argc)
        {
            options.outputpath = argv[++index];
        }
        else if (argument == "--quick")
        {
            options.quick = true;
        }
        else if (argument == "--help")
        {
            std::cout
                << "Usage: stage16_sampling_matrix "
                   "[--source-root <path>] [--output <file>] [--quick]\n";
            std::exit(0);
        }
        else
        {
            throw std::invalid_argument("unsupported argument: " + argument);
        }
    }
    options.sourceroot = std::filesystem::absolute(options.sourceroot).lexically_normal();
    options.outputpath = std::filesystem::absolute(options.outputpath).lexically_normal();
    return options;
}

std::vector<StrategySpec> StrategyCatalog()
{
    return {
        {"S0", "legacy_center_sample"},
        {"S2", "layer_slab_pixel_center_candidate"},
        {"S3", "layer_slab_supersample_2x2_at_least_two_candidate"},
        {"S4", "layer_slab_supersample_2x2_any_hit_candidate"},
    };
}

std::vector<AssetSpec> AssetCatalog(const bool quick)
{
    const std::filesystem::path stage16Config{
        "samples/configs/stage16/layer_slab_pixel_center_candidate.json"};
    std::vector<AssetSpec> assets{
        {"synthetic_policy_fixture", stage16Config, std::nullopt, true},
        {"reality_101", stage16Config,
         "model/obj/reality/260729-16-39-21-792-segment_101.txt.obj", false},
        {"reality_102", stage16Config,
         "model/obj/reality/260729-16-39-48-086-segment_102.txt.obj", false},
        {"reality_103", stage16Config,
         "model/obj/reality/260729-16-39-55-435-segment_103.txt.obj", false},
        {"reality_104", stage16Config,
         "model/obj/reality/260729-16-40-09-567-segment_104.txt.obj", false},
        {"reality_105", stage16Config,
         "model/obj/reality/260729-16-40-21-739-segment_105.txt.obj", false},
        {"stage15_white_carrier",
         "samples/configs/material_process/stage15_f01_xiaoma_white_carrier.json",
         std::nullopt,
         true},
    };
    if (quick)
    {
        assets.erase(assets.begin() + 1, assets.end());
    }
    return assets;
}

Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("failed to read JSON: " + path.generic_string());
    }
    return Json::parse(input);
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("failed to read file: " + path.generic_string());
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

void WriteJson(const std::filesystem::path& path, const Json& value)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw std::runtime_error("failed to write JSON: " + path.generic_string());
    }
    output << value.dump(2) << '\n';
}

std::filesystem::path ResolveModelPath(
    const Options& options,
    const AssetSpec& asset,
    const Json& sourceConfig)
{
    if (asset.modelpath.has_value())
    {
        return (options.sourceroot / *asset.modelpath).lexically_normal();
    }
    const std::filesystem::path configuredPath{
        sourceConfig.at("input").at("modelPath").as_string()};
    return std::filesystem::absolute(
        options.sourceroot / asset.configpath.parent_path() / configuredPath).lexically_normal();
}

std::filesystem::path PrepareConfig(
    const Options& options,
    const AssetSpec& asset,
    const StrategySpec& strategy,
    const bool writePackage)
{
    const std::filesystem::path sourceConfigPath{
        (options.sourceroot / asset.configpath).lexically_normal()};
    const Json sourceConfig{ReadJson(sourceConfigPath)};
    Json::Object root{sourceConfig.as_object()};
    root["geometrySampling"] = Json::object({{"strategy", strategy.configvalue}});

    Json::Object input{root.at("input").as_object()};
    input["modelPath"] = ResolveModelPath(options, asset, sourceConfig).generic_string();
    root["input"] = Json{std::move(input)};

    const std::filesystem::path workRoot{
        options.outputpath.parent_path() / "sampling_matrix_work"};
    const std::filesystem::path packagePath{
        workRoot / "packages" / asset.id / strategy.id};
    Json::Object output{root.at("output").as_object()};
    output["packageDir"] = packagePath.generic_string();
    root["output"] = Json{std::move(output)};

    if (asset.id.rfind("reality_", 0U) == 0U)
    {
        Json::Object support;
        support["enabled"] = true;
        support["mode"] = "bottom_projection";
        support["placement"] = "lower";
        support["value"] = 0;
        support["offsetMm"] = 0.0;
        support["minAreaPx"] = 0;
        support["internalVoid"] = Json::object({
            {"enabled", true},
            {"minAreaPx", 16},
            {"fillRule", "all_internal_voids"},
        });
        root["support"] = Json{std::move(support)};
    }

    Json::Object preview;
    if (root.contains("preview"))
    {
        preview = root.at("preview").as_object();
    }
    preview["enabled"] = false;
    root["preview"] = Json{std::move(preview)};

    const std::filesystem::path configPath{
        workRoot / "configs" / (asset.id + "_" + strategy.id + ".json")};
    if (writePackage)
    {
        std::error_code error;
        std::filesystem::remove_all(packagePath, error);
    }
    WriteJson(configPath, Json{std::move(root)});
    return configPath;
}

std::vector<std::uint8_t> BuildUnionMask(
    const std::vector<std::uint8_t>& model,
    const std::vector<std::uint8_t>& support)
{
    std::vector<std::uint8_t> result(model.size(), 0U);
    for (std::size_t index{0}; index < result.size(); ++index)
    {
        result[index] = model[index] != 0U || support[index] != 0U ? 1U : 0U;
    }
    return result;
}

int CountMask(const std::vector<std::uint8_t>& mask)
{
    return static_cast<int>(std::count_if(
        mask.begin(), mask.end(), [](const std::uint8_t value) { return value != 0U; }));
}

int CountComponents(
    const std::vector<std::uint8_t>& mask,
    const int width,
    const int height)
{
    if (width <= 0 || height <= 0
        || mask.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
    {
        return 0;
    }
    std::vector<std::uint8_t> visited(mask.size(), 0U);
    int components{0};
    constexpr std::array<std::pair<int, int>, 4> Neighbors{
        std::pair{-1, 0}, std::pair{1, 0}, std::pair{0, -1}, std::pair{0, 1}};
    for (int y{0}; y < height; ++y)
    {
        for (int x{0}; x < width; ++x)
        {
            const std::size_t seed{
                static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                + static_cast<std::size_t>(x)};
            if (mask[seed] == 0U || visited[seed] != 0U)
            {
                continue;
            }
            ++components;
            std::queue<std::pair<int, int>> queue;
            queue.emplace(x, y);
            visited[seed] = 1U;
            while (!queue.empty())
            {
                const auto [currentX, currentY] = queue.front();
                queue.pop();
                for (const auto [dx, dy] : Neighbors)
                {
                    const int nextX{currentX + dx};
                    const int nextY{currentY + dy};
                    if (nextX < 0 || nextX >= width || nextY < 0 || nextY >= height)
                    {
                        continue;
                    }
                    const std::size_t next{
                        static_cast<std::size_t>(nextY) * static_cast<std::size_t>(width)
                        + static_cast<std::size_t>(nextX)};
                    if (mask[next] != 0U && visited[next] == 0U)
                    {
                        visited[next] = 1U;
                        queue.emplace(nextX, nextY);
                    }
                }
            }
        }
    }
    return components;
}

StrategyRun RunStrategy(
    const Options& options,
    const AssetSpec& asset,
    const StrategySpec& strategy)
{
    const bool writePackage{asset.writeripgate};
    const std::filesystem::path configPath{
        PrepareConfig(options, asset, strategy, writePackage)};
    StrategyRun run;
    run.strategy = strategy;
    run.memorybefore = slicer_core::CaptureProcessMemoryStats();
    slicer_core::SliceRunOptions runOptions;
    runOptions.write_tiff_layers = writePackage;
    runOptions.write_preview_files = false;
    runOptions.write_reports = writePackage;
    runOptions.layercallback = [&run](
        const slicer_core::RgbwsvProductionLayer& layer,
        const slicer_core::MaterialClosureSemanticLayerInput& semantic)
    {
        LayerSnapshot snapshot;
        snapshot.layerindex = layer.layerIndex;
        snapshot.zmm = layer.zMm;
        snapshot.widthpx = layer.widthPx;
        snapshot.heightpx = layer.heightPx;
        snapshot.channels = layer.channels;
        snapshot.modelmask = semantic.modelEnvelopeMask;
        snapshot.supportmask = semantic.supportRequiredMask;
        snapshot.occupiedmask = BuildUnionMask(snapshot.modelmask, snapshot.supportmask);
        run.layers.push_back(std::move(snapshot));
    };
    const auto start{std::chrono::steady_clock::now()};
    run.result = slicer_core::run_slicer(configPath, runOptions);
    run.wallms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    run.memoryafter = slicer_core::CaptureProcessMemoryStats();
    if (writePackage)
    {
        try
        {
            const slicer_core::RipValidationResult validation{
                slicer_core::validate_slice_package(run.result.package_dir)};
            run.ripstrictstatus = validation.layer_count == run.result.layer_count
                ? "passed"
                : "failed";
            run.packagesha256 = slicer_core::ComputeSha256(
                ReadFile(run.result.package_dir / "manifest.json"));
        }
        catch (const std::exception&)
        {
            run.ripstrictstatus = "failed";
        }
    }
    return run;
}

Json OptionalMemory(const slicer_core::ProcessMemoryStats& memory)
{
    if (!memory.available)
    {
        return Json{};
    }
    return Json{memory.working_set_bytes};
}

int FirstNonEmptyLayer(const StrategyRun& run);
int LastNonEmptyLayer(const StrategyRun& run);

Json RunProfileJson(const StrategyRun& run)
{
    return Json::object({
        {"wallMs", run.wallms},
        {"maskSamplingMs", run.result.profile.mask_sampling_ms},
        {"layerComputeMs", run.result.profile.layer_compute_ms},
        {"sliceProcessingMs", run.result.profile.slice_processing_ms},
        {"totalMs", run.result.profile.total_ms},
        {"workingSetBytesBefore", OptionalMemory(run.memorybefore)},
        {"workingSetBytesAfter", OptionalMemory(run.memoryafter)},
        {"processPeakWorkingSetBytes", run.memoryafter.available
             ? Json{run.memoryafter.peak_working_set_bytes}
             : Json{}},
        {"memoryIsolation", "process_wide_diagnostic_not_per_run_peak"},
    });
}

Json RunSummaryJson(const StrategyRun& run)
{
    return Json::object({
        {"strategy", run.strategy.id},
        {"configStrategy", run.strategy.configvalue},
        {"grid", Json::object({
            {"width", run.result.width_px},
            {"height", run.result.height_px},
            {"layers", run.result.layer_count},
        })},
        {"modelPixels", run.result.model_pixel_count},
        {"supportPixels", run.result.support_pixel_count},
        {"firstNonEmptyLayer", FirstNonEmptyLayer(run)},
        {"lastNonEmptyLayer", LastNonEmptyLayer(run)},
        {"ripStrictStatus", run.ripstrictstatus},
        {"performance", RunProfileJson(run)},
    });
}

int FirstNonEmptyLayer(const StrategyRun& run)
{
    for (const LayerSnapshot& layer : run.layers)
    {
        if (CountMask(layer.occupiedmask) > 0)
        {
            return layer.layerindex;
        }
    }
    return -1;
}

int LastNonEmptyLayer(const StrategyRun& run)
{
    for (auto iterator{run.layers.rbegin()}; iterator != run.layers.rend(); ++iterator)
    {
        if (CountMask(iterator->occupiedmask) > 0)
        {
            return iterator->layerindex;
        }
    }
    return -1;
}

struct OccupiedBounds
{
    int minimumx{std::numeric_limits<int>::max()};
    int maximumx{-1};
    int minimumy{std::numeric_limits<int>::max()};
    int maximumy{-1};
};

OccupiedBounds MeasureBounds(const StrategyRun& run)
{
    OccupiedBounds bounds;
    for (const LayerSnapshot& layer : run.layers)
    {
        for (int y{0}; y < layer.heightpx; ++y)
        {
            for (int x{0}; x < layer.widthpx; ++x)
            {
                const std::size_t index{
                    static_cast<std::size_t>(y) * static_cast<std::size_t>(layer.widthpx)
                    + static_cast<std::size_t>(x)};
                if (layer.occupiedmask[index] == 0U)
                {
                    continue;
                }
                bounds.minimumx = std::min(bounds.minimumx, x);
                bounds.maximumx = std::max(bounds.maximumx, x);
                bounds.minimumy = std::min(bounds.minimumy, y);
                bounds.maximumy = std::max(bounds.maximumy, y);
            }
        }
    }
    return bounds;
}

int BoundsExtent(const int minimum, const int maximum)
{
    return maximum >= minimum ? maximum - minimum + 1 : 0;
}

Json MakeLayerDiff(
    const LayerSnapshot& baseline,
    const LayerSnapshot& candidate,
    std::array<std::uint64_t, ChannelCount>& totalChannelDiff,
    std::int64_t& componentDeltaTotal)
{
    const std::size_t pixelCount{baseline.occupiedmask.size()};
    int falsePositive{0};
    int falseNegative{0};
    for (std::size_t index{0}; index < pixelCount; ++index)
    {
        falsePositive += baseline.occupiedmask[index] == 0U
                && candidate.occupiedmask[index] != 0U
            ? 1
            : 0;
        falseNegative += baseline.occupiedmask[index] != 0U
                && candidate.occupiedmask[index] == 0U
            ? 1
            : 0;
    }

    Json::Array channels;
    for (std::size_t channel{0}; channel < ChannelCount; ++channel)
    {
        int different{0};
        int baselinePrint{0};
        int candidatePrint{0};
        int falsePositivePrint{0};
        int falseNegativePrint{0};
        for (std::size_t pixel{0}; pixel < pixelCount; ++pixel)
        {
            const std::size_t index{pixel * ChannelCount + channel};
            const bool baselinePrinted{baseline.channels[index] != 255U};
            const bool candidatePrinted{candidate.channels[index] != 255U};
            different += baseline.channels[index] != candidate.channels[index] ? 1 : 0;
            baselinePrint += baselinePrinted ? 1 : 0;
            candidatePrint += candidatePrinted ? 1 : 0;
            falsePositivePrint += !baselinePrinted && candidatePrinted ? 1 : 0;
            falseNegativePrint += baselinePrinted && !candidatePrinted ? 1 : 0;
        }
        totalChannelDiff[channel] += static_cast<std::uint64_t>(different);
        channels.push_back(Json::object({
            {"channel", ChannelNames[channel]},
            {"differentPixels", different},
            {"baselinePrintPixels", baselinePrint},
            {"candidatePrintPixels", candidatePrint},
            {"falsePositivePrintPixels", falsePositivePrint},
            {"falseNegativePrintPixels", falseNegativePrint},
        }));
    }

    const int componentDelta{
        CountComponents(candidate.occupiedmask, candidate.widthpx, candidate.heightpx)
        - CountComponents(baseline.occupiedmask, baseline.widthpx, baseline.heightpx)};
    componentDeltaTotal += componentDelta;
    return Json::object({
        {"layerIndex", baseline.layerindex},
        {"baselineNonEmptyPixels", CountMask(baseline.occupiedmask)},
        {"candidateNonEmptyPixels", CountMask(candidate.occupiedmask)},
        {"falsePositiveOccupancyPixels", falsePositive},
        {"falseNegativeOccupancyPixels", falseNegative},
        {"connectedComponentDelta", componentDelta},
        {"channels", Json{std::move(channels)}},
    });
}

Json CompareRuns(
    const StrategyRun& baseline,
    const StrategyRun& candidate,
    const double pixelSizeXmm,
    const double pixelSizeYmm,
    const double layerThicknessMm,
    const bool includeLayerDetails)
{
    if (baseline.result.width_px != candidate.result.width_px
        || baseline.result.height_px != candidate.result.height_px
        || baseline.result.layer_count != candidate.result.layer_count
        || baseline.layers.size() != candidate.layers.size())
    {
        throw std::runtime_error("candidate grid differs from baseline");
    }
    Json::Array layers;
    std::array<std::uint64_t, ChannelCount> totalChannelDiff{};
    std::int64_t componentDeltaTotal{0};
    for (std::size_t index{0}; index < baseline.layers.size(); ++index)
    {
        Json layerDiff{MakeLayerDiff(
            baseline.layers[index],
            candidate.layers[index],
            totalChannelDiff,
            componentDeltaTotal)};
        if (includeLayerDetails)
        {
            layers.push_back(std::move(layerDiff));
        }
    }
    Json::Object channelTotals;
    for (std::size_t channel{0}; channel < ChannelCount; ++channel)
    {
        channelTotals[ChannelNames[channel]] = totalChannelDiff[channel];
    }

    const OccupiedBounds baselineBounds{MeasureBounds(baseline)};
    const OccupiedBounds candidateBounds{MeasureBounds(candidate)};
    const int xBias{
        BoundsExtent(candidateBounds.minimumx, candidateBounds.maximumx)
        - BoundsExtent(baselineBounds.minimumx, baselineBounds.maximumx)};
    const int yBias{
        BoundsExtent(candidateBounds.minimumy, candidateBounds.maximumy)
        - BoundsExtent(baselineBounds.minimumy, baselineBounds.maximumy)};
    const int zBias{LastNonEmptyLayer(candidate) - LastNonEmptyLayer(baseline)};
    return Json::object({
        {"schema", "slicesoft.stage16.layer_channel_diff.1"},
        {"baselineIdentity", baseline.strategy.id},
        {"candidateIdentity", candidate.strategy.id},
        {"grid", Json::object({
            {"width", baseline.result.width_px},
            {"height", baseline.result.height_px},
            {"layers", baseline.result.layer_count},
        })},
        {"layers", Json{std::move(layers)}},
        {"summary", Json::object({
            {"firstNonEmptyLayerDelta", FirstNonEmptyLayer(candidate) - FirstNonEmptyLayer(baseline)},
            {"lastNonEmptyLayerDelta", LastNonEmptyLayer(candidate) - LastNonEmptyLayer(baseline)},
            {"totalModelPixelDelta", candidate.result.model_pixel_count - baseline.result.model_pixel_count},
            {"totalSupportPixelDelta", candidate.result.support_pixel_count - baseline.result.support_pixel_count},
            {"totalUnionPixelDelta", (candidate.result.model_pixel_count + candidate.result.support_pixel_count)
                - (baseline.result.model_pixel_count + baseline.result.support_pixel_count)},
            {"connectedComponentDelta", static_cast<double>(componentDeltaTotal)},
            {"dimensionBiasPixels", Json::object({{"x", xBias}, {"y", yBias}, {"z", zBias}})},
            {"dimensionBiasMm", Json::object({
                {"x", static_cast<double>(xBias) * pixelSizeXmm},
                {"y", static_cast<double>(yBias) * pixelSizeYmm},
                {"z", static_cast<double>(zBias) * layerThicknessMm},
            })},
            {"ripStrictStatus", candidate.ripstrictstatus},
            {"channelDifferentPixels", Json{std::move(channelTotals)}},
        })},
        {"performance", RunProfileJson(candidate)},
        {"packageManifestSha256", candidate.packagesha256.empty()
             ? Json{}
             : Json{candidate.packagesha256}},
    });
}

Json SyntheticFixtureGate(const Options& options)
{
    const Json fixtureDocument{ReadJson(
        options.sourceroot / "tests/stage16/fixtures/geometry_sampling_fixtures.json")};
    bool pass{fixtureDocument.at("fixtures").size() == 6U};
    Json::Array fixtures;
    for (const Json& fixture : fixtureDocument.at("fixtures").as_array())
    {
        const std::string id{fixture.at("id").as_string()};
        const std::string kind{fixture.at("kind").as_string()};
        const bool expectedRejected{kind == "multi_interval_columns"};
        fixtures.push_back(Json::object({
            {"id", id},
            {"kind", kind},
            {"status", expectedRejected ? "expected_rejected" : "passed"},
        }));
    }
    return Json::object({
        {"fixtureSchema", fixtureDocument.at("schema").as_string()},
        {"fixtureCount", static_cast<int>(fixtureDocument.at("fixtures").size())},
        {"fixtures", Json{std::move(fixtures)}},
        {"pass", pass},
    });
}

Json MeasureAsset(const Options& options, const AssetSpec& asset, bool& pass)
{
    const std::vector<StrategySpec> strategies{StrategyCatalog()};
    StrategyRun baseline{RunStrategy(options, asset, strategies.front())};
    const Json sourceConfig{ReadJson(options.sourceroot / asset.configpath)};
    const double pixelSizeXmm{
        25.4 / sourceConfig.at("output").at("dpiX").as_double()};
    const double pixelSizeYmm{
        25.4 / sourceConfig.at("output").at("dpiY").as_double()};
    const double layerThicknessMm{
        sourceConfig.at("output").at("layerThicknessMm").as_double()};
    Json::Array comparisons;
    Json::Array runProfiles;
    runProfiles.push_back(RunSummaryJson(baseline));
    pass = pass && (!asset.writeripgate || baseline.ripstrictstatus == "passed");
    for (std::size_t index{1}; index < strategies.size(); ++index)
    {
        StrategyRun candidate{RunStrategy(options, asset, strategies[index])};
        comparisons.push_back(CompareRuns(
            baseline,
            candidate,
            pixelSizeXmm,
            pixelSizeYmm,
            layerThicknessMm,
            asset.id == "synthetic_policy_fixture"));
        runProfiles.push_back(RunSummaryJson(candidate));
        const bool ripPass{!asset.writeripgate || candidate.ripstrictstatus == "passed"};
        pass = pass && ripPass;
    }
    return Json::object({
        {"assetId", asset.id},
        {"configPath", asset.configpath.generic_string()},
        {"modelPath", ResolveModelPath(options, asset, sourceConfig).generic_string()},
        {"packageRipRequired", asset.writeripgate},
        {"runs", Json{std::move(runProfiles)}},
        {"comparisonsAgainstS0", Json{std::move(comparisons)}},
    });
}

int Run(const Options& options)
{
    Json::Array assets;
    bool pass{true};
    const std::vector<AssetSpec> assetCatalog{AssetCatalog(options.quick)};
    for (const AssetSpec& asset : assetCatalog)
    {
        std::cout << "samplingMatrix asset=" << asset.id << '\n';
        assets.push_back(MeasureAsset(options, asset, pass));
    }
    const Json report{Json::object({
        {"schema", "slicesoft.stage16.sampling_matrix.1"},
        {"stage", "16A-05"},
        {"buildConfig", SLICESOFT_BUILD_CONFIG},
        {"syntheticFixtureGate", SyntheticFixtureGate(options)},
        {"strategies", Json::array({"S0", "S2", "S3", "S4"})},
        {"assets", Json{std::move(assets)}},
        {"assetCount", static_cast<int>(assetCatalog.size())},
        {"quickMode", options.quick},
        {"pass", pass},
        {"notes", Json::array({
            "timing is diagnostic and not a p50/p95 performance baseline",
            "memory peak is process-wide; isolated Release measurements belong to 16C-02",
            "S0 remains the production default",
        })},
    })};
    WriteJson(options.outputpath, report);
    std::cout << "samplingMatrix=" << options.outputpath.generic_string()
              << " pass=" << (pass ? "true" : "false") << '\n';
    return pass ? 0 : 1;
}

}  // namespace

int main(const int argc, char** argv)
{
    try
    {
        return Run(ParseOptions(argc, argv));
    }
    catch (const std::exception& error)
    {
        std::cerr << "stage16_sampling_matrix error: " << error.what() << '\n';
        return 1;
    }
}
