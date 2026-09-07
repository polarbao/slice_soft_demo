#include "slicer_core/config.h"
#include "slicer_core/geometry/TransformedModelAdapter.h"
#include "slicer_core/model.h"
#include "slicer_core/pipeline/MultiModelProductionService.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/scene/SceneEffectiveConfig.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
using namespace slicer_core;

void Require(const bool condition, const std::string& message)
{
    if (!condition) { throw std::runtime_error(message); }
}

bool Near(const double lhs, const double rhs)
{
    return std::abs(lhs - rhs) < 1.0e-8;
}

Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    Require(static_cast<bool>(input), "cannot read JSON: " + path.generic_string());
    return Json::parse(input);
}

std::string ReadBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    Require(static_cast<bool>(input), "cannot read source: " + path.generic_string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::filesystem::path WriteProfile(const std::filesystem::path& root,
                                   const std::filesystem::path& modelPath)
{
    const auto source = std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "samples/configs/golden/material_process_top2_fixture.json";
    auto document = ReadJson(source).as_object();
    auto input = document.at("input").as_object();
    input["modelPath"] = modelPath.generic_string();
    input["format"] = "obj";
    document["input"] = Json{std::move(input)};
    auto output = document.at("output").as_object();
    output["packageDir"] = (root / "output").generic_string();
    output["dpiX"] = 127;
    output["dpiY"] = 127;
    output["layerThicknessMm"] = 1.0;
    document["output"] = Json{std::move(output)};
    auto orientation = document.at("autoOrient").as_object();
    orientation["enabled"] = false;
    document["autoOrient"] = Json{std::move(orientation)};
    const auto path = root / "profile.json";
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    Require(static_cast<bool>(stream), "cannot write profile");
    stream << Json{std::move(document)}.dump(2) << '\n';
    stream.close();
    Require(static_cast<bool>(stream), "profile write failed");
    return path;
}

struct FixtureScene
{
    MultiModelScene scene;
    std::array<BoundingBox, 2> printablebounds;
};

BoundingBox PrintingBounds(const TransformedModelGeometry& geometry)
{
    BoundingBox bounds{geometry.triangles.at(0).a, geometry.triangles.at(0).a};
    for (const auto& triangle : geometry.triangles)
    {
        for (const auto& point : {triangle.a, triangle.b, triangle.c})
        {
            bounds.min.x = std::min(bounds.min.x, point.x);
            bounds.min.y = std::min(bounds.min.y, point.y);
            bounds.max.x = std::max(bounds.max.x, point.x);
            bounds.max.y = std::max(bounds.max.y, point.y);
        }
    }
    return bounds;
}

FixtureScene MakeScene(const std::filesystem::path& profilePath)
{
    const auto profile = load_slice_config(profilePath);
    const auto model = load_model_report(profile, profilePath.parent_path());
    Require(model.frame_triangle_count == 2U, "scene source separates locator faces");
    FixtureScene fixture;
    auto& scene = fixture.scene;
    scene.sceneid = "frame-scene";
    scene.scenerevision = 1U;
    scene.resolvedprofileid = profile.material_process_profile.name;
    scene.buildvolume.source = BuildVolumeSource::Fixture;
    scene.buildvolume.widthmm = 340.0;
    scene.buildvolume.heightmm = 240.0;
    scene.buildvolume.zlimitmm = 20.0;
    scene.buildvolume.origin = BuildVolumeOrigin::LowerLeft;
    scene.buildvolume.xdirection = BuildVolumeAxisDirection::Positive;
    scene.buildvolume.ydirection = BuildVolumeAxisDirection::Positive;
    scene.buildvolume.isfixture = true;
    ResourceScope scope;
    scope.resourcescopeid = "frame-scope";
    scope.kind = ResourceScopeKind::ObjDirectory;
    scope.rootpath = model.model_path.parent_path();
    scene.resourcescopes.push_back(scope);
    ModelSource source;
    source.modelid = "frame-model";
    source.sourcepath = model.model_path;
    source.format = "obj";
    source.autoorientenabled = false;
    source.resourcescopeid = scope.resourcescopeid;
    source.sourcehash = ComputeSha256(ReadBytes(model.model_path));
    source.resourcehash = ComputeSceneResourceHash(model);
    source.displayname = "frame fixture";
    scene.models.push_back(source);
    for (std::size_t i = 0U; i < 2U; ++i)
    {
        SceneModelInstance item;
        item.instance.instanceid = "frame-instance-" + std::to_string(i);
        item.instance.modelid = source.modelid;
        item.instance.sourcetransformidentity = model.model_path.generic_string();
        item.instance.sourcebboxmm = model.bbox_mm;
        item.instance.transform.translatexmm = i == 0U ? 120.0 : 294.0;
        item.instance.transform.translateymm = i == 0U ? 40.0 : 120.0;
        item.instance.transform.rotatezdeg = i == 0U ? 0.0 : 90.0;
        item.instance.transform.landonbuildplate = true;
        const auto transformed = AdaptTransformedModel(model, item.instance);
        Require(transformed.IsValid(), "fixture instance transforms");
        item.instance.effectivebboxmm = transformed.geometry.bboxmm;
        fixture.printablebounds[i] = PrintingBounds(transformed.geometry);
        item.requestedtransform = item.instance.transform;
        item.effectivetransform = item.instance.transform;
        item.admissionstatus = SceneInstanceAdmissionStatus::Admitted;
        item.resolvedprofileid = scene.resolvedprofileid;
        scene.instances.push_back(std::move(item));
    }
    return fixture;
}

bool ContainsXY(const BoundingBox& bounds, const double x, const double y)
{
    constexpr double tolerance = 1.0e-8;
    return x >= bounds.min.x - tolerance && x <= bounds.max.x + tolerance
        && y >= bounds.min.y - tolerance && y <= bounds.max.y + tolerance;
}

void ValidatePixels(const std::filesystem::path& package,
                    const std::array<BoundingBox, 2>& printableBounds)
{
    const auto strict = validate_slice_package(package);
    Require(strict.width_px == 1570 && strict.height_px == 1100,
            "scene canvas retains translated and rotated locator XY bounds");
    Require(strict.layer_count == 12, "scene physical layers ignore extreme locator Z");
    Require(strict.dpi_x == 127 && strict.dpi_y == 127, "low-cost scene sampling retained");
    const auto manifest = ReadJson(package / "manifest.json");
    const auto& origin = manifest.at("grid").at("originMm").as_array();
    const double originX = origin.at(0).as_double();
    const double originY = origin.at(1).as_double();
    Require(Near(originX, 10.0) && Near(originY, 10.0), "global origin includes frame not printable hull");
    std::array<std::uint64_t, 2> printed{};
    std::uint64_t emptyCanvasPixels = 0U;
    const auto& layers = manifest.at("layers").as_array();
    Require(layers.size() == 12U, "manifest layer list excludes locator height");
    for (const auto& layer : layers)
    {
        const auto image = read_rgbwsv_tiff(package / layer.at("path").as_string());
        for (std::uint32_t row = 0U; row < image.spec.height; ++row)
        {
            for (std::uint32_t column = 0U; column < image.spec.width; ++column)
            {
                const double x = originX + (static_cast<double>(column) + 0.5) * strict.pixel_size_x_mm;
                const double y = originY + (static_cast<double>(row) + 0.5) * strict.pixel_size_y_mm;
                const bool first = ContainsXY(printableBounds[0], x, y);
                const bool second = ContainsXY(printableBounds[1], x, y);
                const std::size_t offset = (static_cast<std::size_t>(row) * image.spec.width + column) * 6U;
                bool anyPrint = false;
                for (std::size_t channel = 0U; channel < 6U; ++channel)
                { anyPrint = anyPrint || image.pixels.at(offset + channel) != 255U; }
                if (!first && !second)
                {
                    Require(!anyPrint, "nonprinting scene canvas must be empty in all RGBWSV channels");
                    ++emptyCanvasPixels;
                }
                if (first && anyPrint) { ++printed[0]; }
                if (second && anyPrint) { ++printed[1]; }
            }
        }
    }
    Require(printed[0] > 0U && printed[1] > 0U, "both scene instances retain real printing pixels");
    Require(emptyCanvasPixels > 800000U, "large locator-defined empty canvas preserved across layers");
}

void Run(const std::filesystem::path& root)
{
    const auto modelPath = std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "tests/fixtures/frame_geometry/with_frame.obj";
    const auto profile = WriteProfile(root, modelPath);
    const auto fixture = MakeScene(profile);
    SceneEffectiveConfigRequest effectiveRequest;
    effectiveRequest.scene = fixture.scene;
    effectiveRequest.sourcescenepath = root / "scene_config.draft.json";
    effectiveRequest.generatedconfigpath = root / "scene_config.effective.json";
    effectiveRequest.sourceprofileid = fixture.scene.resolvedprofileid;
    effectiveRequest.sourceprofileconfigpath = profile;
    effectiveRequest.outputpackagedir = root / "output";
    effectiveRequest.generatedatutc = "2026-09-07T00:00:00.000Z";
    effectiveRequest.dpix = 127;
    effectiveRequest.dpiy = 127;
    effectiveRequest.layerheightmm = 1.0;
    const auto effective = WriteSceneEffectiveConfig(effectiveRequest);
    Require(effective.IsValid(), effective.error ? effective.error->message : "effective config failed");
    MultiModelProductionRequest request;
    request.effectiveconfigpath = effectiveRequest.generatedconfigpath;
    request.jobid = "frame-scene-test";
    request.attemptid = "frame-scene-attempt";
    const auto result = RunMultiModelProductionService(request);
    Require(result.IsValid(), result.error ? result.error->message : "scene package failed");
    Require(result.visibleinstancecount == 2U && result.layercount == 12, "scene result instance/layer count");
    ValidatePixels(result.packagedir, fixture.printablebounds);
}
}

int main()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path()
        / ("slicesoft_frame_scene_" + std::to_string(stamp));
    try
    {
        std::filesystem::create_directories(root);
        Run(root);
        std::filesystem::remove_all(root);
        std::cout << "PASS two-instance frame scene canvas, empty pixels and strict RIP\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << "\nArtifacts: " << root.generic_string() << '\n';
        return 1;
    }
}
