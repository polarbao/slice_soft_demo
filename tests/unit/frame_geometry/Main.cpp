#include "slicer_core/geometry/ContactLevelingAnalyzer.h"
#include "slicer_core/geometry/TransformedModelAdapter.h"
#include "slicer_core/materials/volume/MaterialLayerNameResolver.h"
#include "slicer_core/model/FrameGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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

bool Equal(const Vec3& lhs, const Vec3& rhs)
{
    return Near(lhs.x, rhs.x) && Near(lhs.y, rhs.y) && Near(lhs.z, rhs.z);
}

std::filesystem::path Fixture(const char* filename)
{
    return std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "tests/fixtures/frame_geometry" / filename;
}

ModelReport Load(const char* filename, const bool autoOrient)
{
    ModelLoadConfig config;
    config.input.model_path = Fixture(filename);
    config.auto_orient.enabled = autoOrient;
    return load_model_report(config, {});
}

Vec3 Rotate(Vec3 point, const double x, const double y, const double z)
{
    const auto radians = [](const double degrees)
    { return degrees * std::numbers::pi / 180.0; };
    const double rx = radians(x), ry = radians(y), rz = radians(z);
    point = {point.x, point.y * std::cos(rx) - point.z * std::sin(rx),
             point.y * std::sin(rx) + point.z * std::cos(rx)};
    point = {point.x * std::cos(ry) + point.z * std::sin(ry), point.y,
             -point.x * std::sin(ry) + point.z * std::cos(ry)};
    return {point.x * std::cos(rz) - point.y * std::sin(rz),
            point.x * std::sin(rz) + point.y * std::cos(rz), point.z};
}

void Include(BoundingBox& bounds, const Vec3& point, const bool includeZ)
{
    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
    if (includeZ)
    {
        bounds.min.z = std::min(bounds.min.z, point.z);
        bounds.max.z = std::max(bounds.max.z, point.z);
    }
}

void VerifyBounds(const std::vector<Triangle>& triangles,
                  const std::vector<Vec3>& frame, const BoundingBox& actual)
{
    BoundingBox expected{triangles.at(0).a, triangles.at(0).a};
    for (const auto& triangle : triangles)
    {
        Include(expected, triangle.a, true);
        Include(expected, triangle.b, true);
        Include(expected, triangle.c, true);
    }
    for (const auto& point : frame) { Include(expected, point, false); }
    Require(Equal(actual.min, expected.min) && Equal(actual.max, expected.max),
            "XY includes locator points but Z includes printable triangles only");
}

void ComparePrinting(const ModelReport& plain, const ModelReport& framed)
{
    Require(plain.triangles.size() == framed.triangles.size(), "print triangle count");
    Require(plain.triangle_textures.size() == framed.triangle_textures.size(), "UV count");
    for (std::size_t i = 0; i < plain.triangles.size(); ++i)
    {
        const auto& lhs = plain.triangles[i];
        const auto& rhs = framed.triangles[i];
        Require(Equal(lhs.a, rhs.a) && Equal(lhs.b, rhs.b) && Equal(lhs.c, rhs.c),
                "interleaved frame faces preserve printable triangle order and coordinates");
        const auto& lt = plain.triangle_textures[i];
        const auto& rt = framed.triangle_textures[i];
        Require(lt.has_uv == rt.has_uv && lt.material_name == rt.material_name,
                "printable material binding preserved");
        for (std::size_t uv = 0; uv < 3; ++uv)
        {
            Require(Near(lt.uv[uv].u, rt.uv[uv].u) && Near(lt.uv[uv].v, rt.uv[uv].v),
                    "printable UV preserved");
        }
    }
    Require(plain.material_infos.size() == framed.material_infos.size(), "material table count");
    Require(framed.material_infos.size() == 1U, "reserved material declaration removed");
    const auto& lhs = plain.material_infos.at(0);
    const auto& rhs = framed.material_infos.at(0);
    Require(lhs.name == rhs.name && lhs.diffuse_rgb == rhs.diffuse_rgb
            && lhs.has_diffuse == rhs.has_diffuse && lhs.has_opacity == rhs.has_opacity
            && Near(lhs.opacity, rhs.opacity) && lhs.has_texture == rhs.has_texture
            && lhs.diffuse_texture_path == rhs.diffuse_texture_path,
            "printable material definition preserved");
    Require(plain.auto_orient.selected_orientation == framed.auto_orient.selected_orientation
            && plain.auto_orient.rotation_deg == framed.auto_orient.rotation_deg,
            "extreme locator coordinates cannot change auto orientation");
    Require(Near(plain.bbox_mm.min.z, framed.bbox_mm.min.z)
            && Near(plain.bbox_mm.max.z, framed.bbox_mm.max.z), "printable Z unchanged");
}

