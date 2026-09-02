#include "slicer_core/model.h"

#include "slicer_core/model/ObjFaceParser.h"
#include "slicer_core/model/MtlMaterialParser.h"

#include "miniz.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <set>
#include <vector>

namespace slicer_core {
namespace {

constexpr double pi{3.14159265358979323846};

struct MeshData {
    std::vector<Vec3> vertices;
    std::vector<TexCoord> texcoords;
    std::size_t normal_count{0};
    bool has_normals{false};
    std::vector<std::array<std::size_t, 3>> faces;
    std::vector<TriangleTextureInfo> triangle_textures;
    std::size_t raw_face_count{0};
    std::size_t faces_with_uv{0};
    std::size_t faces_without_uv{0};
    std::string stl_encoding;
    std::vector<std::string> material_libraries;
    std::vector<MaterialStat> materials;
    std::vector<MaterialInfo> material_infos;
    ThreeMfReportInfo three_mf;
};

struct ZipEntryData {
    std::string name;
    std::vector<std::uint8_t> data;
    std::uint16_t compression_method{0};
    std::uint64_t compressed_size{0};
    std::uint64_t uncompressed_size{0};
};

struct ThreeMfMaterial {
    std::string key;
    std::string name;
    std::array<std::uint8_t, 3> rgb{0, 0, 0};
    bool has_rgb{false};
};

struct ThreeMfTriangle {
    std::array<std::size_t, 3> vertices{};
    std::string material_key;
    bool has_uv{false};
    std::array<TexCoord, 3> uv{};
};

struct ThreeMfObject {
    std::string id;
    bool is_mesh{false};
    bool is_components{false};
    std::vector<Vec3> vertices;
    std::vector<ThreeMfTriangle> triangles;
    std::vector<std::pair<std::string, std::array<double, 12>>> components;
};

struct ThreeMfColorGroup {
    std::string id;
    std::vector<std::array<std::uint8_t, 3>> colors;
};

struct ThreeMfTexture2D {
    std::string id;
    std::string path;
    std::string content_type;
    std::filesystem::path extracted_path;
    bool loaded{false};
};

struct ThreeMfTexture2DGroup {
    std::string id;
    std::string texid;
    std::vector<TexCoord> coords;
};

struct ThreeMfXmlReader {
    explicit ThreeMfXmlReader(std::string xml) : xml_text{std::move(xml)} {
        validate_restricted_xml(xml_text);
    }

    std::map<std::string, std::string> root_attributes() const;
    std::vector<std::string> tags(const std::string& tag_name) const;
    std::string block(const std::string& tag_name, std::size_t start_pos = 0) const;
    std::vector<std::string> blocks(const std::string& tag_name) const;
    std::vector<std::string> tags_in(const std::string& block_text, const std::string& tag_name) const;
    std::string block_in(const std::string& block_text, const std::string& tag_name, std::size_t start_pos = 0) const;

private:
    static void validate_restricted_xml(const std::string& xml);

