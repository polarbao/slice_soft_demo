#include "slicer_core/geometry/ContactLevelingAnalyzer.h"
#include "slicer_core/json_value.h"
#include "slicer_core/model.h"
#include "slicer_core/system/Sha256.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

struct Options
{
    std::filesystem::path sourceroot{SLICESOFT_SOURCE_DIR};
    std::filesystem::path outputpath{
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "output/benchmarks/stage16/contact_leveling_diagnostic.json"};
};

struct AssetSpec
{
    std::string assetid;
    std::filesystem::path relativepath;
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
        else if (argument == "--help")
        {
            std::cout
                << "Usage: stage16_contact_leveling_diagnostic "
                   "[--source-root <path>] [--output <file>]\n";
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

std::vector<AssetSpec> AssetCatalog()
{
    return {
        {"reality_101", "model/obj/reality/260729-16-39-21-792-segment_101.txt.obj"},
        {"reality_102", "model/obj/reality/260729-16-39-48-086-segment_102.txt.obj"},
        {"reality_103", "model/obj/reality/260729-16-39-55-435-segment_103.txt.obj"},
        {"reality_104", "model/obj/reality/260729-16-40-09-567-segment_104.txt.obj"},
        {"reality_105", "model/obj/reality/260729-16-40-21-739-segment_105.txt.obj"},
        {"standard_nai_you", "model/obj/nai_you_new/MF_nai_you.obj"},
    };
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("failed to read asset: " + path.generic_string());
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

slicer_core::Json MakeCandidate(
    const slicer_core::ContactLevelingCandidate& candidate)
{
    return slicer_core::Json::object({
        {"available", candidate.available},
        {"status", candidate.status},
        {"rejectionReason", candidate.rejectionreason},
        {"candidateAngleDeg", candidate.candidateangledeg},
        {"baselineFirstHalfSlabAreaMm2", candidate.baselinefirsthalfslabareamm2},
        {"candidateFirstHalfSlabAreaMm2", candidate.candidatefirsthalfslabareamm2},
        {"contactAreaImprovementMm2", candidate.contactareaimprovementmm2},
        {"baselineHeightMm", candidate.baselineheightmm},
        {"candidateHeightMm", candidate.candidateheightmm},
        {"heightIncreaseMm", candidate.heightincreasemm},
        {"baselineFootprintXMm", candidate.baselinefootprintxmm},
        {"candidateFootprintXMm", candidate.candidatefootprintxmm},
        {"footprintIncreaseMm", candidate.footprintincreasemm},
        {"sideEnvelopeDeltaMm", candidate.sideenvelopedeltamm},
        {"positiveZConstraintSatisfied", candidate.positivezconstraintsatisfied},
        {"positiveYConstraintSatisfied", candidate.positiveyconstraintsatisfied},
        {"angleConstraintSatisfied", candidate.angleconstraintsatisfied},
        {"heightConstraintSatisfied", candidate.heightconstraintsatisfied},
        {"footprintConstraintSatisfied", candidate.footprintconstraintsatisfied},
        {"evaluatedCandidateCount", candidate.evaluatedcandidatecount},
    });
}

slicer_core::Json MeasureAsset(
    const Options& options,
    const AssetSpec& spec,
    const slicer_core::ContactPostureMetricPolicy& metricPolicy,
    const slicer_core::ContactLevelingPolicy& levelingPolicy,
    bool& pass)
{
    const std::filesystem::path path{
        (options.sourceroot / spec.relativepath).lexically_normal()};
    slicer_core::ModelLoadConfig config;
    config.input.model_path = path;
    config.input.format = "obj";
    config.auto_orient.enabled = true;
    config.auto_orient.max_height_mm = 9.0;
    const slicer_core::ModelReport model{
        slicer_core::load_model_report(config, options.sourceroot)};
    const slicer_core::ContactLevelingCandidate candidate{
        slicer_core::AnalyzeContactLeveling(model, metricPolicy, levelingPolicy)};
    const bool assetPass{candidate.available
        && candidate.status == "diagnostic_only"
        && candidate.contactareaimprovementmm2 >= -1.0e-9
        && candidate.positivezconstraintsatisfied
        && candidate.positiveyconstraintsatisfied
        && candidate.angleconstraintsatisfied
        && candidate.heightconstraintsatisfied
        && candidate.footprintconstraintsatisfied};
    pass = pass && assetPass;
    return slicer_core::Json::object({
        {"assetId", spec.assetid},
        {"modelPath", spec.relativepath.generic_string()},
        {"sourceSha256", slicer_core::ComputeSha256(ReadFile(path))},
        {"selectedOrientation", model.auto_orient.selected_orientation},
        {"candidate", MakeCandidate(candidate)},
        {"pass", assetPass},
    });
}

int Run(const Options& options)
{
    const slicer_core::ContactPostureMetricPolicy metricPolicy;
    const slicer_core::ContactLevelingPolicy levelingPolicy;
    slicer_core::Json::Array assets;
    bool pass{true};
    for (const AssetSpec& spec : AssetCatalog())
    {
        assets.push_back(MeasureAsset(
            options,
            spec,
            metricPolicy,
            levelingPolicy,
            pass));
    }
    const slicer_core::Json report{slicer_core::Json::object({
        {"schema", "slicesoft.stage16.contact_leveling_diagnostic.1"},
        {"stage", "16B-02"},
        {"mode", "diagnostic_only"},
        {"angleRangeDeg", slicer_core::Json::array({
            levelingPolicy.minimumangledeg,
            levelingPolicy.maximumangledeg,
        })},
        {"coarseAngleIncrementDeg", levelingPolicy.coarseangleincrementdeg},
        {"refineAngleIncrementDeg", levelingPolicy.refineangleincrementdeg},
        {"assets", slicer_core::Json{std::move(assets)}},
        {"assetCount", 6},
        {"pass", pass},
    })};
    std::filesystem::create_directories(options.outputpath.parent_path());
    std::ofstream output(options.outputpath, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error("failed to write report: " + options.outputpath.generic_string());
    }
    output << report.dump(2) << '\n';
    output.close();
    std::cout << "contactLevelingDiagnostic="
              << options.outputpath.generic_string()
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
        std::cerr << "stage16_contact_leveling_diagnostic error: "
                  << error.what() << '\n';
        return 1;
    }
}
