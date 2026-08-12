#include "slicer_core/geometry/ContactLevelingAnalyzer.h"
#include "slicer_core/geometry/ContactPostureMetrics.h"
#include "slicer_core/json_value.h"
#include "slicer_core/model.h"
#include "slicer_core/slicer.h"
#include "slicer_core/system/Sha256.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using slicer_core::Json;

struct Options
{
    std::filesystem::path sourceroot{SLICESOFT_SOURCE_DIR};
    std::filesystem::path outputpath{
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "output/benchmarks/stage16/posture_matrix.json"};
    bool quick{false};
};

struct AssetSpec
{
    std::string id;
    std::filesystem::path relativepath;
    bool runslice{true};
};

struct PostureRun
{
    std::string id;
    double angledeg{0.0};
    slicer_core::ModelReport model;
    slicer_core::ContactPostureMetrics metrics;
    std::optional<slicer_core::SliceRunResult> sliceresult;
    std::string slicestatus{"not_run"};
    std::string sliceerror;
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
                << "Usage: stage16_posture_matrix "
                   "[--source-root <path>] [--output <file>] [--quick]\n";
            std::exit(0);
        }
        else
        {
            throw std::invalid_argument("unsupported argument: " + argument);
        }
    }
    options.sourceroot =
        std::filesystem::absolute(options.sourceroot).lexically_normal();
    options.outputpath =
        std::filesystem::absolute(options.outputpath).lexically_normal();
    return options;
}

std::vector<AssetSpec> AssetCatalog(const bool quick)
{
    std::vector<AssetSpec> assets{
        {"reality_101", "model/obj/reality/260729-16-39-21-792-segment_101.txt.obj", true},
        {"reality_102", "model/obj/reality/260729-16-39-48-086-segment_102.txt.obj", true},
        {"reality_103", "model/obj/reality/260729-16-39-55-435-segment_103.txt.obj", true},
        {"reality_104", "model/obj/reality/260729-16-40-09-567-segment_104.txt.obj", true},
        {"reality_105", "model/obj/reality/260729-16-40-21-739-segment_105.txt.obj", true},
        {"standard_nai_you", "model/obj/nai_you_new/MF_nai_you.obj", false},
    };
    if (quick)
    {
        assets.erase(assets.begin(), assets.end() - 1);
    }
    return assets;
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

Json ReadJson(const std::filesystem::path& path)
{
    std::istringstream input{ReadFile(path)};
    return Json::parse(input);
}

void WriteJson(const std::filesystem::path& path, const Json& value)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error("failed to write report: " + path.generic_string());
    }
    output << value.dump(2) << '\n';
}

std::filesystem::path PrepareConfig(
    const Options& options,
    const AssetSpec& asset)
{
    const std::filesystem::path sourcePath{
        options.sourceroot
        / "samples/configs/stage16/layer_slab_pixel_center_candidate.json"};
    Json::Object root{ReadJson(sourcePath).as_object()};
    root["geometrySampling"] = Json::object({
        {"strategy", "layer_slab_supersample_2x2_at_least_two_candidate"},
    });
    Json::Object input{root.at("input").as_object()};
    input["modelPath"] =
        (options.sourceroot / asset.relativepath).lexically_normal().generic_string();
    root["input"] = Json{std::move(input)};
    Json::Object output{root.at("output").as_object()};
    output["packageDir"] =
        (options.outputpath.parent_path() / "posture_matrix_work" / asset.id)
            .generic_string();
    root["output"] = Json{std::move(output)};
    root["support"] = Json::object({
        {"enabled", true},
        {"mode", "bottom_projection"},
        {"placement", "lower"},
        {"value", 0},
        {"offsetMm", 0.0},
        {"minAreaPx", 0},
        {"internalVoid", Json::object({
            {"enabled", true},
            {"minAreaPx", 16},
            {"fillRule", "all_internal_voids"},
        })},
    });
    const std::filesystem::path configPath{
        options.outputpath.parent_path()
        / "posture_matrix_work/configs"
        / (asset.id + "_S3.json")};
    WriteJson(configPath, Json{std::move(root)});
    return configPath;
}