void ImportSeparation()
{
    for (const bool enabled : {false, true})
    {
        const auto plain = Load("printable.obj", enabled);
        const auto framed = Load("with_frame.obj", enabled);
        ComparePrinting(plain, framed);
        Require(plain.frame_vertices.empty() && plain.frame_triangle_count == 0U,
                "unreferenced reserved declaration does not create geometry");
        Require(framed.frame_vertices.size() == 6U && framed.frame_triangle_count == 2U,
                "locator faces separated and audited");
        Require(framed.triangle_count == 12U && framed.face_count == 13U
                && framed.vertex_count == 14U && framed.faces_with_uv == 11U
                && framed.faces_without_uv == 2U,
                "printable triangle count separated from original vertex/face/UV statistics");
        Require(ResolveMaterialLayerNaming(framed.material_infos).violations.empty(),
                "filtered model passes strict layer naming");
        VerifyBounds(framed.triangles, framed.frame_vertices, framed.bbox_mm);
        if (enabled) { Require(Near(framed.bbox_mm.min.z, 0.0), "auto orientation grounds printable Z"); }
        else
        {
            Require(Equal(framed.bbox_mm.min, {-110.0, -30.0, 5.0})
                    && Equal(framed.bbox_mm.max, {110.0, 30.0, 17.0}), "source frame bounds");
        }
        ModelLoadConfig config;
        config.auto_orient.enabled = enabled;
        config.transform.scale = {1.2, 0.8, 1.7};
        config.transform.rotation_deg = {20.0, 30.0, -15.0};
        config.transform.translation_mm = {12.0, -7.0, 5.0};
        config.input.model_path = Fixture("printable.obj");
        const auto transformedPlain = load_model_report(config, {});
        config.input.model_path = Fixture("with_frame.obj");
        const auto transformedFrame = load_model_report(config, {});
        ComparePrinting(transformedPlain, transformedFrame);
        VerifyBounds(transformedFrame.triangles, transformedFrame.frame_vertices, transformedFrame.bbox_mm);
    }
}

Vec3 InstancePoint(Vec3 point, const Vec3& pivot, const ModelTransform& transform)
{
    point = {(point.x - pivot.x) * transform.uniformscale,
             (point.y - pivot.y) * transform.uniformscale,
             (point.z - pivot.z) * transform.uniformscale};
    if (transform.mirrorx) { point.x = -point.x; }
    if (transform.mirrory) { point.y = -point.y; }
    point = Rotate(point, transform.rotatexdeg, transform.rotateydeg, transform.rotatezdeg);
    return {point.x + pivot.x + transform.translatexmm,
            point.y + pivot.y + transform.translateymm,
            point.z + pivot.z + transform.translatezmm};
}

