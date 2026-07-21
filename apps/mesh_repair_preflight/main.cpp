#include "slicer_core/config.h"
#include "slicer_core/diagnostics/MeshRepairReport.h"
#include "slicer_core/geometry/MeshScaleTolerance.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshRepairPreflight.h"
#include "slicer_core/geometry/repair/MeshRepairService.h"
#include "slicer_core/model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{

using Matrix4 = std::array<double, 16>;

struct Options
{
    std::filesystem::path configPath;
    std::filesystem::path outputPath;
    std::string sourceId;
    double voxelMm{0.10};
    std::size_t maxSelfIntersectionPairs{128U};
    std::size_t maxTrianglePairChecks{250000U};
    bool requireOpenVdbOff{false};
    bool executeCleanup{false};
    bool executeR2Topology{false};
    bool executeR2Boundary{false};
    bool executeR2EvidenceGuard{false};
    bool classifyR3NonManifoldPatterns{false};
    double weldToleranceMm{0.0};
    std::size_t maxBoundaryLoopEdges{0U};
    double maxBoundaryLoopDiameterMm{0.0};
    double maxBoundaryLoopPerimeterMm{0.0};
    double maxBoundaryPlanarityErrorMm{0.0};
    double maxHoleAreaMm2{0.0};
    double maxAffectedFaceRatio{0.0};
};

std::string RequireValue(
    const int argc,
    char** argv,
    int& index,
    const std::string& argument)
{
    if (index + 1 >= argc)
    {
        throw std::runtime_error(argument + " requires a value");
    }
    ++index;
    return argv[index];
}

