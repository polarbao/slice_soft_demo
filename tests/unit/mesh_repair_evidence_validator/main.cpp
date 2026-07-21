#include "slicer_core/geometry/MeshScaleTolerance.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/geometry/repair/MeshRepairEvidenceValidator.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshRepairService.h"

#include <iostream>
#include <string>

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

slicer_core::SurfaceTriangleAttributes MakeAttributes(
    const std::size_t sourceIndex,
    const bool hasUv = true)
{
    slicer_core::SurfaceTriangleAttributes attributes;
    attributes.source_triangle_index = sourceIndex;
    attributes.has_uv = hasUv;
    attributes.material_name = "fixture-material";
    attributes.uv = {
        slicer_core::TexCoord{0.0, 0.0},
        slicer_core::TexCoord{1.0, 0.0},
        slicer_core::TexCoord{0.0, 1.0}};
    return attributes;
}

slicer_core::AdaptedTriangleMesh MakeAttributedBox(const bool hasUv = true)
{
    slicer_core::AdaptedTriangleMesh mesh;
    mesh.mesh = slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    for (std::size_t index{0U}; index < mesh.mesh.triangles.size(); ++index)
    {
        mesh.triangle_attributes.push_back(MakeAttributes(index, hasUv));
    }
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    return mesh;
}

slicer_core::AdaptedTriangleMesh MakeBoxWithTopHole()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox(false);
    mesh.mesh.triangles.erase(
        mesh.mesh.triangles.begin() + 2,
        mesh.mesh.triangles.begin() + 4);
    mesh.triangle_attributes.erase(
        mesh.triangle_attributes.begin() + 2,
        mesh.triangle_attributes.begin() + 4);
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    return mesh;
}

slicer_core::AdaptedTriangleMesh MakeBoxWithDuplicateFace()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    mesh.mesh.triangles.push_back(mesh.mesh.triangles.front());
    mesh.triangle_attributes.push_back(
        MakeAttributes(mesh.triangle_attributes.size()));
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    return mesh;
}

slicer_core::MeshRepairCleanupRequest MakeRequest(
    const slicer_core::AdaptedTriangleMesh& mesh)
{
    slicer_core::MeshRepairCleanupRequest request;
    request.mesh = &mesh;
    request.input.sourcePath = "generated/evidence-validator.obj";
    request.input.inputFormat = "generated";
    request.options.enabled = true;
    request.options.mode = "repair_then_strict";
    request.options.validatePostRepairEvidence = true;
    request.sourceHash = "fixture-source-hash";
    request.robustnessOptions.tolerance = slicer_core::MakeMeshScaleTolerance(
        mesh.mesh.bbox_mm,
        0.10);
    request.robustnessOptions.max_triangle_pair_checks = 100000U;
    return request;
}

void EnableBoundaryFill(slicer_core::MeshRepairCleanupRequest& request)
{
    request.options.allowVertexWeld = true;
    request.options.weldToleranceMm = 0.0001;
    request.options.allowWindingRepair = true;
    request.options.allowBoundaryFill = true;
    request.options.allowNewFaces = true;
    request.options.maxBoundaryLoopEdges = 8U;
    request.options.maxBoundaryLoopDiameterMm = 2.0;
    request.options.maxBoundaryLoopPerimeterMm = 5.0;
    request.options.maxBoundaryPlanarityErrorMm = 0.01;
    request.options.maxHoleAreaMm2 = 2.0;
    request.options.maxAffectedFaceRatio = 0.25;
    request.options.newFaceAttributePolicy = "inherit_uniform_material_no_uv";
}

slicer_core::MeshRepairEvidenceValidationResult ValidateAgain(
    const slicer_core::AdaptedTriangleMesh& original,
    const slicer_core::AdaptedTriangleMesh& candidate,
    const slicer_core::MeshRepairResult& evidence,
    const slicer_core::MeshRobustnessOptions& robustnessOptions)
{
    slicer_core::MeshRepairEvidenceValidationRequest request;
    request.originalMesh = &original;
    request.candidateMesh = &candidate;
    request.evidence = &evidence;
    request.robustnessOptions = robustnessOptions;
    return slicer_core::ValidateMeshRepairEvidence(request);
}

