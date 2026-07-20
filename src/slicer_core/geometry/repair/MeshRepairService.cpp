#include "slicer_core/geometry/repair/MeshRepairService.h"

#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshRepairPreflight.h"
#include "slicer_core/geometry/repair/MeshRepairTopologyOperations.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

using Clock = std::chrono::steady_clock;
using FaceKey = std::array<int, 3>;

struct PlannedTriangle
{
    std::size_t inputIndex{0U};
    std::size_t sourceIndex{0U};
    bool degenerate{false};
    std::optional<std::size_t> duplicateOfInputIndex;
};

struct CleanupPlan
{
    std::vector<PlannedTriangle> triangles;
    bool attributeConflict{false};
    std::uint64_t materialConflicts{0U};
    std::uint64_t uvConflicts{0U};
};

double ElapsedMilliseconds(const Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

FaceKey MakeFaceKey(const std::array<int, 3>& triangle)
{
    FaceKey key = triangle;
    std::sort(key.begin(), key.end());
    return key;
}

bool FindSameOrientationShift(
    const std::array<int, 3>& reference,
    const std::array<int, 3>& candidate,
    std::size_t& shift)
{
    for (std::size_t candidateShift{0U}; candidateShift < 3U; ++candidateShift)
    {
        bool matches{true};
        for (std::size_t corner{0U}; corner < 3U; ++corner)
        {
            if (candidate.at(corner) != reference.at((corner + candidateShift) % 3U))
            {
                matches = false;
                break;
            }
        }
        if (matches)
        {
            shift = candidateShift;
            return true;
        }
    }
    return false;
}

bool AttributesMatch(
    const SurfaceTriangleAttributes& reference,
    const SurfaceTriangleAttributes& candidate,
    const std::size_t candidateShift)
{
    if (reference.has_uv != candidate.has_uv
        || reference.material_name != candidate.material_name)
    {
        return false;
    }
    if (!reference.has_uv)
    {
        return true;
    }
    for (std::size_t corner{0U}; corner < 3U; ++corner)
    {
        const TexCoord& left = candidate.uv.at(corner);
        const TexCoord& right = reference.uv.at((corner + candidateShift) % 3U);
        if (left.u != right.u || left.v != right.v)
        {
            return false;
        }
    }
    return true;
}

void RecordAttributeConflicts(
    const SurfaceTriangleAttributes& reference,
    const SurfaceTriangleAttributes& candidate,
    const std::size_t candidateShift,
    CleanupPlan& plan)
{
    if (reference.material_name != candidate.material_name)
    {
        ++plan.materialConflicts;
    }
    bool uvConflict = reference.has_uv != candidate.has_uv;
    if (reference.has_uv && candidate.has_uv)
    {
        for (std::size_t corner{0U}; corner < 3U; ++corner)
        {
            const TexCoord& left = candidate.uv.at(corner);
            const TexCoord& right = reference.uv.at((corner + candidateShift) % 3U);
            if (left.u != right.u || left.v != right.v)
            {
                uvConflict = true;
                break;
            }
        }
    }
    if (uvConflict)
    {
        ++plan.uvConflicts;
    }
}

double TriangleAreaMm2(
    const TriangleMeshData& mesh,
    const std::array<int, 3>& triangle)
{
    const Vec3& a = mesh.vertices.at(static_cast<std::size_t>(triangle.at(0U)));
    const Vec3& b = mesh.vertices.at(static_cast<std::size_t>(triangle.at(1U)));
    const Vec3& c = mesh.vertices.at(static_cast<std::size_t>(triangle.at(2U)));
    const double abX = b.x - a.x;
    const double abY = b.y - a.y;
    const double abZ = b.z - a.z;
    const double acX = c.x - a.x;
    const double acY = c.y - a.y;
    const double acZ = c.z - a.z;
    const double crossX = abY * acZ - abZ * acY;
    const double crossY = abZ * acX - abX * acZ;
    const double crossZ = abX * acY - abY * acX;
    return 0.5 * std::sqrt(crossX * crossX + crossY * crossY + crossZ * crossZ);
}

bool IsDegenerate(
    const TriangleMeshData& mesh,
    const std::array<int, 3>& triangle,
    const double areaToleranceMm2)
{
    if (triangle.at(0U) == triangle.at(1U)
        || triangle.at(1U) == triangle.at(2U)
        || triangle.at(0U) == triangle.at(2U))
    {
        return true;
    }
    return TriangleAreaMm2(mesh, triangle) <= areaToleranceMm2;
}

void ValidateRequest(const MeshRepairCleanupRequest& request)
{
    if (request.mesh == nullptr)
    {
        throw MeshRepairError(
            MeshRepairErrorCode::InputInvalid,
            "mesh repair cleanup requires an adapted mesh");
    }
    if (!request.options.enabled)
    {
        throw MeshRepairError(
            MeshRepairErrorCode::RepairNotEnabled,
            "mesh repair cleanup requires explicit enablement");
    }
    if (request.options.mode != "repair_then_strict")
    {
        throw MeshRepairError(
            MeshRepairErrorCode::InputInvalid,
            "mesh repair cleanup only supports repair_then_strict mode");
    }
    if (request.mesh->triangle_attributes.size() != request.mesh->mesh.triangles.size())
    {
        throw MeshRepairError(
            MeshRepairErrorCode::AttributeMismatch,
            "mesh repair cleanup requires one attribute record per triangle");
    }

    std::set<std::size_t> sourceIds;
    for (const SurfaceTriangleAttributes& attributes : request.mesh->triangle_attributes)
    {
        if (!sourceIds.insert(attributes.source_triangle_index).second)
        {
            throw MeshRepairError(
                MeshRepairErrorCode::InputInvalid,
                "mesh repair cleanup requires unique source triangle ids");
        }
    }
    for (const std::size_t sourceIndex : request.mesh->rejected_degenerate_source_triangle_indices)
    {
        if (!sourceIds.insert(sourceIndex).second)
        {
            throw MeshRepairError(
                MeshRepairErrorCode::InputInvalid,
                "rejected degenerate source id overlaps an accepted triangle");
        }
    }
}

CleanupPlan BuildCleanupPlan(
    const AdaptedTriangleMesh& mesh,
    const double areaToleranceMm2)
{
    CleanupPlan plan;
    std::map<FaceKey, std::vector<std::size_t>> retainedInputsByFace;
    for (std::size_t inputIndex{0U}; inputIndex < mesh.mesh.triangles.size(); ++inputIndex)
    {
        PlannedTriangle planned;
        planned.inputIndex = inputIndex;
        planned.sourceIndex = mesh.triangle_attributes.at(inputIndex).source_triangle_index;
        const std::array<int, 3>& triangle = mesh.mesh.triangles.at(inputIndex);
        planned.degenerate = IsDegenerate(mesh.mesh, triangle, areaToleranceMm2);
        if (!planned.degenerate)
        {
            std::vector<std::size_t>& retainedInputs = retainedInputsByFace[MakeFaceKey(triangle)];
            for (const std::size_t retainedInputIndex : retainedInputs)
            {
                std::size_t shift{0U};
                if (!FindSameOrientationShift(
                        mesh.mesh.triangles.at(retainedInputIndex),
                        triangle,
                        shift))
                {
                    continue;
                }
                if (!AttributesMatch(
                        mesh.triangle_attributes.at(retainedInputIndex),
                        mesh.triangle_attributes.at(inputIndex),
                        shift))
                {
                    plan.attributeConflict = true;
                    RecordAttributeConflicts(
                        mesh.triangle_attributes.at(retainedInputIndex),
                        mesh.triangle_attributes.at(inputIndex),
                        shift,
                        plan);
                }
                else
                {
                    planned.duplicateOfInputIndex = retainedInputIndex;
                }
                break;
            }
            if (!planned.duplicateOfInputIndex.has_value() && !plan.attributeConflict)
            {
                retainedInputs.push_back(inputIndex);
            }
        }
        plan.triangles.push_back(planned);
    }
    return plan;
}

MeshRepairResult BuildPreflightEvidence(const MeshRepairCleanupRequest& request)
{
    MeshRepairPreflightRequest preflightRequest;
    preflightRequest.mesh = request.mesh;
    preflightRequest.input = request.input;
    preflightRequest.options = request.options;
    preflightRequest.options.enabled = false;
    preflightRequest.options.mode = "strict_closed";
    preflightRequest.robustnessOptions = request.robustnessOptions;
    preflightRequest.sourceHash = request.sourceHash;
    MeshRepairResult evidence = EvaluateMeshRepairPreflight(preflightRequest);
    evidence.mode = request.options.mode;
    evidence.repairEnabled = request.options.enabled;
    evidence.options = request.options;
    evidence.hashes = ComputeMeshRepairPreHashes(*request.mesh, request.options);
    evidence.hashes.sourceHash = request.sourceHash;
    return evidence;
}

void AddRetainedMapping(
    MeshRepairResult& evidence,
    const std::size_t sourceIndex,
    const std::size_t outputIndex)
{
    MeshRepairTriangleMapping mapping;
    mapping.sourceTriangleIndex = sourceIndex;
    mapping.outputTriangleIndex = outputIndex;
    mapping.disposition = MeshRepairTriangleDisposition::Retained;
    evidence.sourceMappings.push_back(mapping);
}

void AddRemovedMapping(
    MeshRepairResult& evidence,
    const std::size_t sourceIndex,
    const MeshRepairTriangleDisposition disposition,
    const std::optional<std::size_t> retainedSourceIndex = std::nullopt)
{
    MeshRepairTriangleMapping mapping;
    mapping.sourceTriangleIndex = sourceIndex;
    mapping.disposition = disposition;
    if (retainedSourceIndex.has_value())
    {
        mapping.retainedSourceTriangleIndex = retainedSourceIndex.value();
    }
    evidence.sourceMappings.push_back(mapping);
}

MeshRepairOperation MakeDegenerateOperation(
    const std::uint64_t operationId,
    const std::size_t sourceIndex,
    const std::optional<std::size_t> inputIndex)
{
    MeshRepairOperation operation;
    operation.operationId = operationId;
    operation.type = MeshRepairOperationType::RemoveDegenerateTriangle;
    operation.reasonCode = "MESH_DEGENERATE_TRIANGLES";
    operation.inputElementIds = {sourceIndex};
    operation.parameters = Json::object({
        {"adapterFiltered", !inputIndex.has_value()},
        {"inputTriangleIndex", inputIndex.has_value() ? Json{inputIndex.value()} : Json{nullptr}},
    });
    operation.attributeDecision = MeshRepairAttributeDecision::Preserved;
    operation.affectedFaces = 1U;
    return operation;
}

MeshRepairOperation MakeDuplicateOperation(
    const std::uint64_t operationId,
    const PlannedTriangle& planned,
    const std::size_t retainedSourceIndex,
    const std::size_t retainedOutputIndex)
{
    MeshRepairOperation operation;
    operation.operationId = operationId;
    operation.type = MeshRepairOperationType::RemoveExactDuplicateFace;
    operation.reasonCode = "MESH_DUPLICATE_FACES";
    operation.inputElementIds = {planned.sourceIndex};
    operation.outputElementIds = {retainedOutputIndex};
    operation.parameters = Json::object({
        {"inputTriangleIndex", planned.inputIndex},
        {"retainedInputTriangleIndex", planned.duplicateOfInputIndex.value()},
        {"retainedSourceTriangleIndex", retainedSourceIndex},
        {"retainedOutputTriangleIndex", retainedOutputIndex},
    });
    operation.attributeDecision = MeshRepairAttributeDecision::Preserved;
    operation.affectedFaces = 1U;
    return operation;
}

void SortMappings(MeshRepairResult& evidence)
{
    std::sort(
        evidence.sourceMappings.begin(),
        evidence.sourceMappings.end(),
        [](const MeshRepairTriangleMapping& left, const MeshRepairTriangleMapping& right)
        {
            return left.sourceTriangleIndex < right.sourceTriangleIndex;
        });
}

MeshRepairCleanupResult BuildBlockedResult(
    const MeshRepairCleanupRequest& request,
    MeshRepairResult evidence,
    const CleanupPlan& plan)
{
    MeshRepairCleanupResult cleanup;
    cleanup.candidate = *request.mesh;
    evidence.status = MeshRepairStatus::ManualRepairRequired;
    evidence.repairAttempted = false;
    evidence.operations.clear();
    evidence.hashes.repairOperationHash = ComputeMeshRepairOperationsHash({});
    evidence.attributePreservation.status = "blocked_attribute_conflict";
    evidence.attributePreservation.materialConflicts = plan.materialConflicts;
    evidence.attributePreservation.uvConflicts = plan.uvConflicts;
    evidence.attributePreservation.pass = false;
    for (std::size_t index{0U}; index < request.mesh->triangle_attributes.size(); ++index)
    {
        AddRetainedMapping(
            evidence,
            request.mesh->triangle_attributes.at(index).source_triangle_index,
            index);
    }
    for (const std::size_t sourceIndex : request.mesh->rejected_degenerate_source_triangle_indices)
    {
        AddRemovedMapping(
            evidence,
            sourceIndex,
            MeshRepairTriangleDisposition::RemovedDegenerate);
    }
    SortMappings(evidence);
    cleanup.evidence = std::move(evidence);
    return cleanup;
}

void BuildCandidate(
    const MeshRepairCleanupRequest& request,
    const CleanupPlan& plan,
    MeshRepairCleanupResult& cleanup)
{
    cleanup.candidate.mesh.vertices = request.mesh->mesh.vertices;
    cleanup.candidate.mesh.bbox_mm = request.mesh->mesh.bbox_mm;
    cleanup.candidate.mesh.source_name = request.mesh->mesh.source_name;
    cleanup.candidate.material_infos = request.mesh->material_infos;

    std::map<std::size_t, std::size_t> outputIndexByInput;
    std::uint64_t operationId{1U};
    for (const std::size_t sourceIndex : request.mesh->rejected_degenerate_source_triangle_indices)
    {
        cleanup.evidence.operations.push_back(
            MakeDegenerateOperation(operationId++, sourceIndex, std::nullopt));
        AddRemovedMapping(
            cleanup.evidence,
            sourceIndex,
            MeshRepairTriangleDisposition::RemovedDegenerate);
    }
    for (const PlannedTriangle& planned : plan.triangles)
    {
        if (planned.degenerate)
        {
            cleanup.evidence.operations.push_back(
                MakeDegenerateOperation(operationId++, planned.sourceIndex, planned.inputIndex));
            AddRemovedMapping(
                cleanup.evidence,
                planned.sourceIndex,
                MeshRepairTriangleDisposition::RemovedDegenerate);
            continue;
        }
        if (planned.duplicateOfInputIndex.has_value())
        {
            continue;
        }
        const std::size_t outputIndex = cleanup.candidate.mesh.triangles.size();
        cleanup.candidate.mesh.triangles.push_back(
            request.mesh->mesh.triangles.at(planned.inputIndex));
        cleanup.candidate.triangle_attributes.push_back(
            request.mesh->triangle_attributes.at(planned.inputIndex));
        outputIndexByInput.emplace(planned.inputIndex, outputIndex);
        AddRetainedMapping(cleanup.evidence, planned.sourceIndex, outputIndex);
    }
    for (const PlannedTriangle& planned : plan.triangles)
    {
        if (!planned.duplicateOfInputIndex.has_value())
        {
            continue;
        }
        const std::size_t retainedInputIndex = planned.duplicateOfInputIndex.value();
        const std::size_t retainedOutputIndex = outputIndexByInput.at(retainedInputIndex);
        const std::size_t retainedSourceIndex =
            request.mesh->triangle_attributes.at(retainedInputIndex).source_triangle_index;
        cleanup.evidence.operations.push_back(MakeDuplicateOperation(
            operationId++,
            planned,
            retainedSourceIndex,
            retainedOutputIndex));
        AddRemovedMapping(
            cleanup.evidence,
            planned.sourceIndex,
            MeshRepairTriangleDisposition::RemovedExactDuplicate,
            retainedSourceIndex);
    }
    SortMappings(cleanup.evidence);

    cleanup.candidate.topology = AnalyzeMeshTopology(cleanup.candidate.mesh);
    cleanup.candidate.topology.source_triangles = request.mesh->topology.source_triangles;
    cleanup.candidate.topology.degenerate_triangles = 0U;
}

void BuildPostEvidence(
    const MeshRepairCleanupRequest& request,
    MeshRepairCleanupResult& cleanup)
{
    MeshRepairPreflightRequest postRequest;
    postRequest.mesh = &cleanup.candidate;
    postRequest.input = request.input;
    postRequest.options = request.options;
    postRequest.options.enabled = false;
    postRequest.options.mode = "strict_closed";
    postRequest.robustnessOptions = request.robustnessOptions;
    postRequest.sourceHash = request.sourceHash;
    const MeshRepairResult post = EvaluateMeshRepairPreflight(postRequest);
    cleanup.evidence.postRepair = post.preRepair;
    cleanup.evidence.attributePreservation.status = "passed";
    cleanup.evidence.attributePreservation.sourceMappedTriangles =
        cleanup.candidate.mesh.triangles.size();
    cleanup.evidence.attributePreservation.pass = true;
    cleanup.evidence.admission.mode = "repair_then_strict";
    cleanup.evidence.admission.status = "non_production_only";
    cleanup.evidence.admission.postRepairStrictPass = post.preRepair.strictPass;
    cleanup.evidence.admission.productionAllowed = false;
    cleanup.evidence.admission.blockerCodes.clear();
    cleanup.evidence.admission.suggestedActions.clear();
    if (!post.preRepair.strictPass)
    {
        for (const MeshRepairEligibilityDecision& decision : post.eligibility.decisions)
        {
            cleanup.evidence.admission.blockerCodes.push_back(decision.issueCode);
            cleanup.evidence.admission.suggestedActions.push_back(decision.suggestedAction);
        }
    }
    cleanup.evidence.issues = post.preRepair.issues;
    if (post.status == MeshRepairStatus::StrictPassNoRepair)
    {
        cleanup.evidence.status = cleanup.evidence.operations.empty()
            ? MeshRepairStatus::StrictPassNoRepair
            : MeshRepairStatus::RepairedStrictPass;
    }
    else if (post.status == MeshRepairStatus::RejectedSelfIntersection)
    {
        cleanup.evidence.status = MeshRepairStatus::RejectedSelfIntersection;
    }
    else
    {
        cleanup.evidence.status = MeshRepairStatus::ManualRepairRequired;
    }
}

}  // namespace

