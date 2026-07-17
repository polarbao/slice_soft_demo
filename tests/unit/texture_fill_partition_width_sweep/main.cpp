#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/json_value.h"
#include "slicer_core/materials/texture_application/GlobalTextureFillPartitionService.h"
#include "slicer_core/materials/texture_application/LegacyCpuGlobalDistanceBackend.h"
#include "slicer_core/materials/texture_application/OpenVdbTextureFillConformanceBackend.h"
#include "slicer_core/reports/TextureFillPartitionReport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool HasIssueCode(
    const std::vector<slicer_core::ValidationIssue>& issues,
    const std::string& code)
{
    for (const slicer_core::ValidationIssue& issue : issues)
    {
        if (issue.code == code)
        {
            return true;
        }
    }
    return false;
}

slicer_core::TextureFillPartitionGridSpec MakeGrid(
    const int width,
    const int height,
    const int depth,
    const double spacing)
{
    slicer_core::TextureFillPartitionGridSpec grid;
    grid.width = width;
    grid.height = height;
    grid.depth = depth;
    grid.originXMm = -0.1;
    grid.originYMm = -0.1;
    grid.originZMm = -0.1;
    grid.spacingXMm = spacing;
    grid.spacingYMm = spacing;
    grid.spacingZMm = spacing;
    return grid;
}

slicer_core::GlobalTextureFillPartitionRequest MakeRequest(
    const slicer_core::TriangleMeshData& mesh,
    const slicer_core::TextureFillPartitionGridSpec& grid,
    const double widthMm)
{
    slicer_core::GlobalTextureFillPartitionRequest request;
    request.mesh = &mesh;
    request.grid = grid;
    request.options.requestedWidthMm = widthMm;
    return request;
}

class DeterministicSweepBackend final
    : public slicer_core::IGlobalTextureFillPartitionBackend
{
public:
    enum class FixtureMode
    {
        Normal,
        FailIntermediate,
        NonMonotonic,
        ModelChanged,
    };

    explicit DeterministicSweepBackend(
        const FixtureMode mode = FixtureMode::Normal)
        : m_mode(mode)
    {
    }

    slicer_core::GlobalTextureFillPartitionCandidate Evaluate(
        const slicer_core::GlobalTextureFillPartitionRequest& request) const override
    {
        slicer_core::GlobalTextureFillPartitionCandidate candidate;
        candidate.available = true;
        candidate.backend = "deterministic_sweep_fixture";
        candidate.backendRole = "conformance_candidate";
        candidate.modelMask.grid = request.grid;
        candidate.textureSurfaceMask.grid = request.grid;
        candidate.modelFillMask.grid = request.grid;
        if (m_mode == FixtureMode::FailIntermediate
            && std::abs(request.options.requestedWidthMm - 0.25) < 1.0e-9)
        {
            candidate.blocked = true;
            candidate.issues.push_back(slicer_core::MakeValidationIssue(
                "E_FIXTURE_SAMPLE_BLOCKED",
                slicer_core::ValidationSeverity::Error,
                "fixture blocked the intermediate sample"));
            return candidate;
        }

        const std::size_t voxelCount = static_cast<std::size_t>(
            request.grid.width * request.grid.height * request.grid.depth);
        candidate.modelMask.values.assign(voxelCount, 1U);
        candidate.textureSurfaceMask.values.assign(voxelCount, 0U);
        candidate.modelFillMask.values.assign(voxelCount, 1U);
        const double width = request.options.requestedWidthMm;
        std::size_t textureCount{1U};
        if (width >= 0.15)
        {
            textureCount = 2U;
        }
        if (width >= 0.25)
        {
            textureCount = 3U;
        }
        if (width >= 0.35)
        {
            textureCount = voxelCount;
        }
        if (m_mode == FixtureMode::NonMonotonic
            && std::abs(width - 0.25) < 1.0e-9)
        {
            textureCount = 1U;
        }
        for (std::size_t index{0U}; index < textureCount; ++index)
        {
            candidate.textureSurfaceMask.values.at(index) = 1U;
            candidate.modelFillMask.values.at(index) = 0U;
        }
        if (m_mode == FixtureMode::ModelChanged
            && std::abs(width - 0.25) < 1.0e-9)
        {
            candidate.modelMask.values.back() = 0U;
            candidate.textureSurfaceMask.values.back() = 0U;
            candidate.modelFillMask.values.back() = 0U;
        }
        candidate.widthMetrics.classificationResolutionMm = 0.05;
        candidate.widthMetrics.epsilonMm = 1.0e-9;
        candidate.widthMetrics.effectiveMinimumWidthMm = 0.10;
        candidate.widthMetrics.effectiveWidthMm = std::min(width, 0.40);
        candidate.widthMetrics.maxInteriorDistanceMm = 0.40;
        candidate.widthMetrics.allTextureThresholdMm = 0.40;
        candidate.widthMetrics.allTexture = width >= 0.40 - 1.0e-9;
        candidate.performance.totalCoreMs = 1.0;
        return candidate;
    }

private:
    FixtureMode m_mode{FixtureMode::Normal};
};