Options ParseOptions(const int argc, char** argv)
{
    Options options;
    for (int index{1}; index < argc; ++index)
    {
        const std::string argument{argv[index]};
        if (argument == "--config")
        {
            options.configPath = RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--output")
        {
            options.outputPath = RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--source-id")
        {
            options.sourceId = RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--voxel-mm")
        {
            options.voxelMm = std::stod(RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--max-self-intersection-pairs")
        {
            options.maxSelfIntersectionPairs = static_cast<std::size_t>(
                std::stoull(RequireValue(argc, argv, index, argument)));
        }
        else if (argument == "--max-triangle-pair-checks")
        {
            options.maxTrianglePairChecks = static_cast<std::size_t>(
                std::stoull(RequireValue(argc, argv, index, argument)));
        }
        else if (argument == "--require-openvdb-off")
        {
            options.requireOpenVdbOff = true;
        }
        else if (argument == "--execute-cleanup")
        {
            options.executeCleanup = true;
        }
        else if (argument == "--execute-r2-02")
        {
            options.executeR2Topology = true;
        }
        else if (argument == "--execute-r2-03")
        {
            options.executeR2Boundary = true;
        }
        else if (argument == "--execute-r2-04")
        {
            options.executeR2EvidenceGuard = true;
        }
        else if (argument == "--classify-r3-01")
        {
            options.classifyR3NonManifoldPatterns = true;
        }
        else if (argument == "--weld-tolerance-mm")
        {
            options.weldToleranceMm = std::stod(RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--max-boundary-loop-edges")
        {
            options.maxBoundaryLoopEdges = static_cast<std::size_t>(
                std::stoull(RequireValue(argc, argv, index, argument)));
        }
        else if (argument == "--max-boundary-loop-diameter-mm")
        {
            options.maxBoundaryLoopDiameterMm = std::stod(
                RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--max-boundary-loop-perimeter-mm")
        {
            options.maxBoundaryLoopPerimeterMm = std::stod(
                RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--max-boundary-planarity-error-mm")
        {
            options.maxBoundaryPlanarityErrorMm = std::stod(
                RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--max-hole-area-mm2")
        {
            options.maxHoleAreaMm2 = std::stod(RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--max-affected-face-ratio")
        {
            options.maxAffectedFaceRatio = std::stod(
                RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--help" || argument == "-h")
        {
            std::cout
                << "mesh_repair_preflight --config <config.json> --output <report.json> "
                << "[--source-id <stable-path>] [--voxel-mm <value>] "
                << "[--max-self-intersection-pairs <count>] "
                << "[--max-triangle-pair-checks <count>] [--require-openvdb-off] "
                << "[--execute-cleanup | --execute-r2-02 --weld-tolerance-mm <value> | "
                << "--execute-r2-03 --weld-tolerance-mm <value> "
                << "--max-boundary-loop-edges <count> "
                << "--max-boundary-loop-diameter-mm <value> "
                << "--max-boundary-loop-perimeter-mm <value> "
                << "--max-boundary-planarity-error-mm <value> "
                << "--max-hole-area-mm2 <value> --max-affected-face-ratio <value> | "
                << "--execute-r2-04 with the same explicit R2-03 budgets | "
                << "--classify-r3-01]\n";
            std::exit(0);
        }
        else
        {
            throw std::runtime_error("unknown argument: " + argument);
        }
    }

    if (options.configPath.empty())
    {
        throw std::runtime_error("--config is required");
    }
    if (options.outputPath.empty())
    {
        throw std::runtime_error("--output is required");
    }
    if (!std::isfinite(options.voxelMm) || options.voxelMm <= 0.0)
    {
        throw std::runtime_error("--voxel-mm must be finite and positive");
    }
    const int operationSetCount = (options.executeCleanup ? 1 : 0)
        + (options.executeR2Topology ? 1 : 0)
        + (options.executeR2Boundary ? 1 : 0)
        + (options.executeR2EvidenceGuard ? 1 : 0)
        + (options.classifyR3NonManifoldPatterns ? 1 : 0);
    if (operationSetCount > 1)
    {
        throw std::runtime_error("mesh repair operation sets are mutually exclusive");
    }
    if ((options.executeR2Topology
         || options.executeR2Boundary
         || options.executeR2EvidenceGuard)
        && (!std::isfinite(options.weldToleranceMm) || options.weldToleranceMm <= 0.0))
    {
        throw std::runtime_error("R2-02/R2-03 requires a finite positive --weld-tolerance-mm");
    }
    if (options.executeR2Boundary || options.executeR2EvidenceGuard)
    {
        const bool finiteBudgets = std::isfinite(options.maxBoundaryLoopDiameterMm)
            && std::isfinite(options.maxBoundaryLoopPerimeterMm)
            && std::isfinite(options.maxBoundaryPlanarityErrorMm)
            && std::isfinite(options.maxHoleAreaMm2)
            && std::isfinite(options.maxAffectedFaceRatio);
        if (options.maxBoundaryLoopEdges < 3U
            || !finiteBudgets
            || options.maxBoundaryLoopDiameterMm <= 0.0
            || options.maxBoundaryLoopPerimeterMm <= 0.0
            || options.maxBoundaryPlanarityErrorMm <= 0.0
            || options.maxHoleAreaMm2 <= 0.0
            || options.maxAffectedFaceRatio <= 0.0
            || options.maxAffectedFaceRatio > 1.0)
        {
            throw std::runtime_error("--execute-r2-03 requires explicit positive boundary budgets");
        }
    }
    return options;
}

Matrix4 IdentityMatrix()
{
    return {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0};
}

Matrix4 Multiply(const Matrix4& left, const Matrix4& right)
{
    Matrix4 result{};
    for (std::size_t row{0U}; row < 4U; ++row)
    {
        for (std::size_t column{0U}; column < 4U; ++column)
        {
            for (std::size_t inner{0U}; inner < 4U; ++inner)
            {
                result.at(row * 4U + column) +=
                    left.at(row * 4U + inner) * right.at(inner * 4U + column);
            }
        }
    }
    return result;
}

Matrix4 TranslationMatrix(const double x, const double y, const double z)
{
    Matrix4 result = IdentityMatrix();
    result.at(3U) = x;
    result.at(7U) = y;
    result.at(11U) = z;
    return result;
}

Matrix4 ScaleMatrix(const double x, const double y, const double z)
{
    Matrix4 result = IdentityMatrix();
    result.at(0U) = x;
    result.at(5U) = y;
    result.at(10U) = z;
    return result;
}

Matrix4 RotationX(const double radians)
{
    Matrix4 result = IdentityMatrix();
    result.at(5U) = std::cos(radians);
    result.at(6U) = -std::sin(radians);
    result.at(9U) = std::sin(radians);
    result.at(10U) = std::cos(radians);
    return result;
}

Matrix4 RotationY(const double radians)
{
    Matrix4 result = IdentityMatrix();
    result.at(0U) = std::cos(radians);
    result.at(2U) = std::sin(radians);
    result.at(8U) = -std::sin(radians);
    result.at(10U) = std::cos(radians);
    return result;
}

Matrix4 RotationZ(const double radians)
{
    Matrix4 result = IdentityMatrix();
    result.at(0U) = std::cos(radians);
    result.at(1U) = -std::sin(radians);
    result.at(4U) = std::sin(radians);
    result.at(5U) = std::cos(radians);
    return result;
}

double UnitScaleToMillimeters(const std::string& unit)
{
    if (unit == "mm")
    {
        return 1.0;
    }
    if (unit == "cm")
    {
        return 10.0;
    }
    if (unit == "m")
    {
        return 1000.0;
    }
    if (unit == "inch" || unit == "in")
    {
        return 25.4;
    }
    throw std::runtime_error("unsupported modelTransform.unit: " + unit);
}

Matrix4 RotationFromDegrees(const std::array<double, 3>& rotationDegrees)
{
    constexpr double pi = 3.14159265358979323846;
    const Matrix4 x = RotationX(rotationDegrees.at(0U) * pi / 180.0);
    const Matrix4 y = RotationY(rotationDegrees.at(1U) * pi / 180.0);
    const Matrix4 z = RotationZ(rotationDegrees.at(2U) * pi / 180.0);
    return Multiply(z, Multiply(y, x));
}

std::array<double, 3> AutoOrientationDegrees(const std::string& name)
{
    if (name == "identity")
    {
        return {0.0, 0.0, 0.0};
    }
    if (name == "rotate_x_90")
    {
        return {90.0, 0.0, 0.0};
    }
    if (name == "rotate_x_minus_90")
    {
        return {-90.0, 0.0, 0.0};
    }
    if (name == "rotate_y_90")
    {
        return {0.0, 90.0, 0.0};
    }
    if (name == "rotate_y_minus_90")
    {
        return {0.0, -90.0, 0.0};
    }
    throw std::runtime_error("unsupported auto orientation: " + name);
}

slicer_core::Vec3 TransformPoint(const Matrix4& matrix, const slicer_core::Vec3& point)
{
    return {
        matrix.at(0U) * point.x + matrix.at(1U) * point.y
            + matrix.at(2U) * point.z + matrix.at(3U),
        matrix.at(4U) * point.x + matrix.at(5U) * point.y
            + matrix.at(6U) * point.z + matrix.at(7U),
        matrix.at(8U) * point.x + matrix.at(9U) * point.y
            + matrix.at(10U) * point.z + matrix.at(11U)};
}

double RotatedMinimumZ(
    const slicer_core::BoundingBox& bounds,
    const Matrix4& rotation)
{
    double minimum = std::numeric_limits<double>::max();
    for (const double x : {bounds.min.x, bounds.max.x})
    {
        for (const double y : {bounds.min.y, bounds.max.y})
        {
            for (const double z : {bounds.min.z, bounds.max.z})
            {
                minimum = std::min(
                    minimum,
                    TransformPoint(rotation, {x, y, z}).z);
            }
        }
    }
    return minimum;
}

Matrix4 BuildFinalTransform(
    const slicer_core::SliceConfig& config,
    const slicer_core::ModelReport& scene)
{
    const double unitScale = UnitScaleToMillimeters(config.transform.unit);
    const Matrix4 configuredScale = ScaleMatrix(
        unitScale * config.transform.scale.at(0U),
        unitScale * config.transform.scale.at(1U),
        unitScale * config.transform.scale.at(2U));
    const Matrix4 configuredRotation = RotationFromDegrees(config.transform.rotation_deg);
    const Matrix4 configuredTranslation = TranslationMatrix(
        config.transform.translation_mm.at(0U),
        config.transform.translation_mm.at(1U),
        config.transform.translation_mm.at(2U));
    const Matrix4 configured = Multiply(
        configuredTranslation,
        Multiply(configuredRotation, configuredScale));

    const Matrix4 autoRotation = RotationFromDegrees(
        AutoOrientationDegrees(scene.auto_orient.selected_orientation));
    if (!scene.auto_orient.applied)
    {
        return configured;
    }
    const double minimumZ = RotatedMinimumZ(
        scene.auto_orient.original_bbox_mm,
        autoRotation);
    return Multiply(
        TranslationMatrix(0.0, 0.0, -minimumZ),
        Multiply(autoRotation, configured));
}

std::string ReadBinaryFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("failed to read input file: " + path.string());
    }
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

void WriteReport(
    const std::filesystem::path& outputPath,
    const slicer_core::Json& report)
{
    if (!outputPath.parent_path().empty())
    {
        std::filesystem::create_directories(outputPath.parent_path());
    }
    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error("failed to write preflight report: " + outputPath.string());
    }
    output << report.dump(2) << '\n';
}

int RunPreflight(const Options& options)
{
    const slicer_core::OpenVdbStatus openVdbStatus = slicer_core::GetOpenVdbStatus();
    if (options.requireOpenVdbOff && openVdbStatus.compiled_with_openvdb)
    {
        throw std::runtime_error("pre-repair baseline requires the default OpenVDB OFF build lane");
    }
    const slicer_core::SliceConfig config =
        slicer_core::load_slice_config(options.configPath);
    const std::filesystem::path configDirectory = options.configPath.parent_path().empty()
        ? std::filesystem::current_path()
        : options.configPath.parent_path();
    const slicer_core::ModelReport scene =
        slicer_core::load_model_report(config, configDirectory);
    const slicer_core::AdaptedTriangleMesh adapted =
        slicer_core::AdaptSceneModelToTriangleMesh(scene);

    slicer_core::MeshRepairInputSummary input;
    input.sourcePath = options.sourceId.empty()
        ? scene.model_path.generic_string()
        : options.sourceId;
    input.inputFormat = scene.format;
    input.finalTransform = BuildFinalTransform(config, scene);
    const std::string sourceHash = slicer_core::ComputeMeshRepairSha256(
        ReadBinaryFile(scene.model_path));
    slicer_core::MeshRobustnessOptions robustnessOptions;
    robustnessOptions.tolerance = slicer_core::MakeMeshScaleTolerance(
        adapted.mesh.bbox_mm,
        options.voxelMm);
    robustnessOptions.max_self_intersection_pairs =
        options.maxSelfIntersectionPairs;
    robustnessOptions.max_triangle_pair_checks =
        options.maxTrianglePairChecks;

    slicer_core::MeshRepairResult result;
    std::size_t candidateTriangleCount = adapted.mesh.triangles.size();
    std::size_t candidateVertexCount = adapted.mesh.vertices.size();
    if (options.executeCleanup
        || options.executeR2Topology
        || options.executeR2Boundary
        || options.executeR2EvidenceGuard)
    {
        slicer_core::MeshRepairCleanupRequest request;
        request.mesh = &adapted;
        request.input = input;
        request.options.enabled = true;
        request.options.mode = "repair_then_strict";
        const bool executeGuardedTopology =
            options.executeR2Topology
            || options.executeR2Boundary
            || options.executeR2EvidenceGuard;
        const bool executeBoundary =
            options.executeR2Boundary || options.executeR2EvidenceGuard;
        request.options.allowVertexWeld = executeGuardedTopology;
        request.options.weldToleranceMm = executeGuardedTopology
            ? options.weldToleranceMm
            : 0.0;
        request.options.allowWindingRepair = executeGuardedTopology;
        request.options.allowBoundaryFill = executeBoundary;
        request.options.maxBoundaryLoopEdges = options.maxBoundaryLoopEdges;
        request.options.maxBoundaryLoopDiameterMm = options.maxBoundaryLoopDiameterMm;
        request.options.maxBoundaryLoopPerimeterMm = options.maxBoundaryLoopPerimeterMm;
        request.options.maxBoundaryPlanarityErrorMm =
            options.maxBoundaryPlanarityErrorMm;
        request.options.maxHoleAreaMm2 = options.maxHoleAreaMm2;
        request.options.maxAffectedFaceRatio = options.maxAffectedFaceRatio;
        request.options.allowNewFaces = executeBoundary;
        request.options.newFaceAttributePolicy = executeBoundary
            ? "inherit_uniform_material_no_uv"
            : "reject";
        request.options.validatePostRepairEvidence = options.executeR2EvidenceGuard;
        request.sourceHash = sourceHash;
        request.robustnessOptions = robustnessOptions;
        slicer_core::MeshRepairCleanupResult cleanup =
            slicer_core::ExecuteMeshRepairCleanup(request);
        candidateTriangleCount = cleanup.candidate.mesh.triangles.size();
        candidateVertexCount = cleanup.candidate.mesh.vertices.size();
        result = std::move(cleanup.evidence);
    }
    else
    {
        slicer_core::MeshRepairPreflightRequest request;
        request.mesh = &adapted;
        request.input = input;
        request.options.enabled = false;
        request.options.mode = "strict_closed";
        request.options.classifyNonManifoldPatterns =
            options.classifyR3NonManifoldPatterns;
        request.sourceHash = sourceHash;
        request.robustnessOptions = robustnessOptions;
        result = slicer_core::EvaluateMeshRepairPreflight(request);
    }
    WriteReport(options.outputPath, slicer_core::BuildMeshRepairReport(result));

    std::cout
        << "mesh_repair_preflight: evidence collected\n"
        << "  source: " << input.sourcePath << '\n'
        << "  operationSet: "
        << (options.classifyR3NonManifoldPatterns
                ? "r3_non_manifold_pattern_classifier"
                : (options.executeR2EvidenceGuard
                ? "r2_post_strict_attribute_guard"
                : (options.executeR2Boundary
                ? "r2_boundary_loop_repair"
                : (options.executeR2Topology
                        ? "r2_vertex_weld_winding"
                        : (options.executeCleanup ? "r2_cleanup" : "preflight")))))
        << '\n'
        << "  status: " << slicer_core::MeshRepairStatusName(result.status) << '\n'
        << "  vertices: " << result.input.vertexCount << '\n'
        << "  triangles: " << result.input.triangleCount << '\n'
        << "  candidateVertices: " << candidateVertexCount << '\n'
        << "  candidateTriangles: " << candidateTriangleCount << '\n'
        << "  components: " << result.input.componentCount << '\n'
        << "  productionOutputWritten: false\n"
        << "  report: " << options.outputPath.generic_string() << '\n';
    return 0;
}

}  // namespace

int main(const int argc, char** argv)
{
    try
    {
        return RunPreflight(ParseOptions(argc, argv));
    }
    catch (const std::exception& error)
    {
        std::cerr << "mesh_repair_preflight error: " << error.what() << '\n';
        return 1;
    }
}