MeshRepairCleanupResult ExecuteMeshRepairCleanup(
    const MeshRepairCleanupRequest& request)
{
    ValidateRequest(request);
    const Clock::time_point totalStart = Clock::now();
    MeshRepairResult evidence = BuildPreflightEvidence(request);
    if (evidence.status == MeshRepairStatus::RejectedSelfIntersection)
    {
        MeshRepairCleanupResult cleanup;
        cleanup.candidate = *request.mesh;
        cleanup.evidence = std::move(evidence);
        return cleanup;
    }
    const CleanupPlan plan = BuildCleanupPlan(
        *request.mesh,
        request.robustnessOptions.tolerance.area_epsilon_mm2);
    if (plan.attributeConflict)
    {
        return BuildBlockedResult(request, std::move(evidence), plan);
    }

    MeshRepairCleanupResult cleanup;
    cleanup.evidence = std::move(evidence);
    const Clock::time_point repairStart = Clock::now();
    BuildCandidate(request, plan, cleanup);

    bool topologyBlocked{false};
    std::string topologyAttributeStatus;
    std::string topologyBlockerCode;
    if (request.options.allowVertexWeld || request.options.allowWindingRepair)
    {
        MeshRepairTopologyOperationRequest topologyRequest;
        topologyRequest.mesh = &cleanup.candidate;
        topologyRequest.options = request.options;
        topologyRequest.robustnessOptions = request.robustnessOptions;
        topologyRequest.firstOperationId = cleanup.evidence.operations.size() + 1U;
        MeshRepairTopologyOperationResult topology =
            ExecuteMeshRepairTopologyOperations(topologyRequest);
        cleanup.evidence.vertexMappings = std::move(topology.vertexMappings);
        topologyBlocked = topology.blocked;
        topologyAttributeStatus = std::move(topology.attributeStatus);
        topologyBlockerCode = std::move(topology.blockerCode);
        if (!topologyBlocked)
        {
            cleanup.candidate = std::move(topology.candidate);
            cleanup.evidence.operations.insert(
                cleanup.evidence.operations.end(),
                std::make_move_iterator(topology.operations.begin()),
                std::make_move_iterator(topology.operations.end()));
        }
    }
    cleanup.evidence.performance.repairMs = ElapsedMilliseconds(repairStart);
    cleanup.evidence.repairAttempted = !cleanup.evidence.operations.empty();

    const Clock::time_point postStart = Clock::now();
    BuildPostEvidence(request, cleanup);
    cleanup.evidence.performance.postDiagnosticsMs = ElapsedMilliseconds(postStart);
    if (topologyBlocked)
    {
        cleanup.evidence.status = MeshRepairStatus::ManualRepairRequired;
        cleanup.evidence.attributePreservation.status = topologyAttributeStatus;
        cleanup.evidence.attributePreservation.pass = false;
        cleanup.evidence.admission.postRepairStrictPass = false;
        cleanup.evidence.admission.blockerCodes.push_back(topologyBlockerCode);
        cleanup.evidence.admission.suggestedActions.push_back(
            "inspect guarded vertex weld and winding evidence");
    }

    const Clock::time_point hashStart = Clock::now();
    cleanup.evidence.hashes.postRepairGeometryHash =
        ComputeMeshRepairGeometryHash(cleanup.candidate.mesh);
    cleanup.evidence.hashes.postRepairAttributeHash =
        ComputeMeshRepairAttributeHash(cleanup.candidate);
    cleanup.evidence.hashes.repairOperationHash =
        ComputeMeshRepairOperationsHash(cleanup.evidence.operations);
    cleanup.evidence.performance.hashMs =
        cleanup.evidence.performance.hashMs.value_or(0.0)
        + ElapsedMilliseconds(hashStart);
    cleanup.evidence.performance.totalRepairCoreMs = ElapsedMilliseconds(totalStart);
    cleanup.evidence.productionOutputWritten = false;
    return cleanup;
}

}  // namespace slicer_core