    std::string xml_text;
};

struct OrientationCandidate {
    std::string name;
    std::array<double, 3> rotation_deg;
    std::vector<Vec3> vertices;
    BoundingBox bbox;
};

double unit_scale_to_mm(const std::string& unit) {
    if (unit == "mm") {
        return 1.0;
    }
    if (unit == "cm") {
        return 10.0;
    }
    if (unit == "m") {
        return 1000.0;
    }
    if (unit == "inch" || unit == "in") {
        return 25.4;
    }
    throw std::runtime_error("unsupported modelTransform.unit: " + unit);
}

Vec3 rotate_xyz(const Vec3& point, const std::array<double, 3>& rotation_deg) {
    Vec3 result{point};
    const double rx{rotation_deg.at(0) * pi / 180.0};
    const double ry{rotation_deg.at(1) * pi / 180.0};
    const double rz{rotation_deg.at(2) * pi / 180.0};

    const double cx{std::cos(rx)};
    const double sx{std::sin(rx)};
    result = {result.x, result.y * cx - result.z * sx, result.y * sx + result.z * cx};

    const double cy{std::cos(ry)};
    const double sy{std::sin(ry)};
    result = {result.x * cy + result.z * sy, result.y, -result.x * sy + result.z * cy};

    const double cz{std::cos(rz)};
    const double sz{std::sin(rz)};
    result = {result.x * cz - result.y * sz, result.x * sz + result.y * cz, result.z};
    return result;
}

Vec3 apply_transform(const Vec3& input, const TransformConfig& transform) {
    const double unit_scale{unit_scale_to_mm(transform.unit)};
    Vec3 result{
        input.x * unit_scale * transform.scale.at(0),
        input.y * unit_scale * transform.scale.at(1),
        input.z * unit_scale * transform.scale.at(2)};
    result = rotate_xyz(result, transform.rotation_deg);
    result.x += transform.translation_mm.at(0);
    result.y += transform.translation_mm.at(1);
    result.z += transform.translation_mm.at(2);
    return result;
}

double bbox_height(const BoundingBox& bbox) {
    return bbox.max.z - bbox.min.z;
}

double bbox_width(const BoundingBox& bbox) {
    return bbox.max.x - bbox.min.x;
}

double bbox_depth(const BoundingBox& bbox) {
    return bbox.max.y - bbox.min.y;
}

double bbox_footprint_area(const BoundingBox& bbox) {
    return bbox_width(bbox) * bbox_depth(bbox);
}

BoundingBox compute_bbox(const std::vector<Vec3>& vertices) {
    if (vertices.empty()) {
        throw std::runtime_error("cannot compute bbox for empty vertex list");
    }
    BoundingBox bbox{
        {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()},
        {std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()}};
    for (const Vec3& point : vertices) {
        bbox.min.x = std::min(bbox.min.x, point.x);
        bbox.min.y = std::min(bbox.min.y, point.y);
        bbox.min.z = std::min(bbox.min.z, point.z);
        bbox.max.x = std::max(bbox.max.x, point.x);
        bbox.max.y = std::max(bbox.max.y, point.y);
        bbox.max.z = std::max(bbox.max.z, point.z);
    }
    return bbox;
}

std::vector<Vec3> rotate_points(const std::vector<Vec3>& vertices, const std::array<double, 3>& rotation_deg) {
    std::vector<Vec3> result;
    result.reserve(vertices.size());
    for (const Vec3& point : vertices) {
        result.push_back(rotate_xyz(point, rotation_deg));
    }
    return result;
}

std::vector<Vec3> normalize_to_build_plate(const std::vector<Vec3>& vertices, const BoundingBox& bbox) {
    std::vector<Vec3> result;
    result.reserve(vertices.size());
    for (const Vec3& point : vertices) {
        result.push_back({point.x, point.y, point.z - bbox.min.z});
    }
    return result;
}

std::vector<Triangle> build_triangles(
    const std::vector<Vec3>& vertices,
    const std::vector<std::array<std::size_t, 3>>& faces) {
    std::vector<Triangle> triangles;
    triangles.reserve(faces.size());
    for (const auto& face : faces) {
        triangles.push_back({vertices.at(face.at(0)), vertices.at(face.at(1)), vertices.at(face.at(2))});
    }
    return triangles;
}

double triangle_area_squared(const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
    const Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
    const Vec3 cross{
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x};
    return 0.25 * (cross.x * cross.x + cross.y * cross.y + cross.z * cross.z);
}

std::size_t count_degenerate_triangles(
    const std::vector<Vec3>& vertices,
    const std::vector<std::array<std::size_t, 3>>& faces) {
    std::size_t count{0};
    for (const auto& face : faces) {
        if (triangle_area_squared(vertices.at(face.at(0)), vertices.at(face.at(1)), vertices.at(face.at(2))) <= 1.0e-18) {
            ++count;
        }
    }
    return count;
}

OrientationCandidate make_orientation_candidate(
    const std::string& name,
    const std::array<double, 3>& rotation_deg,
    const std::vector<Vec3>& source_vertices) {
    std::vector<Vec3> rotated = rotate_points(source_vertices, rotation_deg);
    BoundingBox rotated_bbox = compute_bbox(rotated);
    rotated = normalize_to_build_plate(rotated, rotated_bbox);
    rotated_bbox = compute_bbox(rotated);
    return {name, rotation_deg, std::move(rotated), rotated_bbox};
}

double TransverseSpanInEndBand(
    const OrientationCandidate& candidate,
    const bool longAxisIsX,
    const bool lowEnd)
{
    constexpr double kEndBandFraction{0.12};
    const double longitudinalMinimum =
        longAxisIsX ? candidate.bbox.min.x : candidate.bbox.min.y;
    const double longitudinalMaximum =
        longAxisIsX ? candidate.bbox.max.x : candidate.bbox.max.y;
    const double longitudinalSpan =
        longitudinalMaximum - longitudinalMinimum;
    const double bandBoundary = lowEnd
        ? longitudinalMinimum
            + longitudinalSpan * kEndBandFraction
        : longitudinalMaximum
            - longitudinalSpan * kEndBandFraction;
    double transverseMinimum{
        std::numeric_limits<double>::max()};
    double transverseMaximum{
        std::numeric_limits<double>::lowest()};
    std::size_t count{0U};
    for (const Vec3& point : candidate.vertices)
    {
        const double longitudinalCoordinate =
            longAxisIsX ? point.x : point.y;
        const double transverseCoordinate =
            longAxisIsX ? point.y : point.x;
        const bool inBand = lowEnd
            ? longitudinalCoordinate <= bandBoundary
            : longitudinalCoordinate >= bandBoundary;
        if (!inBand)
        {
            continue;
        }
        transverseMinimum =
            std::min(transverseMinimum, transverseCoordinate);
        transverseMaximum =
            std::max(transverseMaximum, transverseCoordinate);
        ++count;
    }
    if (count < 2U)
    {
        return longAxisIsX
            ? bbox_depth(candidate.bbox)
            : bbox_width(candidate.bbox);
    }
    return transverseMaximum - transverseMinimum;
}

OrientationCandidate AlignPlanarHeading(
    const OrientationCandidate& candidate,
    const std::vector<Vec3>& sourceVertices)
{
    constexpr double kPlanarToleranceMm{1.0e-9};
    const double width = bbox_width(candidate.bbox);
    const double depth = bbox_depth(candidate.bbox);
    const double height = bbox_height(candidate.bbox);
    if (height
        >= std::min(width, depth) - kPlanarToleranceMm)
    {
        return candidate;
    }

    const bool longAxisIsX =
        width > depth + kPlanarToleranceMm;
    const double lowEndSpan =
        TransverseSpanInEndBand(
            candidate,
            longAxisIsX,
            true);
    const double highEndSpan =
        TransverseSpanInEndBand(
            candidate,
            longAxisIsX,
            false);
    const bool lowEndIsTip =
        lowEndSpan <= highEndSpan + kPlanarToleranceMm;
    const double rotationZ = longAxisIsX
        ? (lowEndIsTip ? -90.0 : 90.0)
        : (lowEndIsTip ? 180.0 : 0.0);
    if (std::abs(rotationZ) <= kPlanarToleranceMm)
    {
        return candidate;
    }

    std::array<double, 3> rotation = candidate.rotation_deg;
    rotation.at(2U) += rotationZ;
    std::string suffix;
    if (rotationZ == -90.0)
    {
        suffix = "_rotate_z_minus_90";
    }
    else if (rotationZ == 90.0)
    {
        suffix = "_rotate_z_90";
    }
    else
    {
        suffix = "_rotate_z_180";
    }
    return make_orientation_candidate(
        candidate.name + suffix,
        rotation,
        sourceVertices);
}

bool IsPlanarNailCandidate(
    const OrientationCandidate& candidate)
{
    constexpr double kMinimumAspectRatio{1.5};
    constexpr double kMaximumThicknessRatio{0.75};
    constexpr double kSpanToleranceMm{1.0e-9};
    const double width = bbox_width(candidate.bbox);
    const double depth = bbox_depth(candidate.bbox);
    const double height = bbox_height(candidate.bbox);
    const double longSpan = std::max(width, depth);
    const double shortSpan = std::min(width, depth);
    if (shortSpan <= kSpanToleranceMm)
    {
        return false;
    }
    return longSpan / shortSpan >= kMinimumAspectRatio
        && height / shortSpan <= kMaximumThicknessRatio;
}

OrientationCandidate AlignNailFrontUp(
    const OrientationCandidate& candidate,
    const std::vector<Vec3>& sourceVertices)
{
    constexpr double kSideBandFraction{0.125};
    constexpr double kCenterHalfBandFraction{0.125};
    constexpr double kMinimumEnvelopeDeltaMm{0.05};
    constexpr double kRelativeEnvelopeDelta{0.05};
    if (!IsPlanarNailCandidate(candidate))
    {
        return candidate;
    }

    const double width = bbox_width(candidate.bbox);
    const double depth = bbox_depth(candidate.bbox);
    const bool longAxisIsX = width >= depth;
    const double transverseMinimum =
        longAxisIsX ? candidate.bbox.min.y : candidate.bbox.min.x;
    const double transverseMaximum =
        longAxisIsX ? candidate.bbox.max.y : candidate.bbox.max.x;
    const double transverseSpan =
        transverseMaximum - transverseMinimum;
    const double transverseCenter =
        0.5 * (transverseMinimum + transverseMaximum);
    const double sideBandWidth =
        transverseSpan * kSideBandFraction;
    const double centerHalfBandWidth =
        transverseSpan * kCenterHalfBandFraction;
    double centerLowerEnvelope{
        std::numeric_limits<double>::max()};
    double sideLowerEnvelope{
        std::numeric_limits<double>::max()};
    std::size_t centerSampleCount{0U};
    std::size_t sideSampleCount{0U};
    for (const Vec3& point : candidate.vertices)
    {
        const double transverseCoordinate =
            longAxisIsX ? point.y : point.x;
        if (std::abs(
                transverseCoordinate - transverseCenter)
            <= centerHalfBandWidth)
        {
            centerLowerEnvelope =
                std::min(centerLowerEnvelope, point.z);
            ++centerSampleCount;
        }
        if (transverseCoordinate
                <= transverseMinimum + sideBandWidth
            || transverseCoordinate
                >= transverseMaximum - sideBandWidth)
        {
            sideLowerEnvelope =
                std::min(sideLowerEnvelope, point.z);
            ++sideSampleCount;
        }
    }
    if (centerSampleCount == 0U || sideSampleCount == 0U)
    {
        return candidate;
    }

    const double requiredEnvelopeDelta =
        std::max(
            kMinimumEnvelopeDeltaMm,
            bbox_height(candidate.bbox)
                * kRelativeEnvelopeDelta);
    if (centerLowerEnvelope + requiredEnvelopeDelta
        >= sideLowerEnvelope)
    {
        return candidate;
    }

    std::array<double, 3> rotation =
        candidate.rotation_deg;
    const std::size_t longAxisRotationIndex =
        longAxisIsX ? 0U : 1U;
    rotation.at(longAxisRotationIndex) += 180.0;
    return make_orientation_candidate(
        candidate.name
            + (longAxisIsX
                   ? "_rotate_x_180"
                   : "_rotate_y_180"),
        rotation,
        sourceVertices);
}

OrientationCandidate choose_auto_orientation(
    const std::vector<Vec3>& vertices,
    const BoundingBox& original_bbox,
    const AutoOrientConfig& config)
{
    constexpr double kHeightToleranceMm{1.0e-9};
    constexpr double kFootprintToleranceSquareMm{1.0e-9};
    const OrientationCandidate sourceIdentity{
        "identity",
        {0.0, 0.0, 0.0},
        vertices,
        original_bbox};
    if (!config.enabled)
    {
        return sourceIdentity;
    }

    const OrientationCandidate groundedIdentity =
        make_orientation_candidate(
            "identity",
            {0.0, 0.0, 0.0},
            vertices);
    if (bbox_height(original_bbox)
        <= config.max_height_mm + kHeightToleranceMm)
    {
        return AlignPlanarHeading(
            AlignNailFrontUp(
                groundedIdentity,
                vertices),
            vertices);
    }

    std::vector<OrientationCandidate> candidates;
    candidates.push_back(groundedIdentity);
    candidates.push_back(make_orientation_candidate("rotate_x_90", {90.0, 0.0, 0.0}, vertices));
    candidates.push_back(make_orientation_candidate("rotate_x_minus_90", {-90.0, 0.0, 0.0}, vertices));
    candidates.push_back(make_orientation_candidate("rotate_y_90", {0.0, 90.0, 0.0}, vertices));
    candidates.push_back(make_orientation_candidate("rotate_y_minus_90", {0.0, -90.0, 0.0}, vertices));

    const auto isSignificantlyLess =
        [](const double left,
           const double right,
           const double tolerance)
    {
        return left < right - tolerance;
    };
    const auto isBetter =
        [&](const OrientationCandidate& candidate,
            const OrientationCandidate& selected)
    {
        const double candidateHeight =
            bbox_height(candidate.bbox);
        const double selectedHeight =
            bbox_height(selected.bbox);
        const double candidateFootprint =
            bbox_footprint_area(candidate.bbox);
        const double selectedFootprint =
            bbox_footprint_area(selected.bbox);
        const bool candidateFits =
            candidateHeight
            <= config.max_height_mm + kHeightToleranceMm;
        const bool selectedFits =
            selectedHeight
            <= config.max_height_mm + kHeightToleranceMm;
        if (candidateFits != selectedFits)
        {
            return candidateFits;
        }
        if (candidateFits)
        {
            if (isSignificantlyLess(
                    candidateFootprint,
                    selectedFootprint,
                    kFootprintToleranceSquareMm))
            {
                return true;
            }
            if (isSignificantlyLess(
                    selectedFootprint,
                    candidateFootprint,
                    kFootprintToleranceSquareMm))
            {
                return false;
            }
            return isSignificantlyLess(
                candidateHeight,
                selectedHeight,
                kHeightToleranceMm);
        }
        if (isSignificantlyLess(
                candidateHeight,
                selectedHeight,
                kHeightToleranceMm))
        {
            return true;
        }
        if (isSignificantlyLess(
                selectedHeight,
                candidateHeight,
                kHeightToleranceMm))
        {
            return false;
        }
        return isSignificantlyLess(
            candidateFootprint,
            selectedFootprint,
            kFootprintToleranceSquareMm);
    };

    std::size_t selectedIndex{0};
    for (std::size_t index{1};
         index < candidates.size();
         ++index)
    {
        if (isBetter(
                candidates.at(index),
                candidates.at(selectedIndex)))
        {
            selectedIndex = index;
        }
    }
    return AlignPlanarHeading(
        AlignNailFrontUp(
            candidates.at(selectedIndex),
            vertices),
        vertices);
}

std::filesystem::path resolve_path(const std::filesystem::path& path, const std::filesystem::path& config_dir) {
    if (path.is_absolute()) {
        return path;
    }
    const std::filesystem::path from_config = config_dir / path;
    if (std::filesystem::exists(from_config)) {
        return from_config;
    }
    return std::filesystem::current_path() / path;
}

std::string detect_format(const std::filesystem::path& path, std::string configured_format) {
    std::transform(configured_format.begin(), configured_format.end(), configured_format.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (configured_format != "auto") {
        return configured_format;
    }
    std::string extension{path.extension().string()};
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (extension == ".stl") {
        return "stl";
    }
    if (extension == ".obj") {
        return "obj";
    }
    if (extension == ".3mf") {
        return "3mf";
    }
    throw std::runtime_error("cannot infer model format from extension: " + path.string());
}

void load_ascii_stl(const std::filesystem::path& path, const TransformConfig& transform, MeshData& mesh) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("failed to open STL model: " + path.string());
    }