slicer_core::ModelReport LoadAsset(
    const Options& options,
    const AssetSpec& asset)
{
    slicer_core::ModelLoadConfig config;
    config.input.model_path = options.sourceroot / asset.relativepath;
    config.input.format = "obj";
    config.auto_orient.enabled = true;
    config.auto_orient.max_height_mm = 9.0;
    return slicer_core::load_model_report(config, options.sourceroot);
}

PostureRun MakeRun(
    const std::string& id,
    const double angleDeg,
    const slicer_core::ModelReport& baseline,
    const slicer_core::ContactPostureMetricPolicy& metricPolicy)
{
    PostureRun run;
    run.id = id;
    run.angledeg = angleDeg;
    run.model = std::abs(angleDeg) <= 1.0e-12
        ? baseline
        : slicer_core::ApplyContactLevelingAngle(baseline, angleDeg);
    run.metrics = slicer_core::MeasureContactPosture(run.model, metricPolicy);
    return run;
}

void SlicePosture(
    PostureRun& run,
    const std::filesystem::path& configPath)
{
    slicer_core::SliceRunOptions options;
    options.write_tiff_layers = false;
    options.write_preview_files = false;
    options.write_reports = false;
    options.modelreportoverride = &run.model;
    try
    {
        run.sliceresult = slicer_core::run_slicer(configPath, options);
        run.slicestatus = "passed";
    }
    catch (const std::exception& error)
    {
        run.slicestatus = "failed";
        run.sliceerror = error.what();
    }
}

Json MakePostureJson(
    const PostureRun& run,
    const PostureRun& baseline)
{
    const double heightMm{run.model.bbox_mm.max.z - run.model.bbox_mm.min.z};
    const double baselineHeightMm{
        baseline.model.bbox_mm.max.z - baseline.model.bbox_mm.min.z};
    const double footprintMm{run.model.bbox_mm.max.x - run.model.bbox_mm.min.x};
    const double baselineFootprintMm{
        baseline.model.bbox_mm.max.x - baseline.model.bbox_mm.min.x};
    const int modelPixels{run.sliceresult.has_value()
        ? run.sliceresult->model_pixel_count
        : 0};
    const int supportPixels{run.sliceresult.has_value()
        ? run.sliceresult->support_pixel_count
        : 0};
    const int baselineModelPixels{baseline.sliceresult.has_value()
        ? baseline.sliceresult->model_pixel_count
        : 0};
    const int baselineSupportPixels{baseline.sliceresult.has_value()
        ? baseline.sliceresult->support_pixel_count
        : 0};
    return Json::object({
        {"postureId", run.id},
        {"angleDeg", run.angledeg},
        {"contact", Json::object({
            {"firstHalfSlabAreaMm2", run.metrics.firsthalfslabareamm2},
            {"improvementAgainstP0Mm2", run.metrics.firsthalfslabareamm2
                - baseline.metrics.firsthalfslabareamm2},
            {"sideEnvelopeDeltaMm", run.metrics.sideenvelopedeltamm},
        })},
        {"geometry", Json::object({
            {"heightMm", heightMm},
            {"heightIncreaseAgainstP0Mm", heightMm - baselineHeightMm},
            {"footprintXMm", footprintMm},
            {"footprintIncreaseAgainstP0Mm", footprintMm - baselineFootprintMm},
            {"positiveZConstraintSatisfied", run.metrics.positivezconstraintsatisfied},
            {"positiveYConstraintSatisfied", run.metrics.positiveyconstraintsatisfied},
        })},
        {"slice", Json::object({
            {"status", run.slicestatus},
            {"error", run.sliceerror},
            {"admissionChangedAgainstP0", run.slicestatus != baseline.slicestatus},
            {"modelPixels", modelPixels},
            {"modelPixelDeltaAgainstP0", modelPixels - baselineModelPixels},
            {"supportPixels", supportPixels},
            {"supportPixelDeltaAgainstP0", supportPixels - baselineSupportPixels},
            {"layerCount", run.sliceresult.has_value()
                ? run.sliceresult->layer_count
                : 0},
        })},
    });
}