bool TestClosedBoxPassesAllEvidenceGates()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    const slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(request);

    return ExpectTrue(cleanup.evidence.evidenceValidation.pass, "closed box evidence must pass")
        && ExpectTrue(
            cleanup.evidence.evidenceValidation.status == "passed",
            "closed box validator status must be passed")
        && ExpectTrue(
            cleanup.evidence.evidenceValidation.operationSequencePass
                && cleanup.evidence.evidenceValidation.sourceMappingPass
                && cleanup.evidence.evidenceValidation.vertexMappingPass
                && cleanup.evidence.evidenceValidation.generatedMappingPass
                && cleanup.evidence.evidenceValidation.attributePass
                && cleanup.evidence.evidenceValidation.postStrictComplete
                && cleanup.evidence.evidenceValidation.postStrictPass
                && cleanup.evidence.evidenceValidation.hashConsistencyPass,
            "all closed box gates must pass")
        && ExpectTrue(
            cleanup.evidence.status == slicer_core::MeshRepairStatus::StrictPassNoRepair,
            "closed box should remain strict no-op")
        && ExpectTrue(!cleanup.evidence.productionOutputWritten, "validator must stay non-production");
}

bool TestMissingSourceMappingIsBlocked()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    const slicer_core::MeshRepairCleanupRequest cleanupRequest = MakeRequest(mesh);
    slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(cleanupRequest);
    cleanup.evidence.sourceMappings.pop_back();

    const slicer_core::MeshRepairEvidenceValidationResult validation = ValidateAgain(
        mesh,
        cleanup.candidate,
        cleanup.evidence,
        cleanupRequest.robustnessOptions);
    return ExpectTrue(!validation.validation.pass, "missing source mapping must fail")
        && ExpectTrue(
            validation.validation.status == "blocked_source_mapping",
            "missing source mapping must expose stable status");
}

bool TestDuplicateVertexSourceIsBlocked()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    const slicer_core::MeshRepairCleanupRequest cleanupRequest = MakeRequest(mesh);
    slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(cleanupRequest);
    cleanup.evidence.vertexMappings.at(1U).sourceVertexIndices = {0U};

    const slicer_core::MeshRepairEvidenceValidationResult validation = ValidateAgain(
        mesh,
        cleanup.candidate,
        cleanup.evidence,
        cleanupRequest.robustnessOptions);
    return ExpectTrue(!validation.validation.pass, "duplicate vertex source must fail")
        && ExpectTrue(
            validation.validation.status == "blocked_vertex_mapping",
            "duplicate vertex source must expose stable status");
}

bool TestGeneratedMappingAndAttributeCorruptionAreBlocked()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeBoxWithTopHole();
    slicer_core::MeshRepairCleanupRequest cleanupRequest = MakeRequest(mesh);
    EnableBoundaryFill(cleanupRequest);
    const slicer_core::MeshRepairCleanupResult valid =
        slicer_core::ExecuteMeshRepairCleanup(cleanupRequest);
    if (!ExpectTrue(valid.evidence.evidenceValidation.pass, "simple hole baseline must pass"))
    {
        return false;
    }

    slicer_core::MeshRepairResult mappingEvidence = valid.evidence;
    mappingEvidence.generatedTriangleMappings.front().outputTriangleIndex =
        valid.candidate.mesh.triangles.size();
    const slicer_core::MeshRepairEvidenceValidationResult mappingValidation = ValidateAgain(
        mesh,
        valid.candidate,
        mappingEvidence,
        cleanupRequest.robustnessOptions);

    slicer_core::AdaptedTriangleMesh attributeCandidate = valid.candidate;
    const std::size_t generatedIndex = static_cast<std::size_t>(
        valid.evidence.generatedTriangleMappings.front().outputTriangleIndex);
    attributeCandidate.triangle_attributes.at(generatedIndex).material_name = "corrupt";
    const slicer_core::MeshRepairEvidenceValidationResult attributeValidation = ValidateAgain(
        mesh,
        attributeCandidate,
        valid.evidence,
        cleanupRequest.robustnessOptions);

    return ExpectTrue(
               mappingValidation.validation.status == "blocked_generated_mapping",
               "invalid generated output must be blocked")
        && ExpectTrue(
            attributeValidation.validation.status == "blocked_generated_attribute",
            "generated material corruption must be blocked");
}

bool TestOperationSequenceAndHashCorruptionAreBlocked()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeBoxWithDuplicateFace();
    const slicer_core::MeshRepairCleanupRequest cleanupRequest = MakeRequest(mesh);
    const slicer_core::MeshRepairCleanupResult valid =
        slicer_core::ExecuteMeshRepairCleanup(cleanupRequest);
    if (!ExpectTrue(!valid.evidence.operations.empty(), "duplicate fixture must create operation"))
    {
        return false;
    }

    slicer_core::MeshRepairResult operationEvidence = valid.evidence;
    operationEvidence.operations.front().operationId = 2U;
    const slicer_core::MeshRepairEvidenceValidationResult operationValidation = ValidateAgain(
        mesh,
        valid.candidate,
        operationEvidence,
        cleanupRequest.robustnessOptions);

    slicer_core::MeshRepairResult hashEvidence = valid.evidence;
    hashEvidence.hashes.repairOperationHash = "corrupt";
    const slicer_core::MeshRepairEvidenceValidationResult hashValidation = ValidateAgain(
        mesh,
        valid.candidate,
        hashEvidence,
        cleanupRequest.robustnessOptions);

    return ExpectTrue(
               operationValidation.validation.status == "blocked_operation_sequence",
               "operation id gap must be blocked")
        && ExpectTrue(
            hashValidation.validation.status == "blocked_hash_consistency",
            "hash mismatch must be blocked");
}