    std::vector<std::size_t> facet_vertices;
    facet_vertices.reserve(3);
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream stream{line};
        std::string token;
        stream >> token;
        if (token == "facet")
        {
            std::string normalToken;
            Vec3 normal;
            if (stream >> normalToken >> normal.x >> normal.y >> normal.z
                && normalToken == "normal")
            {
                mesh.has_normals = true;
            }
            continue;
        }
        if (token != "vertex") {
            continue;
        }
        Vec3 vertex{};
        if (!(stream >> vertex.x >> vertex.y >> vertex.z)) {
            throw std::runtime_error("invalid STL vertex line in: " + path.string());
        }
        mesh.vertices.push_back(apply_transform(vertex, transform));
        facet_vertices.push_back(mesh.vertices.size() - 1U);
        if (facet_vertices.size() == 3) {
            mesh.faces.push_back({facet_vertices.at(0), facet_vertices.at(1), facet_vertices.at(2)});
            ++mesh.raw_face_count;
            facet_vertices.clear();
        }
    }
    mesh.stl_encoding = "ascii";
}

std::uint32_t read_u32_le(std::istream& input) {
    std::array<unsigned char, 4> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input) {
        throw std::runtime_error("truncated binary STL while reading uint32");
    }
    return static_cast<std::uint32_t>(bytes.at(0)) | (static_cast<std::uint32_t>(bytes.at(1)) << 8U)
        | (static_cast<std::uint32_t>(bytes.at(2)) << 16U) | (static_cast<std::uint32_t>(bytes.at(3)) << 24U);
}

std::uint16_t read_u16_le(std::istream& input) {
    std::array<unsigned char, 2> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input) {
        throw std::runtime_error("truncated binary STL while reading uint16");
    }
    return static_cast<std::uint16_t>(bytes.at(0)) | static_cast<std::uint16_t>(bytes.at(1) << 8U);
}

float read_f32_le(std::istream& input) {
    const std::uint32_t bits{read_u32_le(input)};
    float value{0.0F};
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool looks_like_binary_stl(const std::filesystem::path& path) {
    const std::uintmax_t size = std::filesystem::file_size(path);
    if (size < 84U) {
        return false;
    }
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("failed to open STL model: " + path.string());
    }
    input.seekg(80, std::ios::beg);
    const std::uint32_t triangle_count{read_u32_le(input)};
    return size == 84ULL + static_cast<std::uintmax_t>(triangle_count) * 50ULL;
}

void load_binary_stl(const std::filesystem::path& path, const TransformConfig& transform, MeshData& mesh) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("failed to open STL model: " + path.string());
    }
    input.seekg(80, std::ios::beg);
    const std::uint32_t triangle_count{read_u32_le(input)};
    mesh.has_normals = triangle_count > 0U;
    mesh.vertices.reserve(static_cast<std::size_t>(triangle_count) * 3U);
    mesh.faces.reserve(triangle_count);
    for (std::uint32_t i{0}; i < triangle_count; ++i) {
        (void)read_f32_le(input);
        (void)read_f32_le(input);
        (void)read_f32_le(input);
        std::array<std::size_t, 3> face{};
        for (std::size_t vertex_index{0}; vertex_index < 3U; ++vertex_index) {
            Vec3 vertex{read_f32_le(input), read_f32_le(input), read_f32_le(input)};
            mesh.vertices.push_back(apply_transform(vertex, transform));
            face.at(vertex_index) = mesh.vertices.size() - 1U;
        }
        (void)read_u16_le(input);
        mesh.faces.push_back(face);
        ++mesh.raw_face_count;
    }
    mesh.stl_encoding = "binary";
}

void load_stl(const std::filesystem::path& path, const TransformConfig& transform, MeshData& mesh) {
    if (looks_like_binary_stl(path)) {
        load_binary_stl(path, transform, mesh);
        return;
    }
    load_ascii_stl(path, transform, mesh);
}

std::uint16_t read_u16_le_at(const std::vector<std::uint8_t>& data, const std::size_t offset) {
    if (offset + 2U > data.size()) {
        throw std::runtime_error("truncated ZIP while reading uint16");
    }
    return static_cast<std::uint16_t>(data.at(offset) | (static_cast<std::uint16_t>(data.at(offset + 1U)) << 8U));
}

std::uint32_t read_u32_le_at(const std::vector<std::uint8_t>& data, const std::size_t offset) {
    if (offset + 4U > data.size()) {
        throw std::runtime_error("truncated ZIP while reading uint32");
    }
    return static_cast<std::uint32_t>(data.at(offset))
        | (static_cast<std::uint32_t>(data.at(offset + 1U)) << 8U)
        | (static_cast<std::uint32_t>(data.at(offset + 2U)) << 16U)
        | (static_cast<std::uint32_t>(data.at(offset + 3U)) << 24U);
}

std::vector<ZipEntryData> read_3mf_zip_entries(const std::filesystem::path& path, ThreeMfReportInfo& report) {
    constexpr std::size_t max_entry_count{256};
    constexpr std::uint64_t max_total_uncompressed{128ULL * 1024ULL * 1024ULL};
    std::vector<ZipEntryData> entries;

    mz_zip_archive archive{};
    if (!mz_zip_reader_init_file(&archive, path.string().c_str(), 0)) {
        const std::string error = mz_zip_get_error_string(mz_zip_get_last_error(&archive));
        throw std::runtime_error("E_3MF_ZIP_OPEN_FAILED: " + error + ": " + path.string());
    }

    const auto close_archive = [&archive]() {
        mz_zip_reader_end(&archive);
    };

    const mz_uint file_count = mz_zip_reader_get_num_files(&archive);
    if (file_count > max_entry_count) {
        close_archive();
        throw std::runtime_error("E_3MF_ZIP_TOO_MANY_ENTRIES: 3MF package has too many ZIP entries");
    }

    std::uint64_t total_uncompressed{0};
    for (mz_uint file_index{0}; file_index < file_count; ++file_index) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive, file_index, &stat)) {
            const std::string error = mz_zip_get_error_string(mz_zip_get_last_error(&archive));
            close_archive();
            throw std::runtime_error("E_3MF_ZIP_OPEN_FAILED: failed to stat ZIP entry: " + error);
        }
        if (stat.m_is_directory) {
            continue;
        }
        std::string name{stat.m_filename};
        std::replace(name.begin(), name.end(), '\\', '/');
        if (name.find("..") != std::string::npos || (!name.empty() && name.front() == '/')) {
            close_archive();
            throw std::runtime_error("E_3MF_ZIP_PATH_TRAVERSAL: 3MF ZIP entry path traversal is not allowed: " + name);
        }
        if (stat.m_method != 0 && stat.m_method != 8) {
            close_archive();
            throw std::runtime_error("E_3MF_ZIP_UNSUPPORTED_COMPRESSION: unsupported ZIP compression method for " + name);
        }
        total_uncompressed += stat.m_uncomp_size;
        if (total_uncompressed > max_total_uncompressed) {
            close_archive();
            throw std::runtime_error("E_3MF_ZIP_TOO_LARGE: 3MF package exceeds maximum uncompressed size");
        }
        std::vector<std::uint8_t> data(static_cast<std::size_t>(stat.m_uncomp_size));
        if (!mz_zip_reader_extract_to_mem(&archive, file_index, data.data(), data.size(), 0)) {
            const std::string error = mz_zip_get_error_string(mz_zip_get_last_error(&archive));
            close_archive();
            throw std::runtime_error("E_3MF_ZIP_OPEN_FAILED: failed to extract ZIP entry " + name + ": " + error);
        }
        if (stat.m_method == 0) {
            ++report.stored_entry_count;
        } else if (stat.m_method == 8) {
            ++report.deflated_entry_count;
        }
        entries.push_back({name, data, stat.m_method, stat.m_comp_size, stat.m_uncomp_size});
    }
    close_archive();
    report.entry_count = static_cast<int>(entries.size());
    report.total_uncompressed_bytes = total_uncompressed;
    return entries;
}