void InstanceTransform()
{
    const auto source = Load("with_frame.obj", false);
    for (const bool landing : {false, true})
    {
        ModelInstance instance;
        instance.instanceid = "frame-instance";
        instance.modelid = "frame-model";
        instance.sourcetransformidentity = "frame-source";
        instance.sourcebboxmm = source.bbox_mm;
        instance.effectivebboxmm = source.bbox_mm;
        auto& transform = instance.transform;
        transform.uniformscale = 1.5;
        transform.mirrorx = true;
        transform.rotatexdeg = 30.0;
        transform.rotateydeg = -20.0;
        transform.rotatezdeg = 70.0;
        transform.translatexmm = 13.0;
        transform.translateymm = -11.0;
        transform.translatezmm = 27.0;
        transform.landonbuildplate = landing;
        const auto result = AdaptTransformedModel(source, instance);
        Require(result.IsValid(), "instance transformation succeeds");
        const auto& geometry = result.geometry;
        const Vec3 expectedPivot{0.0, 0.0, 5.0};
        Require(Equal(geometry.pivotmm, expectedPivot), "pivot retains frame XY center and printable minZ");
        double minZ = std::numeric_limits<double>::max();
        for (const auto& triangle : source.triangles)
        {
            for (const auto point : {triangle.a, triangle.b, triangle.c})
            { minZ = std::min(minZ, InstancePoint(point, expectedPivot, transform).z); }
        }
        const double landingOffset = landing ? -minZ : 0.0;
        Require(Near(geometry.landingoffsetzmm, landingOffset), "landing ignores extreme frame Z");
        Require(geometry.framevertices.size() == source.frame_vertices.size(), "frame vertex count retained");
        for (std::size_t i = 0; i < source.frame_vertices.size(); ++i)
        {
            auto expected = InstancePoint(source.frame_vertices[i], expectedPivot, transform);
            expected.z += landingOffset;
            Require(Equal(geometry.framevertices[i], expected), "frame uses shared instance transform and landing");
        }
        for (std::size_t i = 0; i < source.triangles.size(); ++i)
        {
            auto expected = InstancePoint(source.triangles[i].a, expectedPivot, transform);
            expected.z += landingOffset;
            Require(Equal(geometry.triangles[i].a, expected), "print geometry uses same transform");
        }
        VerifyBounds(geometry.triangles, geometry.framevertices, geometry.bboxmm);
        if (landing) { Require(Near(geometry.bboxmm.min.z, 0.0), "landed print minZ is zero"); }
    }
}