slicer_core::TriangleMeshData MakeOctahedronMesh()
{
    slicer_core::TriangleMeshData mesh;
    mesh.source_name = "generated-octahedron";
    mesh.vertices = {
        {0.5, 0.5, 1.0},
        {0.5, 0.5, 0.0},
        {1.0, 0.5, 0.5},
        {0.5, 1.0, 0.5},
        {0.0, 0.5, 0.5},
        {0.5, 0.0, 0.5},
    };
    mesh.triangles = {
        {0, 2, 3},
        {0, 3, 4},
        {0, 4, 5},
        {0, 5, 2},
        {1, 3, 2},
        {1, 4, 3},
        {1, 5, 4},
        {1, 2, 5},
    };
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {1.0, 1.0, 1.0};
    return mesh;
}

void TranslateMesh(
    slicer_core::TriangleMeshData& mesh,
    const double x,
    const double y,
    const double z)
{
    for (slicer_core::Vec3& vertex : mesh.vertices)
    {
        vertex.x += x;
        vertex.y += y;
        vertex.z += z;
    }
    mesh.bbox_mm.min.x += x;
    mesh.bbox_mm.min.y += y;
    mesh.bbox_mm.min.z += z;
    mesh.bbox_mm.max.x += x;
    mesh.bbox_mm.max.y += y;
    mesh.bbox_mm.max.z += z;
}

void AppendReversedMesh(
    slicer_core::TriangleMeshData& destination,
    const slicer_core::TriangleMeshData& source)
{
    const int vertexOffset = static_cast<int>(destination.vertices.size());
    destination.vertices.insert(
        destination.vertices.end(),
        source.vertices.begin(),
        source.vertices.end());
    for (std::array<int, 3> triangle : source.triangles)
    {
        for (int& vertexIndex : triangle)
        {
            vertexIndex += vertexOffset;
        }
        std::swap(triangle.at(1), triangle.at(2));
        destination.triangles.push_back(triangle);
    }
}

slicer_core::TriangleMeshData MakeClosedCavityMesh()
{
    slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    mesh.source_name = "generated-closed-cavity";
    slicer_core::TriangleMeshData inner =
        slicer_core::MakeGeneratedBoxMesh(0.4, 0.4, 0.4);
    TranslateMesh(inner, 0.3, 0.3, 0.3);
    AppendReversedMesh(mesh, inner);
    return mesh;
}

slicer_core::Json LoadGolden(const std::string& fileName)
{
    const std::filesystem::path path =
        std::filesystem::path{SLICESOFT_SOURCE_DIR}
        / "tests" / "golden" / "expected" / fileName;
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("failed to open golden: " + path.string());
    }
    return slicer_core::Json::parse(input);
}

slicer_core::Json MakeStableSweepProjection(
    const slicer_core::TextureFillPartitionWidthSweepResult& sweep)
{
    const slicer_core::Json summary =
        slicer_core::BuildTextureFillPartitionWidthSweepSummary(sweep);
    return slicer_core::Json::object({
        {"backend", summary.at("backend")},
        {"status", summary.at("status")},
        {"minimumWidthMm", summary.at("minimumWidthMm")},
        {"maximumWidthMm", summary.at("maximumWidthMm")},
        {"widthStepMm", summary.at("widthStepMm")},
        {"sampleCount", summary.at("sampleCount")},
        {"monotonic", summary.at("monotonic")},
        {"endpoint", summary.at("endpoint")},
        {"samples", summary.at("samples")},
    });
}

