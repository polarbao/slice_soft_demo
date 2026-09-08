#include "slicer_core/config.h"
#include "slicer_core/model.h"
#include "slicer_core/geometry/TransformedModelAdapter.h"
#include "slicer_core/pipeline/MultiModelProductionService.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/scene/SceneEffectiveConfig.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Utf8Path.h"
#include "slicer_core/system/Sha256.h"
#include "slicer_core/engine/ProductionSliceFacadeFactory.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

slicer_core::Json MakeXPaddingTransferProfile(const std::filesystem::path& source,
    const std::filesystem::path& model, const std::filesystem::path& package, bool pad);
void CompareXPaddingTransfer(const std::filesystem::path& off, const std::filesystem::path& on);
void CheckXPaddingTransferHelper();

namespace
{
using namespace slicer_core;
void Require(bool ok, const std::string& message)
{
    if (!ok) throw std::runtime_error(message);
}
Json Read(const std::filesystem::path& path)
{
    std::ifstream in(path);
    Require(in.good(), "JSON open failed");
    return Json::parse(in);
}
void Write(const std::filesystem::path& path, const Json& data)
{
    std::ofstream out(path);
    out << data.dump(2);
    Require(out.good(), "JSON write failed");
}
std::string Digest(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return ComputeSha256(std::string(std::istreambuf_iterator<char>(in), {}));
}
Json Profile(const std::filesystem::path& source, const std::filesystem::path& model, bool pad, int dpi, double thickness)
{
    auto doc = Read(source / "samples/configs/golden/material_process_top2_fixture.json").as_object();
    auto input = doc.at("input").as_object();
    input["modelPath"] = PathToUtf8(model);
    input["format"] = "obj";
    doc["input"] = Json{input};
    auto output = doc.at("output").as_object();
    output["dpiX"] = dpi;
    output["dpiY"] = dpi;
    output["layerThicknessMm"] = thickness;
    if (pad) output["scenePadToOriginX"] = true;
    doc["output"] = Json{output};
    auto orient = doc.at("autoOrient").as_object();
    orient["enabled"] = false;
    doc["autoOrient"] = Json{orient};
    return Json{doc};
}
MultiModelScene Scene(const SliceConfig& profile, const std::vector<std::filesystem::path>& paths)
{
    MultiModelScene scene;
    scene.sceneid = "xpad-production";
    scene.scenerevision = 1;
    scene.resolvedprofileid = profile.material_process_profile.name.empty()
        ? "xpad-transfer" : profile.material_process_profile.name;
    scene.buildvolume.source = BuildVolumeSource::Fixture;
    scene.buildvolume.widthmm = 230;
    scene.buildvolume.heightmm = 100;
    scene.buildvolume.zlimitmm = 60;
    scene.buildvolume.isfixture = true;
    scene.buildvolume.origin = BuildVolumeOrigin::LowerLeft;
    scene.buildvolume.xdirection = scene.buildvolume.ydirection = BuildVolumeAxisDirection::Positive;
    for (const auto& path : paths)
    {
        auto config = profile;
        config.input.model_path = path;
        const auto model = load_model_report(config, path.parent_path());
        const auto id = std::to_string(scene.models.size());
        ResourceScope scope;
        scope.resourcescopeid = "scope-" + id;
        scope.kind = ResourceScopeKind::ObjDirectory;
        scope.rootpath = path.parent_path();
        scene.resourcescopes.push_back(scope);
        ModelSource source;
        source.modelid = "model-" + id;
        source.sourcepath = path;
        source.format = "obj";
        source.autoorientenabled = false;
        source.resourcescopeid = scope.resourcescopeid;
        source.sourcehash = Digest(path);
        source.resourcehash = ComputeSceneResourceHash(model);
        source.displayname = PathToUtf8(path.stem());
        scene.models.push_back(source);
        SceneModelInstance instance;
        instance.instance.instanceid = "instance-" + id;
        instance.instance.modelid = source.modelid;
        instance.instance.sourcetransformidentity = PathToUtf8(path);
        instance.instance.sourcebboxmm = model.bbox_mm;
        instance.instance.transform.landonbuildplate = true;
        if (paths.size() == 1) // Move only the synthetic fixture, not the real ten assets.
        {
            instance.instance.transform.translatexmm = 21.07984 - model.bbox_mm.min.x;
            instance.instance.transform.translateymm = 5 - model.bbox_mm.min.y;
        }
        const auto transformed = AdaptTransformedModel(model, instance.instance);
        Require(transformed.IsValid(), "source transform failed");
        instance.instance.effectivebboxmm = transformed.geometry.bboxmm;
        instance.requestedtransform = instance.effectivetransform = instance.instance.transform;
        instance.admissionstatus = SceneInstanceAdmissionStatus::Admitted;
        instance.resolvedprofileid = scene.resolvedprofileid;
        scene.instances.push_back(instance);
    }
    return scene;
}
Json RunPackage(const std::filesystem::path& root, const std::filesystem::path& profile,
                const MultiModelScene& scene)
{
    SceneEffectiveConfigRequest config;
    config.scene = scene;
    config.sourcescenepath = root / "scene_config.draft.json";
    config.generatedconfigpath = root / "scene_config.effective.json";
    config.sourceprofileid = scene.resolvedprofileid;
    config.sourceprofileconfigpath = profile;
    config.outputpackagedir = root / "package";
    config.generatedatutc = "2026-09-08T00:00:00.000Z";
    const auto parsed = load_slice_config(profile);
    config.dpix = parsed.output.dpi_x;
    config.dpiy = parsed.output.dpi_y;
    config.layerheightmm = parsed.output.layer_thickness_mm;
    const auto effective = WriteSceneEffectiveConfig(config);
    Require(effective.IsValid(), effective.error ? effective.error->message : "effective failed");
    const auto start = std::chrono::steady_clock::now();
    SliceRunProfile measured;
    if (parsed.transfer_channel_policy.enabled)
    {
        struct Cancel final : api::ICancelToken { bool IsCancelRequested() const noexcept override { return false; } } cancel;
        api::SliceRequest request;
        request.job_id = request.correlation_id = "xpad-transfer";
        request.scene_hash = effective.document.at("identity").at("sceneHash").as_string();
        request.scene_config_path = config.generatedconfigpath;
        request.package_dir = config.outputpackagedir;
        request.output_contract = "p0.rgbwsvt.1";
        const auto result = engine::CreateProductionSliceFacade()->Run(request, cancel, {});
        Require(result.IsOk(), result.Error() ? result.Error()->message + ": " + result.Error()->detail : "T facade failed");
        measured = result.Value()->profile;
    }
    else
    {
        MultiModelProductionRequest request;
        request.effectiveconfigpath = config.generatedconfigpath;
        request.jobid = "xpad";
        request.attemptid = "attempt";
        const auto result = RunMultiModelProductionService(request);
        Require(result.IsValid(), result.error ? result.error->message : "production failed");
        measured = result.profile;
    }
    const double wall = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    const Json timing = Json::object({{"wallMs", wall}, {"profileTotalMs", measured.total_ms},
        {"computeMs", measured.slice_processing_ms}, {"tiffWriteMs", measured.tiff_write_ms},
        {"modelLoadMs", measured.model_load_ms}, {"layerComposeMs", measured.layer_compose_ms},
        {"layerComputeMs", measured.layer_compute_ms}, {"publishMs", measured.package_publish_ms},
        {"reportBuildMs", measured.report_build_ms}, {"reportWriteMs", measured.report_write_ms}});
    Write(root / "timing.json", timing);
    std::cout << "XPAD_TIME " << PathToUtf8(root.filename()) << " " << timing.dump() << std::endl;
    return timing;
}
void ComparePackages(const std::filesystem::path& left, const std::filesystem::path& right)
{
    const auto base = validate_slice_package(left);
    const auto padded = validate_slice_package(right);
    const auto bm = Read(left / "manifest.json");
    const auto pm = Read(right / "manifest.json");
    const double x = bm.at("grid").at("originMm").at(0U).as_double();
    const double px = pm.at("grid").at("originMm").at(0U).as_double();
    const int n = static_cast<int>(std::ceil(x / base.pixel_size_x_mm));
    Require(n > 0 && padded.width_px == base.width_px + n, "production width/padding");
    Require(std::abs(px + n * base.pixel_size_x_mm - x) < 1e-8, "production origin");
    Require(padded.height_px == base.height_px && padded.layer_count == base.layer_count, "Y/Z extent drift");
    for (std::size_t c = 0; c < 6; ++c)
    {
        Require(base.total_channel_stats[c].print_pixels == padded.total_channel_stats[c].print_pixels,
            "persisted print statistics drift");
        Require(padded.total_channel_stats[c].empty_pixels == base.total_channel_stats[c].empty_pixels
            + static_cast<std::uint64_t>(n) * base.height_px * base.layer_count, "persisted empty statistics drift");
    }
    const auto& bl = bm.at("layers").as_array();
    const auto& pl = pm.at("layers").as_array();
    for (std::size_t z = 0; z < bl.size(); ++z)
    {
        const auto a = read_rgbwsv_tiff(left / PathFromUtf8(bl[z].at("path").as_string()));
        const auto b = read_rgbwsv_tiff(right / PathFromUtf8(pl[z].at("path").as_string()));
        for (int y = 0; y < base.height_px; ++y)
        {
            const auto arow = a.pixels.begin() + static_cast<std::ptrdiff_t>(y) * base.width_px * 6;
            const auto brow = b.pixels.begin() + static_cast<std::ptrdiff_t>(y) * padded.width_px * 6;
            Require(std::all_of(brow, brow + n * 6, [](auto v) { return v == 255; }), "TIFF prefix not empty");
            Require(std::equal(arow, arow + base.width_px * 6, brow + n * 6), "TIFF body byte drift");
        }
    }
    std::cout << "XPAD_PACKAGE_PASS layers=" << base.layer_count << " oldWidth=" << base.width_px
        << " newWidth=" << padded.width_px << " columns=" << n << " originX=" << px << '\n';
}
}
void RunXPaddingProduction(bool real, int dpi, double thickness, bool benchmark)
{
    const auto source = std::filesystem::path(SLICESOFT_SOURCE_DIR);
    const auto root = source / "output/xpad" / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(root / "off");
    std::filesystem::create_directories(root / "on");
    std::vector<std::filesystem::path> paths;
    if (real)
    {
        for (const auto& entry : std::filesystem::directory_iterator(source / "model/obj/alg_suoguo/20260908-HuangChenC"))
            if (entry.path().extension() == ".obj") paths.push_back(entry.path());
        std::sort(paths.begin(), paths.end());
        Require(paths.size() == 10, "real matrix requires all ten assets");
    }
    else paths.push_back(source / "samples/models/textured/fixtures/policy_textured_small.obj");
    const auto off = root / "off/profile.json";
    const auto on = root / "on/profile.json";
    Write(off, Profile(source, paths.front(), false, dpi, thickness));
    Write(on, Profile(source, paths.front(), true, dpi, thickness));
    Require(!load_slice_config(off).output.scene_pad_to_origin_x && load_slice_config(on).output.scene_pad_to_origin_x,
        "config roundtrip");
    auto invalid = Profile(source, paths.front(), false, dpi, thickness).as_object();
    auto invalidOutput = invalid.at("output").as_object();
    invalidOutput["scenePadToOriginX"] = "true";
    invalid["output"] = Json{invalidOutput};
    Write(root / "invalid.json", Json{invalid});
    bool rejected = false;
    try { (void)load_slice_config(root / "invalid.json"); } catch (const std::exception&) { rejected = true; }
    Require(rejected, "non-boolean option must be rejected");
    const auto scene = Scene(load_slice_config(off), paths);
    Write(root / "scene.json", SerializeMultiModelScene(scene));
    std::cout << "XPAD_EVIDENCE=" << PathToUtf8(root) << std::endl;
    Json::Array timings;
    for (int round = 0; round < (benchmark ? 4 : 1); ++round)
    {
        for (int turn = 0; turn < 2; ++turn)
        {
            const bool pad = (turn + round) % 2 != 0;
            const auto runRoot = benchmark ? root / ("r" + std::to_string(round) + (pad ? "on" : "off"))
                : root / (pad ? "on" : "off");
            std::filesystem::create_directories(runRoot);
            auto timing = RunPackage(runRoot, pad ? on : off, scene).as_object();
            timing["pad"] = pad;
            timing["round"] = round;
            timing["warmup"] = benchmark && round == 0;
            timings.push_back(Json{timing});
        }
        const auto pair = benchmark ? root / ("r" + std::to_string(round)) : root;
        ComparePackages(benchmark ? std::filesystem::path(pair.string() + "off") / "package" : root / "off/package",
            benchmark ? std::filesystem::path(pair.string() + "on") / "package" : root / "on/package");
        Write(root / "timings.json", Json{timings});
    }
}