std::string entry_text(const std::map<std::string, std::vector<std::uint8_t>>& entries, const std::string& name) {
    const auto found = entries.find(name);
    if (found == entries.end()) {
        return {};
    }
    return std::string(found->second.begin(), found->second.end());
}

std::string xml_unescape(std::string value) {
    const std::array<std::pair<const char*, const char*>, 5> replacements{{
        {"&quot;", "\""},
        {"&apos;", "'"},
        {"&lt;", "<"},
        {"&gt;", ">"},
        {"&amp;", "&"},
    }};
    for (const auto& replacement : replacements) {
        std::size_t pos{0};
        while ((pos = value.find(replacement.first, pos)) != std::string::npos) {
            value.replace(pos, std::strlen(replacement.first), replacement.second);
            pos += std::strlen(replacement.second);
        }
    }
    return value;
}

std::map<std::string, std::string> xml_attributes(const std::string& tag) {
    std::map<std::string, std::string> attributes;
    std::size_t pos{0};
    while (pos < tag.size()) {
        while (pos < tag.size() && (std::isalnum(static_cast<unsigned char>(tag.at(pos))) == 0
                                    && tag.at(pos) != '_' && tag.at(pos) != ':')) {
            ++pos;
        }
        const std::size_t key_start = pos;
        while (pos < tag.size() && (std::isalnum(static_cast<unsigned char>(tag.at(pos))) != 0
                                    || tag.at(pos) == '_' || tag.at(pos) == ':' || tag.at(pos) == '-')) {
            ++pos;
        }
        if (key_start == pos) {
            continue;
        }
        const std::string key = tag.substr(key_start, pos - key_start);
        while (pos < tag.size() && std::isspace(static_cast<unsigned char>(tag.at(pos))) != 0) {
            ++pos;
        }
        if (pos >= tag.size() || tag.at(pos) != '=') {
            continue;
        }
        ++pos;
        while (pos < tag.size() && std::isspace(static_cast<unsigned char>(tag.at(pos))) != 0) {
            ++pos;
        }
        if (pos >= tag.size() || (tag.at(pos) != '"' && tag.at(pos) != '\'')) {
            continue;
        }
        const char quote = tag.at(pos++);
        const std::size_t value_start = pos;
        while (pos < tag.size() && tag.at(pos) != quote) {
            ++pos;
        }
        if (pos >= tag.size()) {
            break;
        }
        attributes[key] = xml_unescape(tag.substr(value_start, pos - value_start));
        ++pos;
    }
    return attributes;
}

std::string xml_tag_name(const std::string& tag);
std::string xml_local_name(const std::string& name);
bool xml_tag_matches_local_name(const std::string& tag, const std::string& tag_name);
std::size_t find_xml_tag_start(const std::string& xml, const std::string& tag_name, std::size_t start_pos);

std::vector<std::string> find_xml_tags(const std::string& xml, const std::string& tag_name) {
    std::vector<std::string> tags;
    std::size_t pos{0};
    while ((pos = find_xml_tag_start(xml, tag_name, pos)) != std::string::npos) {
        const std::size_t end = xml.find('>', pos);
        if (end == std::string::npos) {
            break;
        }
        tags.push_back(xml.substr(pos, end - pos + 1U));
        pos = end + 1U;
    }
    return tags;
}

std::string find_xml_block(const std::string& xml, const std::string& tag_name, const std::size_t start_pos = 0) {
    const std::size_t start = find_xml_tag_start(xml, tag_name, start_pos);
    if (start == std::string::npos) {
        return {};
    }
    const std::size_t open_end = xml.find('>', start);
    if (open_end == std::string::npos) {
        return {};
    }
    const std::string start_tag = xml.substr(start, open_end - start + 1U);
    if (start_tag.size() >= 2U && start_tag.at(start_tag.size() - 2U) == '/') {
        return start_tag;
    }
    int depth{1};
    std::size_t pos = open_end + 1U;
    while ((pos = xml.find('<', pos)) != std::string::npos) {
        const std::size_t end = xml.find('>', pos);
        if (end == std::string::npos) {
            return {};
        }
        const std::string tag = xml.substr(pos, end - pos + 1U);
        if (tag.size() >= 2U && (tag.at(1) == '!' || tag.at(1) == '?')) {
            pos = end + 1U;
            continue;
        }
        if (xml_tag_matches_local_name(tag, tag_name)) {
            const bool closing = tag.size() >= 2U && tag.at(1) == '/';
            const bool self_closing = tag.size() >= 2U && tag.at(tag.size() - 2U) == '/';
            if (closing) {
                --depth;
                if (depth == 0) {
                    return xml.substr(start, end - start + 1U);
                }
            } else if (!self_closing) {
                ++depth;
            }
        }
        pos = end + 1U;
    }
    return {};
}

std::vector<std::string> find_xml_blocks(const std::string& xml, const std::string& tag_name) {
    std::vector<std::string> blocks;
    std::size_t pos{0};
    while (true) {
        const std::string block = find_xml_block(xml, tag_name, pos);
        if (block.empty()) {
            break;
        }
        blocks.push_back(block);
        pos = xml.find(block, pos);
        if (pos == std::string::npos) {
            break;
        }
        pos += block.size();
    }
    return blocks;
}

std::string xml_tag_name(const std::string& tag) {
    std::size_t pos{1};
    if (pos < tag.size() && tag.at(pos) == '/') {
        ++pos;
    }
    while (pos < tag.size() && std::isspace(static_cast<unsigned char>(tag.at(pos))) != 0) {
        ++pos;
    }
    const std::size_t start = pos;
    while (pos < tag.size() && (std::isalnum(static_cast<unsigned char>(tag.at(pos))) != 0
                                || tag.at(pos) == '_' || tag.at(pos) == ':' || tag.at(pos) == '-')) {
        ++pos;
    }
    return tag.substr(start, pos - start);
}

std::string xml_local_name(const std::string& name) {
    const std::size_t colon = name.rfind(':');
    return colon == std::string::npos ? name : name.substr(colon + 1U);
}

bool xml_tag_matches_local_name(const std::string& tag, const std::string& tag_name) {
    return xml_local_name(xml_tag_name(tag)) == tag_name;
}

std::size_t find_xml_tag_start(const std::string& xml, const std::string& tag_name, std::size_t start_pos) {
    while ((start_pos = xml.find('<', start_pos)) != std::string::npos) {
        if (start_pos + 1U < xml.size() && (xml.at(start_pos + 1U) == '/' || xml.at(start_pos + 1U) == '!'
                                             || xml.at(start_pos + 1U) == '?')) {
            ++start_pos;
            continue;
        }
        const std::size_t end = xml.find('>', start_pos);
        if (end == std::string::npos) {
            return std::string::npos;
        }
        const std::string tag = xml.substr(start_pos, end - start_pos + 1U);
        if (xml_tag_matches_local_name(tag, tag_name)) {
            return start_pos;
        }
        start_pos = end + 1U;
    }
    return std::string::npos;
}

void ThreeMfXmlReader::validate_restricted_xml(const std::string& xml) {
    if (xml.find("<!DOCTYPE") != std::string::npos || xml.find("<!ENTITY") != std::string::npos) {
        throw std::runtime_error("E_3MF_XML_PARSE_FAILED: external DTD/entity declarations are not allowed");
    }

    std::vector<std::string> stack;
    std::size_t pos{0};
    while ((pos = xml.find('<', pos)) != std::string::npos) {
        if (xml.compare(pos, 4, "<!--") == 0) {
            const std::size_t end = xml.find("-->", pos + 4U);
            if (end == std::string::npos) {
                throw std::runtime_error("E_3MF_XML_PARSE_FAILED: unterminated XML comment");
            }
            pos = end + 3U;
            continue;
        }
        const std::size_t end = xml.find('>', pos + 1U);
        if (end == std::string::npos) {
            throw std::runtime_error("E_3MF_XML_PARSE_FAILED: unterminated XML tag");
        }
        const std::string tag = xml.substr(pos, end - pos + 1U);
        if (tag.size() >= 2U && (tag.at(1) == '?' || tag.at(1) == '!')) {
            pos = end + 1U;
            continue;
        }
        const bool closing = tag.size() >= 2U && tag.at(1) == '/';
        const bool self_closing = tag.size() >= 2U && tag.at(tag.size() - 2U) == '/';
        const std::string name = xml_local_name(xml_tag_name(tag));
        if (name.empty()) {
            throw std::runtime_error("E_3MF_XML_PARSE_FAILED: empty XML tag name");
        }
        if (closing) {
            if (stack.empty() || stack.back() != name) {
                throw std::runtime_error("E_3MF_XML_PARSE_FAILED: mismatched XML closing tag: " + name);
            }
            stack.pop_back();
        } else if (!self_closing) {
            stack.push_back(name);
        }
        pos = end + 1U;
    }
    if (!stack.empty()) {
        throw std::runtime_error("E_3MF_XML_PARSE_FAILED: unclosed XML tag: " + stack.back());
    }
}