bool TestDuplicateRemovalRequiresRetainedSourceMapping()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeBoxWithDuplicateFace();
    const slicer_core::MeshRepairCleanupRequest cleanupRequest = MakeRequest(mesh);
    slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(cleanupRequest);
    for (slicer_core::MeshRepairTriangleMapping& mapping : cleanup.evidence.sourceMappings)
    {
        if (mapping.disposition
            == slicer_core::MeshRepairTriangleDisposition::RemovedExactDuplicate)
        {
            mapping.retainedSourceTriangleIndex.reset();
        }
    }

    const slicer_core::MeshRepairEvidenceValidationResult validation = ValidateAgain(
        mesh,
        cleanup.candidate,
        cleanup.evidence,
        cleanupRequest.robustnessOptions);
    return ExpectTrue(
        validation.validation.status == "blocked_source_mapping",
        "duplicate removal without retained source must be blocked");
}

bool TestGeneratedSourceIdCannotAliasOriginalSource()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeBoxWithTopHole();
    slicer_core::MeshRepairCleanupRequest cleanupRequest = MakeRequest(mesh);
    EnableBoundaryFill(cleanupRequest);
    slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(cleanupRequest);
    const std::size_t generatedIndex = static_cast<std::size_t>(
        cleanup.evidence.generatedTriangleMappings.front().outputTriangleIndex);
    cleanup.candidate.triangle_attributes.at(generatedIndex).source_triangle_index = 0U;
    cleanup.evidence.hashes.postRepairAttributeHash =
        slicer_core::ComputeMeshRepairAttributeHash(cleanup.candidate);

    const slicer_core::MeshRepairEvidenceValidationResult validation = ValidateAgain(
        mesh,
        cleanup.candidate,
        cleanup.evidence,
        cleanupRequest.robustnessOptions);
    return ExpectTrue(
        validation.validation.status == "blocked_generated_mapping",
        "generated source id must not alias an original source id");
}

bool TestIncompletePostStrictAndOpenBoundaryDiscardCandidate()
{
    const slicer_core::AdaptedTriangleMesh closed = MakeAttributedBox();
    slicer_core::MeshRepairCleanupRequest sampledRequest = MakeRequest(closed);
    sampledRequest.robustnessOptions.max_triangle_pair_checks = 1U;
    const slicer_core::MeshRepairCleanupResult sampled =
        slicer_core::ExecuteMeshRepairCleanup(sampledRequest);

    const slicer_core::AdaptedTriangleMesh open = MakeBoxWithTopHole();
    const slicer_core::MeshRepairCleanupRequest openRequest = MakeRequest(open);
    const slicer_core::MeshRepairCleanupResult blocked =
        slicer_core::ExecuteMeshRepairCleanup(openRequest);

    return ExpectTrue(
               sampled.evidence.evidenceValidation.status
                   == "blocked_incomplete_post_strict",
               "sampled post strict must be blocked")
        && ExpectTrue(
            blocked.evidence.evidenceValidation.status == "blocked_post_strict",
            "open boundary must fail post strict")
        && ExpectTrue(
            blocked.candidate.mesh.triangles == open.mesh.triangles,
            "failed candidate must be discarded")
        && ExpectTrue(
            blocked.evidence.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
            "post strict blocker must remain manual");
}

}  // namespace

int main()
{
    const bool passed = TestClosedBoxPassesAllEvidenceGates()
        && TestMissingSourceMappingIsBlocked()
        && TestDuplicateVertexSourceIsBlocked()
        && TestGeneratedMappingAndAttributeCorruptionAreBlocked()
        && TestOperationSequenceAndHashCorruptionAreBlocked()
        && TestDuplicateRemovalRequiresRetainedSourceMapping()
        && TestGeneratedSourceIdCannotAliasOriginalSource()
        && TestIncompletePostStrictAndOpenBoundaryDiscardCandidate();
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS mesh repair evidence validator unit tests\n";
    return 0;
}