bool RepresentativeSweepMatchesGolden()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    const slicer_core::TextureFillPartitionGridSpec grid =
        MakeGrid(4, 1, 1, 0.05);
    DeterministicSweepBackend backend;
    const slicer_core::GlobalTextureFillPartitionService service(&backend);
    const slicer_core::TextureFillPartitionWidthSweepResult sweep =
        service.EvaluateWidthSweep(MakeRequest(mesh, grid, 0.20));
    const slicer_core::Json actual = MakeStableSweepProjection(sweep);
    const slicer_core::Json expected = LoadGolden(
        "12e_width_sweep_summary.json");
    return ExpectTrue(sweep.monotonicPass, "representative sweep is monotonic")
        && ExpectTrue(sweep.endpointPass, "representative sweep reaches endpoint")
        && ExpectTrue(sweep.samples.size() == 5U, "representative sweep has five samples")
        && ExpectTrue(
            actual.dump(2) == expected.dump(2),
            "representative sweep matches deterministic golden");
}

bool ClosedBoxCpuSweepIsMonotonic()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    slicer_core::LegacyCpuGlobalDistanceBackend backend;
    const slicer_core::GlobalTextureFillPartitionService service(&backend);
    const slicer_core::TextureFillPartitionWidthSweepResult sweep =
        service.EvaluateWidthSweep(MakeRequest(mesh, MakeGrid(12, 12, 12, 0.1), 0.20));
    return ExpectTrue(sweep.status == "diagnostic", "closed box sweep is diagnostic")
        && ExpectTrue(sweep.monotonicPass, "closed box texture/fill counts are monotonic")
        && ExpectTrue(sweep.endpointPass, "closed box reaches all-texture endpoint")
        && ExpectTrue(
            sweep.samples.back().stats.modelFillVoxels == 0U,
            "closed box endpoint has no model fill");
}

bool ThinWallDeduplicatesToOneSample()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 0.15);
    slicer_core::LegacyCpuGlobalDistanceBackend backend;
    const slicer_core::GlobalTextureFillPartitionService service(&backend);
    const slicer_core::TextureFillPartitionWidthSweepResult sweep =
        service.EvaluateWidthSweep(MakeRequest(mesh, MakeGrid(12, 12, 5, 0.1), 0.20));
    return ExpectTrue(sweep.monotonicPass, "thin wall sweep is monotonic")
        && ExpectTrue(sweep.endpointPass, "thin wall reaches endpoint")
        && ExpectTrue(sweep.samples.size() == 1U, "thin wall deduplicates to one sample");
}

bool NonStepGridMinimumProducesOrderedQuantizedSweep()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 0.12);
    slicer_core::LegacyCpuGlobalDistanceBackend backend;
    const slicer_core::GlobalTextureFillPartitionService service(&backend);
    const auto sweep = service.EvaluateWidthSweep(
        MakeRequest(mesh, MakeGrid(20, 20, 5, 0.057), 0.20));
    if (!ExpectTrue(!sweep.samples.empty(), "non-step grid produces sweep samples"))
    {
        return false;
    }
    for (std::size_t index{1U}; index < sweep.samples.size(); ++index)
    {
        if (!ExpectTrue(
                sweep.samples.at(index).requestedWidthMm
                    > sweep.samples.at(index - 1U).requestedWidthMm,
                "non-step grid widths remain strictly ordered"))
        {
            return false;
        }
    }
    const double endpointSteps = sweep.maximumWidthMm / 0.01;
    return ExpectTrue(sweep.monotonicPass, "non-step grid sweep is monotonic")
        && ExpectTrue(sweep.endpointPass, "non-step grid reaches endpoint")
        && ExpectTrue(
            std::abs(endpointSteps - std::round(endpointSteps)) < 1.0e-8,
            "dynamic endpoint is quantized to 0.01 mm");
}

