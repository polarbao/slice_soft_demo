#include "slicer_core/geometry/ContactPostureMetrics.h"
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
        / "output/benchmarks/stage16/posture_baseline.json"};
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
                << "Usage: stage16_posture_baseline "
                   "[--source-root <path>] [--output <file>]\n";
            std::exit(0);
        }
        else
        {
            throw std::invalid_argument(
                "unsupported argument: " + argument);
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
        {
            "reality_101",
            "model/obj/reality/260729-16-39-21-792-segment_101.txt.obj",
        },
        {
            "reality_102",
            "model/obj/reality/260729-16-39-48-086-segment_102.txt.obj",
        },
        {
            "reality_103",
            "model/obj/reality/260729-16-39-55-435-segment_103.txt.obj",
        },
        {
            "reality_104",
            "model/obj/reality/260729-16-40-09-567-segment_104.txt.obj",
        },
        {
            "reality_105",
            "model/obj/reality/260729-16-40-21-739-segment_105.txt.obj",
        },
        {
            "standard_nai_you",
            "model/obj/nai_you_new/MF_nai_you.obj",
        },
    };
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to read asset: " + path.generic_string());
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

slicer_core::Json MakeBoundingBox(const slicer_core::BoundingBox& box)
{
    return slicer_core::Json::object({
        {
            "min",
            slicer_core::Json::array(
                {box.min.x, box.min.y, box.min.z}),
        },
        {
            "max",
            slicer_core::Json::array(
                {box.max.x, box.max.y, box.max.z}),
        },
    });
}

slicer_core::Json MakePolicy(
    const slicer_core::ContactPostureMetricPolicy& policy)
{
    return slicer_core::Json::object({
        {"sideBandFraction", policy.sidebandfraction},
        {"firstSlabFraction", policy.firstslabfraction},
        {"layerThicknessMm", policy.layerthicknessmm},
        {
            "maximumAbsoluteCandidateAngleDeg",
            policy.maximumabsolutecandidateangledeg,
        },
        {
            "requiredPositiveZEnvelopeDeltaMm",
            policy.requiredpositivezenvelopedeltamm,
        },
        {
            "requiredPositiveYTipDeltaMm",
            policy.requiredpositiveytipdeltamm,
        },
        {
            "contactAreaDefinition",
            "xy_projected_triangle_area_clipped_below_z",
        },
    });
}

slicer_core::Json MakeMetrics(
    const slicer_core::ContactPostureMetrics& metrics)
{
    return slicer_core::Json::object({
        {"valid", metrics.valid},
        {"rejectionReason", metrics.rejectionreason},
        {"longAxisLengthMm", metrics.longaxislengthmm},
        {"transverseSpanMm", metrics.transversespanmm},
        {"leftBandMinimumZMm", metrics.leftbandminimumzmm},
        {"rightBandMinimumZMm", metrics.rightbandminimumzmm},
        {"sideEnvelopeDeltaMm", metrics.sideenvelopedeltamm},
        {
            "centerToSideEnvelopeDeltaMm",
            metrics.centertosideenvelopedeltamm,
        },
        {"firstHalfSlabAreaMm2", metrics.firsthalfslabareamm2},
        {"firstSlabAreaMm2", metrics.firstslabareamm2},
        {"secondSlabAreaMm2", metrics.secondslabareamm2},
        {"candidateAngleDeg", metrics.candidateangledeg},
        {"positiveYTipWidthDeltaMm", metrics.positiveytipwidthdeltamm},
        {
            "leftBandVertexCount",
            static_cast<std::uint64_t>(metrics.leftbandvertexcount),
        },
        {
            "rightBandVertexCount",
            static_cast<std::uint64_t>(metrics.rightbandvertexcount),
        },
        {
            "centerBandVertexCount",
            static_cast<std::uint64_t>(metrics.centerbandvertexcount),
        },
        {"candidateAngleWithinLimit", metrics.candidateanglewithinlimit},
        {"positiveZConstraintSatisfied", metrics.positivezconstraintsatisfied},
        {"positiveYConstraintSatisfied", metrics.positiveyconstraintsatisfied},
    });
}

slicer_core::Json MeasureAsset(
    const Options& options,
    const AssetSpec& spec,
    const slicer_core::ContactPostureMetricPolicy& policy,
    bool& pass)
{
    const std::filesystem::path path{
        (options.sourceroot / spec.relativepath).lexically_normal()};
    slicer_core::ModelLoadConfig loadConfig;
    loadConfig.input.model_path = path;
    loadConfig.input.format = "obj";
    loadConfig.auto_orient.enabled = true;
    loadConfig.auto_orient.max_height_mm = 9.0;
    const slicer_core::ModelReport model =
        slicer_core::load_model_report(loadConfig, options.sourceroot);
    const slicer_core::ContactPostureMetrics metrics =
        slicer_core::MeasureContactPosture(model, policy);
    const bool assetPass = metrics.valid
        && metrics.candidateanglewithinlimit
        && metrics.positivezconstraintsatisfied
        && metrics.positiveyconstraintsatisfied;
    pass = pass && assetPass;

    return slicer_core::Json::object({
        {"assetId", spec.assetid},
        {"modelPath", spec.relativepath.generic_string()},
        {"sourceSha256", slicer_core::ComputeSha256(ReadFile(path))},
        {
            "triangleCount",
            static_cast<std::uint64_t>(model.triangle_count),
        },
        {"selectedOrientation", model.auto_orient.selected_orientation},
        {"rotationDeg", slicer_core::Json::array({
            model.auto_orient.rotation_deg.at(0U),
            model.auto_orient.rotation_deg.at(1U),
            model.auto_orient.rotation_deg.at(2U),
        })},
        {"boundingBoxMm", MakeBoundingBox(model.bbox_mm)},
        {"metrics", MakeMetrics(metrics)},
        {"pass", assetPass},
    });
}

int Run(const Options& options)
{
    slicer_core::ContactPostureMetricPolicy policy;
    slicer_core::Json::Array assets;
    bool pass{true};
    for (const AssetSpec& spec : AssetCatalog())
    {
        assets.push_back(MeasureAsset(options, spec, policy, pass));
    }

    const slicer_core::Json report = slicer_core::Json::object({
        {"schema", "slicesoft.stage16.posture_baseline.1"},
        {"stage", "16B-01"},
        {"policy", MakePolicy(policy)},
        {"assets", slicer_core::Json(std::move(assets))},
        {"assetCount", 6},
        {"pass", pass},
    });
    std::filesystem::create_directories(options.outputpath.parent_path());
    std::ofstream output(options.outputpath, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error(
            "failed to write report: " + options.outputpath.generic_string());
    }
    output << report.dump(2) << '\n';
    output.close();
    std::cout << "postureBaseline=" << options.outputpath.generic_string()
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
        std::cerr << "stage16_posture_baseline error: "
                  << error.what() << '\n';
        return 1;
    }
}