void FailClosedNames()
{
    Require(IsFrameMaterial("nail-Default"), "exact reserved name recognized");
    for (const std::string name : {"Default", "nail-default", "NAIL-Default", "nail-Default-L1", "nail-Default "})
    {
        Require(!IsFrameMaterial(name), "near name must not be exempt: " + name);
        std::vector<Vec3> vertices{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        std::vector<std::array<std::size_t, 3>> faces{{0U, 1U, 2U}};
        std::vector<TriangleTextureInfo> textures(1U);
        textures[0].material_name = name;
        const auto frame = ExtractFrameGeometry(vertices, faces, textures);
        Require(frame.vertices.empty() && faces.size() == 1U && vertices.size() == 3U,
                "nonreserved geometry remains printable: " + name);
    }
    const auto ordinary = Load("default.obj", false);
    Require(ordinary.frame_vertices.empty() && ordinary.triangles.size() == 1U,
            "Default face is not silently discarded");
    Require(!ResolveMaterialLayerNaming(ordinary.material_infos).violations.empty(),
            "ordinary Default still fails strict multilayer naming");
    bool rejected = false;
    try { static_cast<void>(Load("frame_only.obj", false)); }
    catch (const std::exception& error)
    {
        rejected = std::string_view(error.what()).find("E_FRAME_NO_PRINTABLE_GEOMETRY") != std::string_view::npos;
    }
    Require(rejected, "frame-only asset fails with explicit diagnostic");
}

void InvalidFrameMetadata()
{
    std::vector<Vec3> vertices{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    std::vector<std::array<std::size_t, 3>> faces{{0U, 1U, 2U}};
    std::vector<TriangleTextureInfo> textures(2U);
    textures[0].material_name = "nail-Default";
    bool rejected = false;
    try { static_cast<void>(ExtractFrameGeometry(vertices, faces, textures)); }
    catch (const std::exception& error)
    {
        rejected = std::string_view(error.what()).find("E_FRAME_BINDING_INVALID") != std::string_view::npos;
    }
    Require(rejected, "misaligned frame binding fails closed");
    for (const double invalid : {std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::quiet_NaN()})
    {
        for (const std::size_t axis : {0U, 1U, 2U})
        {
            auto model = Load("printable.obj", false);
            ImportedFrameGeometry frame;
            frame.printableanchor = model.triangles.at(0).a;
            frame.vertices = {{-110.0, -30.0, -1000.0}};
            if (axis == 0U) { frame.vertices[0].x = invalid; }
            else if (axis == 1U) { frame.vertices[0].y = invalid; }
            else { frame.vertices[0].z = invalid; }
            rejected = false;
            try { static_cast<void>(AttachFrameGeometry(model, frame)); }
            catch (const std::exception& error)
            {
                rejected = std::string_view(error.what()).find("E_FRAME_GEOMETRY_NONFINITE") != std::string_view::npos;
            }
            Require(rejected, "nonfinite frame coordinates fail attachment");
        }
    }
}

void ContactLeveling()
{
    const auto source = Load("with_frame.obj", false);
    const double angle = 13.0;
    const double centerX = (source.bbox_mm.min.x + source.bbox_mm.max.x) / 2.0;
    const auto rotate = [centerX, angle](const Vec3& point)
    {
        auto result = Rotate({point.x - centerX, point.y, point.z}, 0.0, -angle, 0.0);
        result.x += centerX;
        return result;
    };
    double minZ = std::numeric_limits<double>::max();
    for (const auto& triangle : source.triangles)
    {
        for (const auto& point : {triangle.a, triangle.b, triangle.c})
        { minZ = std::min(minZ, rotate(point).z); }
    }
    const auto result = ApplyContactLevelingAngle(source, angle);
    Require(result.frame_vertices.size() == source.frame_vertices.size(), "contact leveling retains frame");
    for (std::size_t i = 0; i < source.frame_vertices.size(); ++i)
    {
        auto expected = rotate(source.frame_vertices[i]);
        expected.z -= minZ;
        Require(Equal(result.frame_vertices[i], expected), "contact leveling frame rotates and grounds with print");
    }
    for (std::size_t i = 0; i < source.triangles.size(); ++i)
    {
        auto expected = rotate(source.triangles[i].a);
        expected.z -= minZ;
        Require(Equal(result.triangles[i].a, expected), "contact leveling print reference");
    }
    VerifyBounds(result.triangles, result.frame_vertices, result.bbox_mm);
    Require(Near(result.bbox_mm.min.z, 0.0), "contact leveling grounds printable geometry only");
}

void RealAsset(const std::filesystem::path& path)
{
    ModelLoadConfig config;
    config.input.model_path = path;
    config.auto_orient.enabled = false;
    const auto model = load_model_report(config, {});
    Require(model.frame_triangle_count == 36U && model.triangle_count == 126134U,
            "gubao05 printable/frame triangle split");
    Require(ResolveMaterialLayerNaming(model.material_infos).violations.empty(), "real asset naming passes");
    VerifyBounds(model.triangles, model.frame_vertices, model.bbox_mm);
    std::cout << "REAL printable_triangles=" << model.triangle_count
              << " frame_triangles=" << model.frame_triangle_count
              << " frame_vertices=" << model.frame_vertices.size()
              << " bbox_min=" << model.bbox_mm.min.x << ',' << model.bbox_mm.min.y << ',' << model.bbox_mm.min.z
              << " bbox_max=" << model.bbox_mm.max.x << ',' << model.bbox_mm.max.y << ',' << model.bbox_mm.max.z << '\n';
}
}

int main(const int argc, char** argv)
{
    try
    {
        ImportSeparation();
        InstanceTransform();
        FailClosedNames();
        InvalidFrameMetadata();
        ContactLeveling();
        if (argc >= 2 && std::string_view(argv[1]) == "--real")
        {
            const auto path = argc >= 3 ? std::filesystem::path(argv[2])
                : std::filesystem::path(SLICESOFT_SOURCE_DIR)
                    / "model/obj/multi-material/gubao05/gubao05-dingwei.obj";
            RealAsset(path);
        }
        std::cout << "PASS frame geometry isolation, naming and transform\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