std::map<std::string, std::string> ThreeMfXmlReader::root_attributes() const {
    const std::size_t model_pos = find_xml_tag_start(xml_text, "model", 0);
    if (model_pos == std::string::npos) {
        throw std::runtime_error("E_3MF_XML_PARSE_FAILED: 3MF model XML missing model root");
    }
    const std::size_t model_end = xml_text.find('>', model_pos);
    if (model_end == std::string::npos) {
        throw std::runtime_error("E_3MF_XML_PARSE_FAILED: malformed model root");
    }
    return xml_attributes(xml_text.substr(model_pos, model_end - model_pos + 1U));
}

std::vector<std::string> ThreeMfXmlReader::tags(const std::string& tag_name) const {
    return find_xml_tags(xml_text, tag_name);
}

std::string ThreeMfXmlReader::block(const std::string& tag_name, const std::size_t start_pos) const {
    return find_xml_block(xml_text, tag_name, start_pos);
}

std::vector<std::string> ThreeMfXmlReader::blocks(const std::string& tag_name) const {
    return find_xml_blocks(xml_text, tag_name);
}

std::vector<std::string> ThreeMfXmlReader::tags_in(const std::string& block_text, const std::string& tag_name) const {
    return find_xml_tags(block_text, tag_name);
}

std::string ThreeMfXmlReader::block_in(
    const std::string& block_text,
    const std::string& tag_name,
    const std::size_t start_pos) const {
    return find_xml_block(block_text, tag_name, start_pos);
}

std::string find_3mf_model_part(
    const std::map<std::string, std::vector<std::uint8_t>>& entries,
    ThreeMfReportInfo& report) {
    const std::string rels = entry_text(entries, "_rels/.rels");
    if (!rels.empty()) {
        const ThreeMfXmlReader rels_reader{rels};
        for (const std::string& tag : rels_reader.tags("Relationship")) {
            const auto attrs = xml_attributes(tag);
            const auto target = attrs.find("Target");
            if (target != attrs.end() && target->second.find(".model") != std::string::npos) {
                std::string normalized = target->second;
                std::replace(normalized.begin(), normalized.end(), '\\', '/');
                if (!normalized.empty() && normalized.front() == '/') {
                    normalized.erase(normalized.begin());
                }
                return normalized;
            }
        }
    } else {
        report.warnings.push_back("E_3MF_RELS_MISSING: _rels/.rels missing; fallback to 3D/3dmodel.model");
    }
    return "3D/3dmodel.model";
}

bool is_supported_three_mf_unit(const std::string& unit) {
    return unit == "micron" || unit == "millimeter" || unit == "mm" || unit.empty() || unit == "centimeter"
        || unit == "inch" || unit == "foot" || unit == "meter";
}

double three_mf_unit_scale_to_mm(const std::string& unit) {
    if (unit == "micron") {
        return 0.001;
    }
    if (unit == "millimeter" || unit == "mm" || unit.empty()) {
        return 1.0;
    }
    if (unit == "centimeter") {
        return 10.0;
    }
    if (unit == "inch") {
        return 25.4;
    }
    if (unit == "foot") {
        return 304.8;
    }
    if (unit == "meter") {
        return 1000.0;
    }
    return 1.0;
}

void collect_unsupported_3mf_metadata(const ThreeMfXmlReader& reader, ThreeMfReportInfo& report) {
    const auto attrs = reader.root_attributes();
    for (const auto& item : attrs) {
        if (item.first.rfind("xmlns:", 0) == 0) {
            const std::string extension = item.first.substr(6);
            if (extension != "xml") {
                report.unsupported_extensions.push_back(extension + "=" + item.second);
            }
        }
    }

    const std::string resources_block = reader.block("resources");
    if (resources_block.empty()) {
        return;
    }
    const std::array<std::string, 2> unsupported_tags{
        "compositematerials",
        "multiproperties"};
    for (const std::string& tag_name : unsupported_tags) {
        const std::vector<std::string> tags = reader.tags_in(resources_block, tag_name);
        for (std::size_t i{0}; i < tags.size(); ++i) {
            report.unsupported_resources.push_back(tag_name);
            ++report.ignored_resource_count;
        }
    }
}

std::string normalize_3mf_internal_path(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    while (!path.empty() && path.front() == '/') {
        path.erase(path.begin());
    }
    if (path.empty() || path.find("..") != std::string::npos) {
        throw std::runtime_error("E_3MF_ZIP_PATH_TRAVERSAL: invalid 3MF internal texture path: " + path);
    }
    return path;
}

std::filesystem::path safe_cache_file_name(const std::filesystem::path& cache_dir, const std::string& id, const std::string& path) {
    std::string name = id + "_" + std::filesystem::path(path).filename().string();
    for (char& ch : name) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '.' || ch == '_' || ch == '-')) {
            ch = '_';
        }
    }
    return cache_dir / name;
}

std::filesystem::path extract_3mf_internal_texture(
    const std::map<std::string, std::vector<std::uint8_t>>& entries,
    const std::string& texture_id,
    const std::string& raw_path,
    const std::filesystem::path& cache_dir,
    ThreeMfReportInfo& report) {
    const std::string normalized = normalize_3mf_internal_path(raw_path);
    const auto found = entries.find(normalized);
    if (found == entries.end()) {
        ++report.texture_missing_count;
        report.warnings.push_back("E_3MF_TEXTURE_PATH_MISSING: texture entry missing: " + normalized);
        return safe_cache_file_name(cache_dir, texture_id, normalized);
    }
    std::filesystem::create_directories(cache_dir);
    const std::filesystem::path output_path = safe_cache_file_name(cache_dir, texture_id, normalized);
    std::ofstream output{output_path, std::ios::binary};
    if (!output) {
        throw std::runtime_error("failed to write 3MF internal texture cache: " + output_path.string());
    }
    output.write(
        reinterpret_cast<const char*>(found->second.data()),
        static_cast<std::streamsize>(found->second.size()));
    ++report.texture_loaded_count;
    return output_path;
}

int parse_property_index(const std::map<std::string, std::string>& attrs, const char* name, const int fallback) {
    const auto found = attrs.find(name);
    if (found == attrs.end()) {
        return fallback;
    }
    return std::stoi(found->second);
}

std::array<std::uint8_t, 3> average_rgb(
    const std::array<std::uint8_t, 3>& a,
    const std::array<std::uint8_t, 3>& b,
    const std::array<std::uint8_t, 3>& c) {
    return {
        static_cast<std::uint8_t>((static_cast<int>(a.at(0)) + static_cast<int>(b.at(0)) + static_cast<int>(c.at(0))) / 3),
        static_cast<std::uint8_t>((static_cast<int>(a.at(1)) + static_cast<int>(b.at(1)) + static_cast<int>(c.at(1))) / 3),
        static_cast<std::uint8_t>((static_cast<int>(a.at(2)) + static_cast<int>(b.at(2)) + static_cast<int>(c.at(2))) / 3)};
}

std::uint8_t hex_to_u8(const std::string& value) {
    return static_cast<std::uint8_t>(std::stoi(value, nullptr, 16));
}

std::optional<std::array<std::uint8_t, 3>> parse_display_color(const std::string& color) {
    if (color.size() < 7 || color.at(0) != '#') {
        return std::nullopt;
    }
    const std::size_t offset = color.size() >= 9 ? 3U : 1U;
    if (offset + 5U >= color.size()) {
        return std::nullopt;
    }
    return std::array<std::uint8_t, 3>{
        hex_to_u8(color.substr(offset, 2)),
        hex_to_u8(color.substr(offset + 2U, 2)),
        hex_to_u8(color.substr(offset + 4U, 2))};
}

std::array<double, 12> identity_3mf_transform() {
    return {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0};
}

std::array<double, 12> parse_3mf_transform(const std::string& text) {
    if (text.empty()) {
        return identity_3mf_transform();
    }
    std::istringstream stream{text};
    std::array<double, 12> matrix{};
    for (double& value : matrix) {
        if (!(stream >> value)) {
            return identity_3mf_transform();
        }
    }
    return matrix;
}

Vec3 apply_3mf_transform(const Vec3& point, const std::array<double, 12>& m) {
    return {
        point.x * m.at(0) + point.y * m.at(3) + point.z * m.at(6) + m.at(9),
        point.x * m.at(1) + point.y * m.at(4) + point.z * m.at(7) + m.at(10),
        point.x * m.at(2) + point.y * m.at(5) + point.z * m.at(8) + m.at(11)};
}

std::array<double, 12> multiply_3mf_transform(const std::array<double, 12>& lhs, const std::array<double, 12>& rhs) {
    const Vec3 origin = apply_3mf_transform(apply_3mf_transform({0.0, 0.0, 0.0}, lhs), rhs);
    const Vec3 x = apply_3mf_transform(apply_3mf_transform({1.0, 0.0, 0.0}, lhs), rhs);
    const Vec3 y = apply_3mf_transform(apply_3mf_transform({0.0, 1.0, 0.0}, lhs), rhs);
    const Vec3 z = apply_3mf_transform(apply_3mf_transform({0.0, 0.0, 1.0}, lhs), rhs);
    return {
        x.x - origin.x, x.y - origin.y, x.z - origin.z,
        y.x - origin.x, y.y - origin.y, y.z - origin.z,
        z.x - origin.x, z.y - origin.y, z.z - origin.z,
        origin.x, origin.y, origin.z};
}

