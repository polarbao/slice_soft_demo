#include "HostRequestBuilder.h"
#include "slicer_core/model/FrameGeometry.h"
#include "slicer_core/materials/transfer/TransferMaterialVolumePlan.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/slicer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace slicer_core;
void Require(const bool value, const char* message)
{
    if (!value) { throw std::runtime_error(message); }
}

std::filesystem::path WriteProfile(const std::filesystem::path& model,
    const std::filesystem::path& work, const bool multilayer, const int dpi,
    const double layerThickness)
{
    const std::string modelPath = model.generic_string();
    const std::string packagePath = (work / "package").generic_string();
    hosteffectiveprofilesettings settings{};
    settings.modelpath = modelPath.c_str();
    settings.modelformat = "obj";
    settings.packagedirectory = packagePath.c_str();
    settings.profileid = multilayer ? "frame-multilayer-transparent" : "frame-rgb-white-ondemand";
    settings.dpix = dpi;
    settings.dpiy = dpi;
    settings.layerthicknessmm = layerThickness;
    settings.materialstrategy = HOST_MATERIAL_RGB_SOLID;
    settings.varnishtoplayers = 1;
    settings.textureenabled = 1;
    settings.texturetopsurfacelayers = 1;
    settings.textureapplymode = HOST_TEXTURE_SOLID_VOLUME_FROM_TOP;
    settings.texturesampler = HOST_TEXTURE_BILINEAR;
    settings.textureflipv = 1;
    settings.texturewhitepolicy = HOST_TEXTURE_WHITE_UNDERBASE;
    settings.supportenabled = 1;
    settings.supportmode = HOST_SUPPORT_BOTTOM_PROJECTION;
    settings.internalvoidenabled = 1;
    settings.internalvoidminareapx = 16;
    settings.baseprojectionlayercount = 30;
    settings.geometrysamplingstrategy = HOST_GEOMETRY_SAMPLING_LEGACY_CENTER;
    settings.materialvolumeenabled = multilayer ? 1 : 0;
    settings.materialvolumeoverlapauto = multilayer ? 1 : 0;
    settings.materialvolumeopacityvarnishenabled = multilayer ? 1 : 0;
    settings.materialvolumeopacityvarnishmax = 0.001;
    settings.geometrydegenerateareaepsilonmm2 = 1e-24;
    char hash[128]{};
    const std::unique_ptr<char, decltype(&std::free)> profile(
        HostBuildEffectiveProfile(&settings, hash, sizeof(hash)), &std::free);
    Require(profile != nullptr, "host profile emission failed");
    std::filesystem::create_directories(work);
    const auto path = work / "profile.json";
    std::ofstream output(path);
    output << profile.get();
    Require(output.good(), "profile write failed");
    return path;
}

BoundingBox PrintableBounds(const ModelReport& model)
{
    BoundingBox bounds{model.triangles.at(0).a, model.triangles.at(0).a};
    for (const auto& triangle : model.triangles)
    {
        for (const auto& point : {triangle.a, triangle.b, triangle.c})
        {
            IncludeFramePointXY(bounds, point);
            bounds.min.z = std::min(bounds.min.z, point.z);
            bounds.max.z = std::max(bounds.max.z, point.z);
        }
    }
    return bounds;
}

