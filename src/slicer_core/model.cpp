#include "slicer_core/model.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace slicer_core {
namespace {

constexpr double pi{3.14159265358979323846};

struct MeshData {
    std::vector<Vec3> vertices;
    std::vector<TexCoord> texcoords;
    std::vector<std::array<std::size_t, 3>> faces;
    std::vector<TriangleTextureInfo> triangle_textures;
    std::size_t raw_face_count{0};
    std::size_t faces_with_uv{0};
    std::size_t faces_without_uv{0};
    std::string stl_encoding;
    std::vector<std::string> material_libraries;
    std::vector<MaterialStat> materials;
    std::vector<MaterialInfo> material_infos;
};

struct ObjFaceVertex {
    std::size_t position_index{0};
    int texcoord_index{-1};
    int normal_index{-1};
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

OrientationCandidate choose_auto_orientation(
    const std::vector<Vec3>& vertices,
    const BoundingBox& original_bbox,
    const AutoOrientConfig& config) {
    const OrientationCandidate identity{"identity", {0.0, 0.0, 0.0}, vertices, original_bbox};
    if (!config.enabled || bbox_height(original_bbox) <= config.max_height_mm) {
        return identity;
    }

    std::vector<OrientationCandidate> candidates;
    candidates.push_back(identity);
    candidates.push_back(make_orientation_candidate("rotate_x_90", {90.0, 0.0, 0.0}, vertices));
    candidates.push_back(make_orientation_candidate("rotate_x_minus_90", {-90.0, 0.0, 0.0}, vertices));
    candidates.push_back(make_orientation_candidate("rotate_y_90", {0.0, 90.0, 0.0}, vertices));
    candidates.push_back(make_orientation_candidate("rotate_y_minus_90", {0.0, -90.0, 0.0}, vertices));

    const auto better = [&](const OrientationCandidate& lhs, const OrientationCandidate& rhs) {
        const bool lhs_fits = bbox_height(lhs.bbox) <= config.max_height_mm;
        const bool rhs_fits = bbox_height(rhs.bbox) <= config.max_height_mm;
        if (lhs_fits != rhs_fits) {
            return lhs_fits;
        }
        if (lhs_fits && rhs_fits) {
            return bbox_footprint_area(lhs.bbox) < bbox_footprint_area(rhs.bbox);
        }
        return bbox_height(lhs.bbox) < bbox_height(rhs.bbox);
    };

    return *std::min_element(candidates.begin(), candidates.end(), [&](const auto& lhs, const auto& rhs) {
        return better(lhs, rhs);
    });
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

std::size_t parse_obj_index(const std::string& token, const std::size_t vertex_count) {
    const std::size_t slash = token.find('/');
    const std::string index_text = slash == std::string::npos ? token : token.substr(0, slash);
    if (index_text.empty()) {
        throw std::runtime_error("OBJ face contains empty vertex index");
    }
    const int index = std::stoi(index_text);
    if (index == 0) {
        throw std::runtime_error("OBJ vertex indices are 1-based; zero is invalid");
    }
    if (index > 0) {
        const std::size_t zero_based = static_cast<std::size_t>(index - 1);
        if (zero_based >= vertex_count) {
            throw std::runtime_error("OBJ face references vertex outside loaded range");
        }
        return zero_based;
    }
    const int resolved = static_cast<int>(vertex_count) + index;
    if (resolved < 0) {
        throw std::runtime_error("OBJ negative face index is outside loaded range");
    }
    return static_cast<std::size_t>(resolved);
}

int parse_optional_obj_index(const std::string& token, const std::size_t item_count) {
    if (token.empty()) {
        return -1;
    }
    const int index = std::stoi(token);
    if (index == 0) {
        throw std::runtime_error("OBJ indices are 1-based; zero is invalid");
    }
    if (index > 0) {
        const int zero_based = index - 1;
        if (zero_based >= static_cast<int>(item_count)) {
            throw std::runtime_error("OBJ face references index outside loaded range");
        }
        return zero_based;
    }
    const int resolved = static_cast<int>(item_count) + index;
    if (resolved < 0) {
        throw std::runtime_error("OBJ negative face index is outside loaded range");
    }
    return resolved;
}

ObjFaceVertex parse_obj_face_vertex(
    const std::string& token,
    const std::size_t vertex_count,
    const std::size_t texcoord_count) {
    ObjFaceVertex result;
    const std::size_t first_slash = token.find('/');
    if (first_slash == std::string::npos) {
        result.position_index = parse_obj_index(token, vertex_count);
        return result;
    }

    const std::string position_text = token.substr(0, first_slash);
    result.position_index = parse_obj_index(position_text, vertex_count);

    const std::size_t second_slash = token.find('/', first_slash + 1U);
    const std::string texcoord_text = second_slash == std::string::npos
        ? token.substr(first_slash + 1U)
        : token.substr(first_slash + 1U, second_slash - first_slash - 1U);
    result.texcoord_index = parse_optional_obj_index(texcoord_text, texcoord_count);

    if (second_slash != std::string::npos) {
        const std::string normal_text = token.substr(second_slash + 1U);
        result.normal_index = normal_text.empty() ? -1 : std::stoi(normal_text);
    }
    return result;
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

std::string trim_copy(const std::string& input) {
    const auto first = std::find_if_not(input.begin(), input.end(), [](const unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(input.rbegin(), input.rend(), [](const unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::uint8_t kd_component_to_u8(const double value) {
    const double clamped = std::clamp(value, 0.0, 1.0);
    return static_cast<std::uint8_t>(std::round(clamped * 255.0));
}

std::filesystem::path resolve_texture_path(
    const std::filesystem::path& raw_path,
    const std::filesystem::path& mtl_dir,
    const std::filesystem::path& obj_dir) {
    if (raw_path.is_absolute()) {
        return raw_path.lexically_normal();
    }
    const std::filesystem::path from_mtl = (mtl_dir / raw_path).lexically_normal();
    if (std::filesystem::exists(from_mtl)) {
        return from_mtl;
    }
    return (obj_dir / raw_path).lexically_normal();
}

void load_mtl(
    const std::filesystem::path& mtl_path,
    const std::filesystem::path& obj_dir,
    MeshData& mesh) {
    std::ifstream input{mtl_path};
    if (!input) {
        return;
    }

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
        if (token == "newmtl") {
            std::string name;
            stream >> name;
            current = name.empty() ? nullptr : &material_info(mesh, name);
        } else if (token == "Kd" && current != nullptr) {
            double r{0.0};
            double g{0.0};
            double b{0.0};
            if (stream >> r >> g >> b) {
                current->diffuse_rgb = {kd_component_to_u8(r), kd_component_to_u8(g), kd_component_to_u8(b)};
                current->has_diffuse = true;
            }
        } else if (token == "map_Kd" && current != nullptr) {
            std::string rest;
            std::getline(stream, rest);
            const std::string texture_name = trim_copy(rest);
            if (!texture_name.empty()) {
                current->diffuse_texture_path = resolve_texture_path(texture_name, mtl_path.parent_path(), obj_dir);
                current->has_texture = true;
                current->texture_exists = std::filesystem::exists(current->diffuse_texture_path);
            }
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
                stream >> active_material;
                if (!active_material.empty()) {
                    (void)material_stat(mesh, active_material);
                }
            } else if (token == "vt") {
                TexCoord texcoord{};
                if (!(stream >> texcoord.u >> texcoord.v)) {
                    throw std::runtime_error("invalid OBJ texture coordinate line in: " + path.string());
                }
                mesh.texcoords.push_back(texcoord);
            } else if (token == "f") {
                std::vector<ObjFaceVertex> face_vertices;
                std::string face_token;
                while (stream >> face_token) {
                    face_vertices.push_back(parse_obj_face_vertex(face_token, mesh.vertices.size(), mesh.texcoords.size()));
                }
                if (face_vertices.size() < 3) {
                    throw std::runtime_error("OBJ face has fewer than three vertices: " + path.string());
                }
                ++mesh.raw_face_count;
                const bool face_has_uv = std::all_of(face_vertices.begin(), face_vertices.end(), [](const ObjFaceVertex& item) {
                    return item.texcoord_index >= 0;
                });
                if (face_has_uv) {
                    ++mesh.faces_with_uv;
                } else {
                    ++mesh.faces_without_uv;
                }
                const std::size_t triangle_count = face_vertices.size() - 2U;
                if (!active_material.empty()) {
                    MaterialStat& stat = material_stat(mesh, active_material);
                    ++stat.face_count;
                    stat.triangle_count += triangle_count;
                }
                for (std::size_t i{1}; i + 1U < face_vertices.size(); ++i) {
                    const ObjFaceVertex& a = face_vertices.at(0);
                    const ObjFaceVertex& b = face_vertices.at(i);
                    const ObjFaceVertex& c = face_vertices.at(i + 1U);
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

ModelReport load_model_report(const SliceConfig& config, const std::filesystem::path& config_dir) {
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
    report.faces_with_uv = mesh.faces_with_uv;
    report.faces_without_uv = mesh.faces_without_uv;
    report.material_libraries = mesh.material_libraries;
    report.materials = mesh.materials;
    report.material_infos = mesh.material_infos;
    report.auto_orient.enabled = config.auto_orient.enabled;
    report.auto_orient.max_height_mm = config.auto_orient.max_height_mm;
    report.auto_orient.selected_orientation = orientation.name;
    report.auto_orient.applied = orientation.name != "identity";
    report.auto_orient.original_bbox_mm = original_bbox;
    report.bbox_mm = orientation.bbox;
    report.triangles = build_triangles(orientation.vertices, mesh.faces);
    report.triangle_textures = mesh.triangle_textures;
    return report;
}

}  // namespace slicer_core