MaterialStat& material_stat(MeshData& mesh, const std::string& name);
MaterialInfo& material_info(MeshData& mesh, const std::string& name);

void add_three_mf_object_instance(
    const ThreeMfObject& object,
    const std::map<std::string, ThreeMfObject>& objects,
    const std::array<double, 12>& instance_transform,
    double unit_scale_to_mm,
    const TransformConfig& transform,
    MeshData& mesh);

void add_three_mf_mesh_instance(
    const ThreeMfObject& object,
    const std::array<double, 12>& instance_transform,
    const double unit_scale_to_mm,
    const TransformConfig& transform,
    MeshData& mesh) {
    const std::size_t vertex_offset = mesh.vertices.size();
    for (const Vec3& vertex : object.vertices) {
        Vec3 transformed = apply_3mf_transform(vertex, instance_transform);
        transformed = {transformed.x * unit_scale_to_mm, transformed.y * unit_scale_to_mm, transformed.z * unit_scale_to_mm};
        mesh.vertices.push_back(apply_transform(transformed, transform));
    }
    for (const ThreeMfTriangle& triangle : object.triangles) {
        mesh.faces.push_back({
            vertex_offset + triangle.vertices.at(0),
            vertex_offset + triangle.vertices.at(1),
            vertex_offset + triangle.vertices.at(2)});
        TriangleTextureInfo texture_info;
        texture_info.material_name = triangle.material_key;
        texture_info.has_uv = triangle.has_uv;
        texture_info.uv = triangle.uv;
        mesh.triangle_textures.push_back(texture_info);
        if (triangle.has_uv) {
            ++mesh.faces_with_uv;
        } else {
            ++mesh.faces_without_uv;
        }
        if (!triangle.material_key.empty()) {
            MaterialStat& stat = material_stat(mesh, triangle.material_key);
            ++stat.face_count;
            ++stat.triangle_count;
        }
        ++mesh.raw_face_count;
    }
}

void add_three_mf_object_instance(
    const ThreeMfObject& object,
    const std::map<std::string, ThreeMfObject>& objects,
    const std::array<double, 12>& instance_transform,
    const double unit_scale_to_mm,
    const TransformConfig& transform,
    MeshData& mesh) {
    if (object.is_mesh) {
        add_three_mf_mesh_instance(object, instance_transform, unit_scale_to_mm, transform, mesh);
        return;
    }
    if (!object.is_components) {
        return;
    }
    for (const auto& component : object.components) {
        const auto found = objects.find(component.first);
        if (found == objects.end()) {
            ++mesh.three_mf.invalid_reference_count;
            throw std::runtime_error("E_3MF_INVALID_COMPONENT_REFERENCE: component references missing object id: " + component.first);
        }
        const std::array<double, 12> combined = multiply_3mf_transform(component.second, instance_transform);
        add_three_mf_object_instance(found->second, objects, combined, unit_scale_to_mm, transform, mesh);
        ++mesh.three_mf.component_count;
    }
}