bool UnavailableBackendDoesNotInventSamples()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    const slicer_core::GlobalTextureFillPartitionService service;
    const slicer_core::TextureFillPartitionWidthSweepResult sweep =
        service.EvaluateWidthSweep(MakeRequest(mesh, MakeGrid(4, 1, 1, 0.05), 0.20));
    return ExpectTrue(!sweep.available, "unavailable sweep remains unavailable")
        && ExpectTrue(sweep.samples.empty(), "unavailable sweep has no invented samples")
        && ExpectTrue(
            HasIssueCode(sweep.issues, "E_12E_WIDTH_SWEEP_SAMPLE_FAILED"),
            "unavailable sweep uses stable sample failure issue");
}

bool BlockedIntermediateStopsIncompleteSweep()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    DeterministicSweepBackend backend(
        DeterministicSweepBackend::FixtureMode::FailIntermediate);
    const slicer_core::GlobalTextureFillPartitionService service(&backend);
    const slicer_core::TextureFillPartitionWidthSweepResult sweep =
        service.EvaluateWidthSweep(MakeRequest(mesh, MakeGrid(4, 1, 1, 0.05), 0.20));
    return ExpectTrue(sweep.status == "fail", "blocked intermediate fails sweep")
        && ExpectTrue(!sweep.monotonicPass, "incomplete sweep is not monotonic pass")
        && ExpectTrue(!sweep.endpointPass, "incomplete sweep has no endpoint pass")
        && ExpectTrue(
            HasIssueCode(sweep.issues, "E_12E_WIDTH_SWEEP_SAMPLE_FAILED"),
            "blocked intermediate uses stable sample failure issue");
}

bool NonMonotonicSamplesUseStableIssues()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    DeterministicSweepBackend backend(
        DeterministicSweepBackend::FixtureMode::NonMonotonic);
    const slicer_core::GlobalTextureFillPartitionService service(&backend);
    const auto sweep = service.EvaluateWidthSweep(
        MakeRequest(mesh, MakeGrid(4, 1, 1, 0.05), 0.20));
    return ExpectTrue(!sweep.monotonicPass, "non-monotonic fixture fails validation")
        && ExpectTrue(
            HasIssueCode(
                sweep.issues,
                "E_12E_WIDTH_SWEEP_TEXTURE_NON_MONOTONIC"),
            "texture decrease uses stable issue")
        && ExpectTrue(
            HasIssueCode(
                sweep.issues,
                "E_12E_WIDTH_SWEEP_FILL_NON_MONOTONIC"),
            "fill increase uses stable issue");
}

bool ModelOccupancyChangeUsesStableIssue()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    DeterministicSweepBackend backend(
        DeterministicSweepBackend::FixtureMode::ModelChanged);
    const slicer_core::GlobalTextureFillPartitionService service(&backend);
    const auto sweep = service.EvaluateWidthSweep(
        MakeRequest(mesh, MakeGrid(4, 1, 1, 0.05), 0.20));
    return ExpectTrue(!sweep.monotonicPass, "model-change fixture fails validation")
        && ExpectTrue(
            HasIssueCode(sweep.issues, "E_12E_WIDTH_SWEEP_MODEL_CHANGED"),
            "model occupancy change uses stable issue");
}

bool SlopedBodyCpuSweepIsMonotonic()
{
    const slicer_core::TriangleMeshData mesh = MakeOctahedronMesh();
    slicer_core::LegacyCpuGlobalDistanceBackend backend;
    const slicer_core::GlobalTextureFillPartitionService service(&backend);
    const auto sweep = service.EvaluateWidthSweep(
        MakeRequest(mesh, MakeGrid(12, 12, 12, 0.1), 0.20));
    return ExpectTrue(sweep.monotonicPass, "sloped body sweep is monotonic")
        && ExpectTrue(sweep.endpointPass, "sloped body reaches endpoint");
}

bool ClosedCavityCpuSweepIsMonotonic()
{
    const slicer_core::TriangleMeshData mesh = MakeClosedCavityMesh();
    slicer_core::LegacyCpuGlobalDistanceBackend backend;
    const slicer_core::GlobalTextureFillPartitionService service(&backend);
    const auto sweep = service.EvaluateWidthSweep(
        MakeRequest(mesh, MakeGrid(12, 12, 12, 0.1), 0.20));
    return ExpectTrue(sweep.monotonicPass, "closed cavity sweep is monotonic")
        && ExpectTrue(sweep.endpointPass, "closed cavity reaches endpoint");
}

