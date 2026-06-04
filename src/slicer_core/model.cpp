#include "slicer_core/model.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace slicer_core {
namespace {

constexpr double pi{3.14159265358979323846};

struct BboxAccumulator {
    BoundingBox box{
        {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()},
        {std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()}};
    std::size_t vertex_count{0};

    void add(const Vec3& point) {
        box.min.x = std::min(box.min.x, point.x);
        box.min.y = std::min(box.min.y, point.y);
        box.min.z = std::min(box.min.z, point.z);
        box.max.x = std::max(box.max.x, point.x);
        box.max.y = std::max(box.max.y, point.y);
        box.max.z = std::max(box.max.z, point.z);
        ++vertex_count;
    }
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

void load_ascii_stl(const std::filesystem::path& path, const TransformConfig& transform, BboxAccumulator& accumulator) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("failed to open STL model: " + path.string());
    }

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
        accumulator.add(apply_transform(vertex, transform));
    }
}

void load_obj(const std::filesystem::path& path, const TransformConfig& transform, BboxAccumulator& accumulator) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("failed to open OBJ model: " + path.string());
    }

    std::string line;
    while (std::getline(input, line)) {
        std::istringstream stream{line};
        std::string token;
        stream >> token;
        if (token != "v") {
            continue;
        }
        Vec3 vertex{};
        if (!(stream >> vertex.x >> vertex.y >> vertex.z)) {
            throw std::runtime_error("invalid OBJ vertex line in: " + path.string());
        }
        accumulator.add(apply_transform(vertex, transform));
    }
}

}  // namespace

ModelReport load_model_report(const SliceConfig& config, const std::filesystem::path& config_dir) {
    const std::filesystem::path model_path = resolve_path(config.input.model_path, config_dir);
    if (!std::filesystem::exists(model_path)) {
        throw std::runtime_error("model file does not exist: " + model_path.string());
    }

    const std::string format = detect_format(model_path, config.input.format);
    BboxAccumulator accumulator;
    if (format == "stl") {
        load_ascii_stl(model_path, config.transform, accumulator);
    } else if (format == "obj") {
        load_obj(model_path, config.transform, accumulator);
    } else {
        throw std::runtime_error("unsupported model format for P0: " + format);
    }

    if (accumulator.vertex_count == 0) {
        throw std::runtime_error("model contains no readable vertices: " + model_path.string());
    }

    ModelReport report;
    report.model_path = model_path;
    report.format = format;
    report.vertex_count = accumulator.vertex_count;
    report.triangle_count = accumulator.vertex_count / 3;
    report.bbox_mm = accumulator.box;
    return report;
}

}  // namespace slicer_core