void RunXPaddingTransfer(bool real, bool benchmark)
{
    CheckXPaddingTransferHelper();
    const auto source = std::filesystem::path(SLICESOFT_SOURCE_DIR);
    const auto root = source / "output/xpad" / ("t" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    auto model = source / "model/obj/reality/finger_suoguo/03.obj";
    if (!real)
    {
        model = root / "box.obj";
        std::ofstream obj(model);
        obj << "mtllib box.mtl\nusemtl transfer\n"
            "v 0 0 0\nv 2 0 0\nv 2 1 0\nv 0 1 0\n"
            "v 0 0 0.6\nv 2 0 0.6\nv 2 1 0.6\nv 0 1 0.6\n"
            "f 1 3 2\nf 1 4 3\nf 5 6 7\nf 5 7 8\n"
            "f 1 2 6\nf 1 6 5\nf 2 3 7\nf 2 7 6\n"
            "f 3 4 8\nf 3 8 7\nf 4 1 5\nf 4 5 8\n";
        Require(obj.good(), "T fixture write failed");
        std::ofstream mtl(root / "box.mtl");
        mtl << "newmtl transfer\nKd 1 0.8627450980392157 0.7764705882352941\n";
        Require(mtl.good(), "T MTL write failed");
    }
    Json::Array timings;
    for (int round = 0; round < (benchmark ? 4 : 1); ++round)
    {
        const auto pair = benchmark ? root / ("r" + std::to_string(round)) : root;
        for (int turn = 0; turn < 2; ++turn)
        {
            const bool pad = (turn + round) % 2 != 0;
            const auto run = pair / (pad ? "on" : "off");
            std::filesystem::create_directories(run);
            const auto path = run / "profile.json";
            Write(path, MakeXPaddingTransferProfile(source, model, run / "package", pad));
            const auto scene = Scene(load_slice_config(path), {model});
            auto timing = RunPackage(run, path, scene).as_object();
            timing["pad"] = pad;
            timing["round"] = round;
            timing["warmup"] = benchmark && round == 0;
            timings.push_back(Json{timing});
        }
        CompareXPaddingTransfer(pair / "off/package", pair / "on/package");
        Write(root / "timings.json", Json{timings});
    }
    std::cout << "XPAD_T_EVIDENCE=" << PathToUtf8(root) << std::endl;
}