void TransferIgnoresFrame(const ModelReport& model)
{
    TransferChannelPolicyConfig policy;
    policy.enabled = true;
    policy.material_diffuse_rgb_values = {{250U, 250U, 250U}};
    MaterialInfo locator;
    locator.name = "nail-Default";
    locator.has_diffuse = true;
    locator.diffuse_rgb = {250U, 250U, 250U};
    const std::vector<MaterialInfo> before{locator};
    Require(ResolveTransferMaterial(policy, before).present, "T control must match locator colour");
    const auto mesh = AdaptSceneModelToTriangleMesh(model);
    MaterialVolumeGrid grid;
    grid.widthPx = 2;
    grid.heightPx = 2;
    grid.layerCount = 3;
    grid.pixelSizeXMm = grid.pixelSizeYMm = grid.layerThicknessMm = 1.0;
    const auto plan = BuildTransferMaterialVolumePlan(policy, mesh, grid);
    Require(!plan.HasRegion() && !plan.material.present, "locator must never be a T material");
    std::vector<std::uint8_t> modelMask(4U, 1U), mask(4U, 9U);
    std::vector<std::uint32_t> owners(4U, 0U);
    for (int layer = 0; layer < 3; ++layer)
    {
        MaterializeTransferLayerMask(plan, layer, modelMask, owners, mask);
        Require(std::all_of(mask.begin(), mask.end(), [](const auto value) { return value == 0U; }),
            "locator colour must produce empty T mask on every layer");
    }
}

void MaterialBoundsKeepClosedEdges(const std::filesystem::path& asset)
{
    ModelLoadConfig config;
    config.input.model_path = asset;
    config.auto_orient.enabled = false;
    const auto model = load_model_report(config, {});
    const auto mesh = AdaptSceneModelToTriangleMesh(model);
    MaterialVolumePolicyConfig policy;
    policy.enabled = true;
    policy.overlap.rules.push_back({"nail-L1", 100});
    MaterialVolumeBuildRequest request;
    request.mesh = &mesh;
    request.policy = &policy;
    request.grid = {5, 7, 0.5, 1.5, 1.0, 1.0, 1.0, 18};
    const auto plan = BuildMaterialVolumePlan(request);
    std::vector<std::uint8_t> mask(35U, 1U);
    std::vector<std::uint32_t> owner(35U);
    for (int layer = 0; layer < 18; ++layer)
    {
        MaterializeMaterialOwnershipLayer(plan, layer, mask, owner);
        for (int y = 0; y < 7; ++y)
        {
            for (int x = 0; x < 5; ++x)
            {
                const bool inside = x >= 1 && x <= 3 && y >= 1 && y <= 5
                    && layer >= 5 && layer <= 16;
                Require((owner[static_cast<std::size_t>(y) * 5U + x] != kNoMaterialOwner) == inside,
                    "material broad phase changed closed-edge or blank-column ownership");
            }
        }
    }
}