void load_3mf(const std::filesystem::path& path, const ModelLoadConfig& config, MeshData& mesh) {
    mesh.three_mf.enabled = true;
    mesh.three_mf.package_path = path;
    const TransformConfig& transform = config.transform;
    const std::vector<ZipEntryData> zip_entries = read_3mf_zip_entries(path, mesh.three_mf);
    std::map<std::string, std::vector<std::uint8_t>> entries;
    for (const ZipEntryData& entry : zip_entries) {
        entries.emplace(entry.name, entry.data);
    }
    if (entries.find("[Content_Types].xml") == entries.end()) {
        throw std::runtime_error("E_3MF_CONTENT_TYPES_MISSING: 3MF package missing [Content_Types].xml");
    }
    const std::string model_part = find_3mf_model_part(entries, mesh.three_mf);
    mesh.three_mf.model_part_path = model_part;
    const std::string model_xml = entry_text(entries, model_part);
    if (model_xml.empty()) {
        throw std::runtime_error("E_3MF_MODEL_PART_MISSING: 3MF package missing model part: " + model_part);
    }
    const ThreeMfXmlReader model_reader{model_xml};
    collect_unsupported_3mf_metadata(model_reader, mesh.three_mf);

    const auto model_attrs = model_reader.root_attributes();
    const auto unit = model_attrs.find("unit");
    if (unit != model_attrs.end()) {
        mesh.three_mf.unit = unit->second;
    }
    if (!is_supported_three_mf_unit(mesh.three_mf.unit)) {
        throw std::runtime_error("E_3MF_UNSUPPORTED_UNIT: unsupported 3MF unit: " + mesh.three_mf.unit);
    }
    mesh.three_mf.unit_scale_to_mm = three_mf_unit_scale_to_mm(mesh.three_mf.unit);

    std::map<std::string, std::string> material_name_by_key;
    std::map<std::string, ThreeMfColorGroup> color_groups;
    std::map<std::string, ThreeMfTexture2D> textures;
    std::map<std::string, ThreeMfTexture2DGroup> texture_groups;
    const std::filesystem::path texture_cache_dir = config.output_package_dir / "cache/3mf_textures";

    for (const std::string& color_group_block : model_reader.blocks("colorgroup")) {
        const std::size_t tag_end = color_group_block.find('>');
        const auto attrs = xml_attributes(color_group_block.substr(0, tag_end + 1U));
        ThreeMfColorGroup group;
        group.id = attrs.count("id") != 0 ? attrs.at("id") : "";
        for (const std::string& color_tag : model_reader.tags_in(color_group_block, "color")) {
            const auto color_attrs = xml_attributes(color_tag);
            const auto color = color_attrs.find("color");
            if (color != color_attrs.end()) {
                const auto rgb = parse_display_color(color->second);
                if (rgb.has_value()) {
                    group.colors.push_back(rgb.value());
                    ++mesh.three_mf.color_count;
                }
            }
        }
        if (!group.id.empty()) {
            color_groups.emplace(group.id, std::move(group));
            ++mesh.three_mf.color_group_count;
        }
    }

    for (const std::string& texture_tag : model_reader.tags("texture2d")) {
        const auto attrs = xml_attributes(texture_tag);
        ThreeMfTexture2D texture;
        texture.id = attrs.count("id") != 0 ? attrs.at("id") : "";
        texture.path = attrs.count("path") != 0 ? attrs.at("path") : "";
        texture.content_type = attrs.count("contenttype") != 0 ? attrs.at("contenttype") : "";
        if (!texture.id.empty()) {
            ++mesh.three_mf.texture2d_count;
            ++mesh.three_mf.texture_resource_count;
            if (!texture.path.empty()) {
                texture.extracted_path =
                    extract_3mf_internal_texture(entries, texture.id, texture.path, texture_cache_dir, mesh.three_mf);
                texture.loaded = std::filesystem::exists(texture.extracted_path);
            } else {
                ++mesh.three_mf.texture_missing_count;
                mesh.three_mf.warnings.push_back("E_3MF_TEXTURE_PATH_MISSING: texture2d has no path: " + texture.id);
            }
            textures.emplace(texture.id, std::move(texture));
        }
    }

    for (const std::string& texture_group_block : model_reader.blocks("texture2dgroup")) {
        const std::size_t tag_end = texture_group_block.find('>');
        const auto attrs = xml_attributes(texture_group_block.substr(0, tag_end + 1U));
        ThreeMfTexture2DGroup group;
        group.id = attrs.count("id") != 0 ? attrs.at("id") : "";
        group.texid = attrs.count("texid") != 0 ? attrs.at("texid") : "";
        if (group.texid.empty()) {
            mesh.three_mf.warnings.push_back("E_3MF_TEXTURE2DGROUP_MISSING_TEXID: texture2dgroup has no texid: " + group.id);
        }
        for (const std::string& coord_tag : model_reader.tags_in(texture_group_block, "tex2coord")) {
            const auto coord_attrs = xml_attributes(coord_tag);
            group.coords.push_back({
                std::stod(coord_attrs.at("u")),
                std::stod(coord_attrs.at("v"))});
            ++mesh.three_mf.tex2coord_count;
        }
        if (!group.id.empty()) {
            texture_groups.emplace(group.id, std::move(group));
            ++mesh.three_mf.texture2d_group_count;
        }
    }

    for (const std::string& base_materials_block : model_reader.blocks("basematerials")) {
        const std::size_t tag_end = base_materials_block.find('>');
        const auto attrs = xml_attributes(base_materials_block.substr(0, tag_end + 1U));
        const std::string resource_id = attrs.count("id") != 0 ? attrs.at("id") : "";
        int index{0};
        for (const std::string& base_tag : model_reader.tags_in(base_materials_block, "base")) {
            const auto base_attrs = xml_attributes(base_tag);
            const std::string material_key = resource_id + ":" + std::to_string(index);
            MaterialInfo& info = material_info(mesh, material_key);
            info.name = base_attrs.count("name") != 0 ? base_attrs.at("name") : material_key;
            material_name_by_key[material_key] = info.name;
            const auto display_color = base_attrs.find("displaycolor");
            if (display_color != base_attrs.end()) {
                const auto rgb = parse_display_color(display_color->second);
                if (rgb.has_value()) {
                    info.diffuse_rgb = rgb.value();
                    info.has_diffuse = true;
                }
            }
            ++index;
        }
        mesh.three_mf.material_resource_count += index;
    }

    std::map<std::string, ThreeMfObject> objects;
    for (const std::string& object_block : model_reader.blocks("object")) {
        const std::size_t tag_end = object_block.find('>');
        const auto attrs = xml_attributes(object_block.substr(0, tag_end + 1U));
        ThreeMfObject object;
        object.id = attrs.count("id") != 0 ? attrs.at("id") : "";
        const std::string block = object_block.substr(tag_end + 1U);
        const std::string mesh_block = model_reader.block_in(block, "mesh");
        const std::string components_block = model_reader.block_in(block, "components");
        if (!mesh_block.empty()) {
            object.is_mesh = true;
            for (const std::string& vertex_tag : model_reader.tags_in(mesh_block, "vertex")) {
                const auto vertex_attrs = xml_attributes(vertex_tag);
                object.vertices.push_back({
                    std::stod(vertex_attrs.at("x")),
                    std::stod(vertex_attrs.at("y")),
                    std::stod(vertex_attrs.at("z"))});
            }
            if (object.vertices.empty()) {
                throw std::runtime_error("E_3MF_XML_PARSE_FAILED: mesh object has no vertices");
            }
            for (const std::string& triangle_tag : model_reader.tags_in(mesh_block, "triangle")) {
                const auto triangle_attrs = xml_attributes(triangle_tag);
                ThreeMfTriangle triangle;
                triangle.vertices = {
                    static_cast<std::size_t>(std::stoul(triangle_attrs.at("v1"))),
                    static_cast<std::size_t>(std::stoul(triangle_attrs.at("v2"))),
                    static_cast<std::size_t>(std::stoul(triangle_attrs.at("v3")))};
                for (const std::size_t vertex_index : triangle.vertices) {
                    if (vertex_index >= object.vertices.size()) {
                        ++mesh.three_mf.invalid_reference_count;
                        throw std::runtime_error("E_3MF_INVALID_TRIANGLE_INDEX: triangle references vertex outside loaded range");
                    }
                }
                const auto pid = triangle_attrs.find("pid");
                const auto p1 = triangle_attrs.find("p1");
                if (pid != triangle_attrs.end()) {
                    const int pindex = triangle_attrs.count("pindex") != 0 ? std::stoi(triangle_attrs.at("pindex")) : -1;
                    const int property_1 = parse_property_index(triangle_attrs, "p1", pindex);
                    const int property_2 = parse_property_index(triangle_attrs, "p2", property_1);
                    const int property_3 = parse_property_index(triangle_attrs, "p3", property_1);
                    const auto texture_group = texture_groups.find(pid->second);
                    const auto color_group = color_groups.find(pid->second);
                    if (texture_group != texture_groups.end()) {
                        const ThreeMfTexture2DGroup& group = texture_group->second;
                        const auto texture = textures.find(group.texid);
                        if (texture == textures.end()) {
                            ++mesh.three_mf.invalid_reference_count;
                            throw std::runtime_error("E_3MF_TEXTURE2DGROUP_MISSING_TEXID: texture2dgroup references missing texid: " + group.texid);
                        }
                        if (property_1 < 0 || property_2 < 0 || property_3 < 0
                            || property_1 >= static_cast<int>(group.coords.size())
                            || property_2 >= static_cast<int>(group.coords.size())
                            || property_3 >= static_cast<int>(group.coords.size())) {
                            ++mesh.three_mf.invalid_reference_count;
                            throw std::runtime_error("E_3MF_TEX2COORD_INDEX_OUT_OF_RANGE: texture coordinate index outside group");
                        }
                        const std::string material_name = "3mf_texture2dgroup_" + group.id;
                        MaterialInfo& info = material_info(mesh, material_name);
                        info.name = material_name;
                        info.has_texture = true;
                        info.diffuse_texture_path = texture->second.extracted_path;
                        info.texture_exists = texture->second.loaded;
                        info.texture_source = "3mf_internal";
                        info.diffuse_rgb = {0, 0, 0};
                        info.has_diffuse = true;
                        triangle.material_key = material_name;
                        triangle.has_uv = true;
                        triangle.uv = {
                            group.coords.at(static_cast<std::size_t>(property_1)),
                            group.coords.at(static_cast<std::size_t>(property_2)),
                            group.coords.at(static_cast<std::size_t>(property_3))};
                        ++mesh.three_mf.texture_group_resolved_triangles;
                    } else if (color_group != color_groups.end()) {
                        const ThreeMfColorGroup& group = color_group->second;
                        if (property_1 < 0 || property_2 < 0 || property_3 < 0
                            || property_1 >= static_cast<int>(group.colors.size())
                            || property_2 >= static_cast<int>(group.colors.size())
                            || property_3 >= static_cast<int>(group.colors.size())) {
                            ++mesh.three_mf.invalid_reference_count;
                            throw std::runtime_error("E_3MF_COLORGROUP_INDEX_OUT_OF_RANGE: color index outside group");
                        }
                        std::array<std::uint8_t, 3> rgb = group.colors.at(static_cast<std::size_t>(property_1));
                        if (property_1 != property_2 || property_1 != property_3) {
                            rgb = average_rgb(
                                group.colors.at(static_cast<std::size_t>(property_1)),
                                group.colors.at(static_cast<std::size_t>(property_2)),
                                group.colors.at(static_cast<std::size_t>(property_3)));
                            ++mesh.three_mf.interpolated_color_fallback_count;
                        }
                        const std::string material_name = "3mf_colorgroup_" + group.id + "_" + std::to_string(property_1);
                        MaterialInfo& info = material_info(mesh, material_name);
                        info.name = material_name;
                        info.diffuse_rgb = rgb;
                        info.has_diffuse = true;
                        triangle.material_key = material_name;
                        ++mesh.three_mf.color_group_resolved_triangles;
                    } else if (p1 != triangle_attrs.end()) {
                        const std::string material_key = pid->second + ":" + p1->second;
                        const auto material_name = material_name_by_key.find(material_key);
                        if (material_name == material_name_by_key.end()) {
                            ++mesh.three_mf.unknown_material_count;
                            mesh.three_mf.warnings.push_back(
                                "E_3MF_UNKNOWN_MATERIAL_ID: triangle references unknown material id: " + material_key);
                            MaterialInfo& fallback = material_info(mesh, material_key);
                            fallback.name = material_key;
                            fallback.diffuse_rgb = {0, 0, 0};
                            fallback.has_diffuse = true;
                            triangle.material_key = fallback.name;
                        } else {
                            triangle.material_key = material_name->second;
                        }
                    }
                }
                object.triangles.push_back(triangle);
            }
            if (object.triangles.empty()) {
                throw std::runtime_error("E_3MF_XML_PARSE_FAILED: mesh object has no triangles");
            }
            ++mesh.three_mf.mesh_object_count;
        } else if (!components_block.empty()) {
            object.is_components = true;
            for (const std::string& component_tag : model_reader.tags_in(components_block, "component")) {
                const auto component_attrs = xml_attributes(component_tag);
                const std::string object_id = component_attrs.count("objectid") != 0 ? component_attrs.at("objectid") : "";
                const std::array<double, 12> matrix =
                    parse_3mf_transform(component_attrs.count("transform") != 0 ? component_attrs.at("transform") : "");
                object.components.push_back({object_id, matrix});
            }
        }
        if (!object.id.empty()) {
            objects.emplace(object.id, object);
            ++mesh.three_mf.object_count;
        }
    }

    const std::string build_block = model_reader.block("build");
    if (build_block.empty()) {
        throw std::runtime_error("E_3MF_XML_PARSE_FAILED: 3MF model part does not contain build items");
    }
    for (const std::string& item_tag : model_reader.tags_in(build_block, "item")) {
        const auto item_attrs = xml_attributes(item_tag);
        const std::string object_id = item_attrs.count("objectid") != 0 ? item_attrs.at("objectid") : "";
        const auto found = objects.find(object_id);
        if (found == objects.end()) {
            ++mesh.three_mf.invalid_reference_count;
            throw std::runtime_error("E_3MF_INVALID_COMPONENT_REFERENCE: build item references missing object id: " + object_id);
        }
        const std::array<double, 12> matrix =
            parse_3mf_transform(item_attrs.count("transform") != 0 ? item_attrs.at("transform") : "");
        add_three_mf_object_instance(found->second, objects, matrix, mesh.three_mf.unit_scale_to_mm, transform, mesh);
    }
    mesh.three_mf.triangle_count = static_cast<int>(mesh.faces.size());
}

MaterialStat& material_stat(MeshData& mesh, const std::string& name) {
    const auto found = std::find_if(mesh.materials.begin(), mesh.materials.end(), [&](const MaterialStat& item) {
        return item.name == name;
    });
    if (found != mesh.materials.end()) {
        return *found;
    }
    mesh.materials.push_back({name, 0, 0});
    return mesh.materials.back();
}