bool FullStepScanRespectsMaximumSamples()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    DeterministicSweepBackend backend;
    const slicer_core::GlobalTextureFillPartitionService service(&backend);
    slicer_core::TextureFillPartitionWidthSweepOptions options;
    options.fullStepScan = true;
    options.maxSamples = 4U;
    const slicer_core::TextureFillPartitionWidthSweepResult sweep =
        service.EvaluateWidthSweep(
            MakeRequest(mesh, MakeGrid(4, 1, 1, 0.05), 0.20),
            options);
    return ExpectTrue(sweep.status == "fail", "oversized full scan fails honestly")
        && ExpectTrue(sweep.samples.empty(), "oversized full scan does not run partial samples")
        && ExpectTrue(
            HasIssueCode(sweep.issues, "E_12E_WIDTH_SWEEP_SAMPLE_FAILED"),
            "oversized full scan uses stable issue");
}

bool RepeatedSweepIsDeterministic()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    DeterministicSweepBackend backend;
    const slicer_core::GlobalTextureFillPartitionService service(&backend);
    const auto request = MakeRequest(mesh, MakeGrid(4, 1, 1, 0.05), 0.20);
    const auto first = service.EvaluateWidthSweep(request);
    const auto second = service.EvaluateWidthSweep(request);
    return ExpectTrue(
        MakeStableSweepProjection(first).dump(2)
            == MakeStableSweepProjection(second).dump(2),
        "repeated sweep summary is deterministic");
}

bool OpenVdbBuildLaneContractIsStable()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    slicer_core::OpenVdbTextureFillConformanceBackend backend;
    const slicer_core::GlobalTextureFillPartitionService service(&backend);
    const auto sweep = service.EvaluateWidthSweep(
        MakeRequest(mesh, MakeGrid(12, 12, 12, 0.1), 0.20));
    const slicer_core::OpenVdbStatus status = slicer_core::GetOpenVdbStatus();
    if (!status.compiled_with_openvdb || !status.runtime_available)
    {
        return ExpectTrue(!sweep.available, "OpenVDB OFF sweep is unavailable")
            && ExpectTrue(sweep.samples.empty(), "OpenVDB OFF sweep has no samples");
    }
    return ExpectTrue(sweep.monotonicPass, "OpenVDB ON sweep is monotonic")
        && ExpectTrue(sweep.endpointPass, "OpenVDB ON sweep reaches endpoint");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"representative_sweep_matches_golden", RepresentativeSweepMatchesGolden},
        {"closed_box_cpu_sweep_is_monotonic", ClosedBoxCpuSweepIsMonotonic},
        {"thin_wall_deduplicates_to_one_sample", ThinWallDeduplicatesToOneSample},
        {"non_step_grid_minimum_produces_ordered_quantized_sweep", NonStepGridMinimumProducesOrderedQuantizedSweep},
        {"unavailable_backend_does_not_invent_samples", UnavailableBackendDoesNotInventSamples},
        {"blocked_intermediate_stops_incomplete_sweep", BlockedIntermediateStopsIncompleteSweep},
        {"non_monotonic_samples_use_stable_issues", NonMonotonicSamplesUseStableIssues},
        {"model_occupancy_change_uses_stable_issue", ModelOccupancyChangeUsesStableIssue},
        {"sloped_body_cpu_sweep_is_monotonic", SlopedBodyCpuSweepIsMonotonic},
        {"closed_cavity_cpu_sweep_is_monotonic", ClosedCavityCpuSweepIsMonotonic},
        {"full_step_scan_respects_maximum_samples", FullStepScanRespectsMaximumSamples},
        {"repeated_sweep_is_deterministic", RepeatedSweepIsDeterministic},
        {"openvdb_build_lane_contract_is_stable", OpenVdbBuildLaneContractIsStable},
    };
    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        if (!test.second())
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }
    std::cout << "Texture/fill partition width sweep unit tests complete.\n";
    return 0;
}