void Run(const std::filesystem::path& asset, const std::filesystem::path& work,
    const bool multilayer, const bool real, const int dpi, const double thickness)
{
    const auto profile = WriteProfile(asset, work, multilayer, dpi, thickness);
    const auto config = load_slice_config(profile);
    const auto model = load_model_report(config, profile.parent_path());
    Require(!model.frame_vertices.empty(), "test requires registration geometry");
    if (!real) { TransferIgnoresFrame(model); }
    const auto body = PrintableBounds(model);
    SliceRunRasterGrid grid;
    std::vector<std::vector<std::uint8_t>> baseline;
    std::uint64_t rgb = 0U, white = 0U, support = 0U, varnish = 0U, blankFrame = 0U;
    SliceRunOptions options;
    options.write_preview_files = false;
    options.write_tiff_layers = real;
    options.write_reports = real;
    options.gridcallback = [&](const auto& value)
    {
        grid = value;
        Require(std::abs(grid.originxmm - model.bbox_mm.min.x) < 1e-8
            && std::abs(grid.originymm - model.bbox_mm.min.y) < 1e-8,
            "frame raster origin was lost");
        Require(grid.widthpx == static_cast<int>(std::ceil(
            (model.bbox_mm.max.x - model.bbox_mm.min.x) / grid.pixelsizexmm))
            && grid.heightpx == static_cast<int>(std::ceil(
            (model.bbox_mm.max.y - model.bbox_mm.min.y) / grid.pixelsizeymm)),
            "frame raster extent was cropped");
    };
    options.layercallback = [&](const auto& layer, const auto&)
    {
        if (!real) { baseline.push_back(layer.channels); }
        for (int y = 0; y < grid.heightpx; ++y)
        {
            for (int x = 0; x < grid.widthpx; ++x)
            {
                const auto base = (static_cast<std::size_t>(y) * grid.widthpx + x) * 6U;
                rgb += layer.channels[base] != 255U || layer.channels[base + 1U] != 255U
                    || layer.channels[base + 2U] != 255U;
                white += layer.channels[base + 3U] != 255U;
                support += layer.channels[base + 4U] != 255U;
                varnish += layer.channels[base + 5U] != 255U;
                const double px = grid.originxmm + (x + 0.5) * grid.pixelsizexmm;
                const double py = grid.originymm + (y + 0.5) * grid.pixelsizeymm;
                // A one-pixel guard excludes legitimate boundary rasterization.
                if (px < body.min.x - grid.pixelsizexmm || px > body.max.x + grid.pixelsizexmm
                    || py < body.min.y - grid.pixelsizeymm || py > body.max.y + grid.pixelsizeymm)
                {
                    for (std::size_t channel = 0U; channel < 6U; ++channel)
                    {
                        Require(layer.channels[base + channel] == 255U,
                            "locator-only area contains printing material or support");
                    }
                    ++blankFrame;
                }
            }
        }
    };
    const auto start = std::chrono::steady_clock::now();
    const auto result = run_slicer(profile, options);
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    Require(rgb > 0U && blankFrame > 0U, "printing body and empty frame must both be present");
    if (real)
    {
        const auto validation = validate_slice_package(result.package_dir);
        Require(validation.layer_count == result.layer_count, "RIP layer count mismatch");
        Require(varnish > 0U, "real transparent materials must reach V");
    }
    else
    {
        // Independent asset without frame, assigned the same output canvas.
        auto controlConfig = config;
        controlConfig.input.model_path = asset.parent_path() / "printable.obj";
        auto control = load_model_report(controlConfig, {});
        control.bbox_mm.min.x = model.bbox_mm.min.x;
        control.bbox_mm.min.y = model.bbox_mm.min.y;
        control.bbox_mm.max.x = model.bbox_mm.max.x;
        control.bbox_mm.max.y = model.bbox_mm.max.y;
        options.modelreportoverride = &control;
        std::size_t index = 0U;
        options.layercallback = [&](const auto& layer, const auto&)
        {
            Require(index < baseline.size() && layer.channels == baseline[index++],
                "frame changed printable RGBWSV/support pixels");
        };
        const auto reference = run_slicer(profile, options);
        Require(index == baseline.size() && reference.layer_count == result.layer_count,
            "frame changed printing layer count");
    }
    std::cout << "PASS " << (multilayer ? "multilayer_transparent" : "rgb_white_ondemand")
        << " grid=" << result.width_px << 'x' << result.height_px
        << " layers=" << result.layer_count << " seconds=" << elapsed
        << " RGB=" << rgb << " W=" << white << " S=" << support << " V=" << varnish
        << " empty_frame_pixel_layers=" << blankFrame << " package=" << result.package_dir << '\n';
}
}

int main(const int argc, char** argv)
{
    try
    {
        const auto root = std::filesystem::path(SLICESOFT_SOURCE_DIR);
        const auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        const bool real = argc > 1 && std::string(argv[1]) == "--real";
        // Keep the established output directory name; do not shorten it to out.
        const auto work = root / "output" / "frame" / stamp;
        if (real)
        {
            Run(root / "model/obj/multi-material/gubao05/gubao05-dingwei.obj", work,
                true, true, argc > 2 ? std::stoi(argv[2]) : 600,
                argc > 3 ? std::stod(argv[3]) : 0.033);
        }
        else
        {
            MaterialBoundsKeepClosedEdges(root / "tests/fixtures/frame_geometry/printable.obj");
            for (const bool multilayer : {false, true})
            {
                Run(root / "tests/fixtures/frame_geometry/with_frame.obj",
                    work / (multilayer ? "multi" : "legacy"), multilayer, false, 127, 0.5);
            }
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL frame production: " << error.what() << '\n';
        return 1;
    }
}