Json MeasureAsset(
    const Options& options,
    const AssetSpec& asset,
    bool& pass)
{
    const slicer_core::ModelReport model{LoadAsset(options, asset)};
    const slicer_core::ContactPostureMetricPolicy metricPolicy;
    const slicer_core::ContactLevelingCandidate p3Candidate{
        slicer_core::AnalyzeContactLeveling(model, metricPolicy)};
    const slicer_core::ContactPostureMetrics p0Metrics{
        slicer_core::MeasureContactPosture(model, metricPolicy)};
    const double p2AngleDeg{-p0Metrics.candidateangledeg};

    std::vector<PostureRun> runs;
    runs.push_back(MakeRun("P0", 0.0, model, metricPolicy));
    runs.push_back(MakeRun("P2", p2AngleDeg, model, metricPolicy));
    runs.push_back(MakeRun(
        "P3",
        p3Candidate.available ? p3Candidate.candidateangledeg : 0.0,
        model,
        metricPolicy));
    if (asset.runslice)
    {
        const std::filesystem::path configPath{PrepareConfig(options, asset)};
        for (PostureRun& run : runs)
        {
            SlicePosture(run, configPath);
        }
    }

    const PostureRun& baseline{runs.front()};
    Json::Array postureJson;
    bool assetPass{p0Metrics.valid && p3Candidate.available};
    for (const PostureRun& run : runs)
    {
        postureJson.push_back(MakePostureJson(run, baseline));
        assetPass = assetPass
            && run.metrics.valid
            && run.metrics.positivezconstraintsatisfied
            && run.metrics.positiveyconstraintsatisfied
            && (run.slicestatus == "passed" || !asset.runslice);
    }
    pass = pass && assetPass;
    return Json::object({
        {"assetId", asset.id},
        {"modelPath", asset.relativepath.generic_string()},
        {"sourceSha256", slicer_core::ComputeSha256(
            ReadFile(options.sourceroot / asset.relativepath))},
        {"selectedOrientation", model.auto_orient.selected_orientation},
        {"samplingStrategy", "S3"},
        {"sliceExecutionRequired", asset.runslice},
        {"p3EvaluatedCandidateCount", p3Candidate.evaluatedcandidatecount},
        {"postures", Json{std::move(postureJson)}},
        {"pass", assetPass},
    });
}

int Run(const Options& options)
{
    const std::vector<AssetSpec> catalog{AssetCatalog(options.quick)};
    Json::Array assets;
    bool pass{true};
    for (const AssetSpec& asset : catalog)
    {
        std::cout << "postureMatrix asset=" << asset.id << '\n';
        assets.push_back(MeasureAsset(options, asset, pass));
    }
    const Json report{Json::object({
        {"schema", "slicesoft.stage16.posture_matrix.1"},
        {"stage", "16B-03"},
        {"mode", "diagnostic_only"},
        {"samplingStrategy", "S3"},
        {"postureDefinitions", Json::object({
            {"P0", "existing deterministic right-angle orientation"},
            {"P2", "side boundary-band lower-envelope balance"},
            {"P3", "bounded maximum first-half-slab contact-area search"},
        })},
        {"assets", Json{std::move(assets)}},
        {"assetCount", static_cast<int>(catalog.size())},
        {"quickMode", options.quick},
        {"pass", pass},
        {"notes", Json::array({
            "P0 remains the production default",
            "P2 and P3 are diagnostic-only and do not mutate imported assets",
            "admissionChangedAgainstP0 compares core-only execution status under the same S3 config",
        })},
    })};
    WriteJson(options.outputpath, report);
    std::cout << "postureMatrix=" << options.outputpath.generic_string()
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
        std::cerr << "stage16_posture_matrix error: " << error.what() << '\n';
        return 1;
    }
}