MaterialInfo& material_info(MeshData& mesh, const std::string& name) {
    const auto found = std::find_if(mesh.material_infos.begin(), mesh.material_infos.end(), [&](const MaterialInfo& item) {
        return item.name == name;
    });
    if (found != mesh.material_infos.end()) {
        return *found;
    }
    mesh.material_infos.push_back(MaterialInfo{});
    mesh.material_infos.back().name = name;
    return mesh.material_infos.back();
}

void load_mtl(
    const std::filesystem::path& mtl_path,
    const std::filesystem::path& obj_dir,
    MeshData& mesh) {
    std::ifstream input{mtl_path};
    if (!input) {
        return;
    }

    const model_detail::MtlMaterialContext context{mtl_path.parent_path(), obj_dir};
    MaterialInfo* current{nullptr};
    std::string line;
    while (std::getline(input, line)) {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        std::istringstream stream{line};
        std::string token;
        stream >> token;
        std::string arguments;
        std::getline(stream, arguments);
        if (token == "newmtl") {
            const std::string name = model_detail::TrimMaterialName(arguments);
            current = name.empty() ? nullptr : &material_info(mesh, name);
        } else if (current != nullptr) {
            (void)model_detail::ApplyMtlMaterialLine(token, arguments, context, current);
        }
    }
}


void load_obj(const std::filesystem::path& path, const TransformConfig& transform, MeshData& mesh) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("failed to open OBJ model: " + path.string());
    }

    std::string active_material;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream stream{line};
        std::string token;
        stream >> token;
        if (token != "v") {
            if (token == "mtllib") {
                std::string library;
                while (stream >> library) {
                    mesh.material_libraries.push_back(library);
                }
            } else if (token == "usemtl") {
                std::string material_line;
                std::getline(stream, material_line);
                active_material = model_detail::TrimMaterialName(material_line);
                if (!active_material.empty()) {
                    (void)material_stat(mesh, active_material);
                }
            } else if (token == "vt") {
                TexCoord texcoord{};
                if (!(stream >> texcoord.u >> texcoord.v)) {
                    throw std::runtime_error("invalid OBJ texture coordinate line in: " + path.string());
                }
                mesh.texcoords.push_back(texcoord);
            } else if (token == "vn")
            {
                Vec3 normal{};
                if (!(stream >> normal.x >> normal.y >> normal.z))
                {
                    throw std::runtime_error("invalid OBJ normal line in: " + path.string());
                }
                ++mesh.normal_count;
            } else if (token == "f") {
                std::vector<model_detail::ObjFaceVertex> face_vertices;
                std::string face_token;
                while (stream >> face_token) {
                    face_vertices.push_back(model_detail::ParseObjFaceVertex(
                        face_token,
                        mesh.vertices.size(),
                        mesh.texcoords.size(),
                        mesh.normal_count));
                }
                if (face_vertices.size() < 3) {
                    throw std::runtime_error("OBJ face has fewer than three vertices: " + path.string());
                }
                ++mesh.raw_face_count;
                const bool face_has_uv = std::all_of(face_vertices.begin(), face_vertices.end(), [](const model_detail::ObjFaceVertex& item) {
                    return item.texcoord_index >= 0;
                });
                if (face_has_uv) {
                    ++mesh.faces_with_uv;
                } else {
                    ++mesh.faces_without_uv;
                }
                const bool faceHasNormals = std::all_of(
                    face_vertices.begin(),
                    face_vertices.end(),
                    [](const model_detail::ObjFaceVertex& item) {
                        return item.normal_index >= 0;
                    });
                mesh.has_normals = mesh.has_normals || faceHasNormals;
                const std::size_t triangle_count = face_vertices.size() - 2U;
                if (!active_material.empty()) {
                    MaterialStat& stat = material_stat(mesh, active_material);
                    ++stat.face_count;
                    stat.triangle_count += triangle_count;
                }
                for (std::size_t i{1}; i + 1U < face_vertices.size(); ++i) {
                    const model_detail::ObjFaceVertex& a = face_vertices.at(0);
                    const model_detail::ObjFaceVertex& b = face_vertices.at(i);
                    const model_detail::ObjFaceVertex& c = face_vertices.at(i + 1U);
                    mesh.faces.push_back({a.position_index, b.position_index, c.position_index});
                    TriangleTextureInfo texture_info;
                    texture_info.material_name = active_material;
                    texture_info.has_uv = a.texcoord_index >= 0 && b.texcoord_index >= 0 && c.texcoord_index >= 0;
                    if (texture_info.has_uv) {
                        texture_info.uv = {
                            mesh.texcoords.at(static_cast<std::size_t>(a.texcoord_index)),
                            mesh.texcoords.at(static_cast<std::size_t>(b.texcoord_index)),
                            mesh.texcoords.at(static_cast<std::size_t>(c.texcoord_index))};
                    }
                    mesh.triangle_textures.push_back(texture_info);
                }
            }
            continue;
        }
        Vec3 vertex{};
        if (!(stream >> vertex.x >> vertex.y >> vertex.z)) {
            throw std::runtime_error("invalid OBJ vertex line in: " + path.string());
        }
        mesh.vertices.push_back(apply_transform(vertex, transform));
    }
}

}  // namespace

ModelAppearanceAssessment AssessModelAppearance(const ModelReport& report)
{
    std::map<std::string, const MaterialInfo*> materials;
    for (const MaterialInfo& material : report.material_infos)
    {
        materials.emplace(material.name, &material);
    }

    bool hasNamedBinding{false};
    for (const TriangleTextureInfo& binding : report.triangle_textures)
    {
        if (binding.material_name.empty())
        {
            continue;
        }
        hasNamedBinding = true;
        const auto material = materials.find(binding.material_name);
        if (material == materials.end())
        {
            if (report.material_libraries.empty()
                && report.material_infos.empty())
            {
                return {
                    "degraded_missing_material_definition",
                    true,
                    "used material '" + binding.material_name
                        + "' has no mtllib/MTL definition"};
            }
            return {
                "invalid_material_binding",
                false,
                "used material '" + binding.material_name
                    + "' is not resolved by the declared material libraries"};
        }
        const MaterialInfo& resolved = *material->second;
        if (resolved.has_texture
            && (resolved.diffuse_texture_path.empty()
                || !resolved.texture_exists))
        {
            return {
                "degraded_missing_texture",
                true,
                "used material '" + binding.material_name
                    + "' references a missing diffuse texture: "
                    + resolved.diffuse_texture_path.generic_string()};
        }
    }

    if (!hasNamedBinding && report.material_infos.empty())
    {
        return {"untextured", false, "model declares no material binding"};
    }
    return {};
}

ModelReport load_model_report(const ModelLoadConfig& config, const std::filesystem::path& config_dir) {
    const std::filesystem::path model_path = resolve_path(config.input.model_path, config_dir);
    if (!std::filesystem::exists(model_path)) {
        throw std::runtime_error("model file does not exist: " + model_path.string());
    }

    const std::string format = detect_format(model_path, config.input.format);
    MeshData mesh;
    if (format == "stl") {
        load_stl(model_path, config.transform, mesh);
    } else if (format == "obj") {
        load_obj(model_path, config.transform, mesh);
        for (const std::string& library : mesh.material_libraries) {
            const std::filesystem::path mtl_path = (model_path.parent_path() / library).lexically_normal();
            load_mtl(mtl_path, model_path.parent_path(), mesh);
        }
    } else if (format == "3mf") {
        load_3mf(model_path, config, mesh);
    } else {
        throw std::runtime_error("unsupported model format for P0: " + format);
    }

    if (mesh.vertices.empty()) {
        throw std::runtime_error("model contains no readable vertices: " + model_path.string());
    }
    if (mesh.faces.empty()) {
        throw std::runtime_error("model contains no readable triangle faces: " + model_path.string());
    }

    const BoundingBox original_bbox = compute_bbox(mesh.vertices);
    const OrientationCandidate orientation =
        choose_auto_orientation(mesh.vertices, original_bbox, config.auto_orient);

    ModelReport report;
    report.model_path = model_path;
    report.format = format;
    report.stl_encoding = mesh.stl_encoding;
    report.vertex_count = mesh.vertices.size();
    report.face_count = mesh.raw_face_count;
    report.triangle_count = mesh.faces.size();
    report.degenerate_triangle_count = count_degenerate_triangles(mesh.vertices, mesh.faces);
    report.texcoord_count = mesh.texcoords.size();
    report.has_normals = mesh.has_normals;
    report.faces_with_uv = mesh.faces_with_uv;
    report.faces_without_uv = mesh.faces_without_uv;
    report.material_libraries = mesh.material_libraries;
    report.materials = mesh.materials;
    report.material_infos = mesh.material_infos;
    report.three_mf = mesh.three_mf;
    report.auto_orient.enabled = config.auto_orient.enabled;
    report.auto_orient.max_height_mm = config.auto_orient.max_height_mm;
    report.auto_orient.selected_orientation = orientation.name;
    report.auto_orient.applied = orientation.name != "identity";
    report.auto_orient.rotation_deg = orientation.rotation_deg;
    report.auto_orient.original_bbox_mm = original_bbox;
    report.bbox_mm = orientation.bbox;
    report.triangles = build_triangles(orientation.vertices, mesh.faces);
    report.triangle_textures = mesh.triangle_textures;
    return report;
}

}  // namespace slicer_core
