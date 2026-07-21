#include "slicer_core/geometry/repair/MeshRepairEvidenceValidator.h"

#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshRepairPreflight.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

using Clock = std::chrono::steady_clock;
using EdgeKey = std::pair<int, int>;

class DisjointSet
{
public:
    explicit DisjointSet(const std::size_t size)
        : m_parent(size),
          m_rank(size, 0U)
    {
        std::iota(m_parent.begin(), m_parent.end(), 0U);
    }

    std::size_t Find(const std::size_t value)
    {
        if (m_parent.at(value) != value)
        {
            m_parent.at(value) = Find(m_parent.at(value));
        }
        return m_parent.at(value);
    }

    void Unite(const std::size_t left, const std::size_t right)
    {
        std::size_t leftRoot = Find(left);
        std::size_t rightRoot = Find(right);
        if (leftRoot == rightRoot)
        {
            return;
        }
        if (m_rank.at(leftRoot) < m_rank.at(rightRoot))
        {
            std::swap(leftRoot, rightRoot);
        }
        m_parent.at(rightRoot) = leftRoot;
        if (m_rank.at(leftRoot) == m_rank.at(rightRoot))
        {
            ++m_rank.at(leftRoot);
        }
    }

private:
    std::vector<std::size_t> m_parent;
    std::vector<std::uint8_t> m_rank;
};

double ElapsedMilliseconds(const Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void BlockValidation(
    MeshRepairEvidenceValidation& validation,
    const std::string& status,
    const MeshRepairErrorCode errorCode,
    const std::string& issueCode,
    const std::string& message)
{
    validation.status = status;
    validation.pass = false;
    validation.candidateAccepted = false;
    validation.blockerCodes = {MeshRepairErrorCodeName(errorCode)};
    validation.issues.push_back(MakeValidationIssue(
        issueCode,
        ValidationSeverity::Error,
        message));
}

bool ContainsElement(
    const std::vector<std::uint64_t>& values,
    const std::uint64_t expected)
{
    return std::find(values.begin(), values.end(), expected) != values.end();
}

int OperationRank(const MeshRepairOperationType type)
{
    switch (type)
    {
    case MeshRepairOperationType::RemoveDegenerateTriangle:
    case MeshRepairOperationType::RemoveExactDuplicateFace:
        return 0;
    case MeshRepairOperationType::WeldVertex:
        return 1;
    case MeshRepairOperationType::FlipTriangleWinding:
        return 2;
    case MeshRepairOperationType::StitchBoundaryLoop:
    case MeshRepairOperationType::FillBoundaryLoop:
        return 3;
    case MeshRepairOperationType::SplitEdgeFan:
        return 4;
    }
    return 5;
}

bool ValidateOperationSequence(
    const MeshRepairResult& evidence,
    MeshRepairEvidenceValidation& validation)
{
    int previousRank{-1};
    for (std::size_t index{0U}; index < evidence.operations.size(); ++index)
    {
        const MeshRepairOperation& operation = evidence.operations.at(index);
        const int rank = OperationRank(operation.type);
        if (operation.operationId != index + 1U || rank < previousRank)
        {
            BlockValidation(
                validation,
                "blocked_operation_sequence",
                MeshRepairErrorCode::OperationInvalid,
                "MESH_REPAIR_OPERATION_SEQUENCE_INVALID",
                "repair operation ids or stage ordering are not canonical");
            return false;
        }
        previousRank = rank;
    }
    validation.operationSequencePass = true;
    return true;
}

bool HasRemovalOperation(
    const MeshRepairResult& evidence,
    const MeshRepairTriangleMapping& mapping)
{
    const MeshRepairOperationType expectedType =
        mapping.disposition == MeshRepairTriangleDisposition::RemovedDegenerate
        ? MeshRepairOperationType::RemoveDegenerateTriangle
        : MeshRepairOperationType::RemoveExactDuplicateFace;
    return std::any_of(
        evidence.operations.begin(),
        evidence.operations.end(),
        [&mapping, expectedType](const MeshRepairOperation& operation)
        {
            return operation.type == expectedType
                && ContainsElement(operation.inputElementIds, mapping.sourceTriangleIndex);
        });
}

bool ValidateSourceMappings(
    const AdaptedTriangleMesh& original,
    const AdaptedTriangleMesh& candidate,
    const MeshRepairResult& evidence,
    MeshRepairEvidenceValidation& validation,
    std::set<std::size_t>& retainedOutputs,
    std::map<std::size_t, std::size_t>& originalTriangleBySource)
{
    std::set<std::size_t> expectedSources;
    for (std::size_t index{0U}; index < original.triangle_attributes.size(); ++index)
    {
        const std::size_t sourceIndex =
            original.triangle_attributes.at(index).source_triangle_index;
        if (!expectedSources.insert(sourceIndex).second)
        {
            BlockValidation(
                validation,
                "blocked_source_mapping",
                MeshRepairErrorCode::OperationInvalid,
                "MESH_REPAIR_SOURCE_MAPPING_INVALID",
                "original source triangle ids are not unique");
            return false;
        }
        originalTriangleBySource.emplace(sourceIndex, index);
    }
    for (const std::size_t sourceIndex : original.rejected_degenerate_source_triangle_indices)
    {
        if (!expectedSources.insert(sourceIndex).second)
        {
            BlockValidation(
                validation,
                "blocked_source_mapping",
                MeshRepairErrorCode::OperationInvalid,
                "MESH_REPAIR_SOURCE_MAPPING_INVALID",
                "rejected source triangle id overlaps an accepted source id");
            return false;
        }
    }

    std::set<std::size_t> seenSources;
    std::map<std::size_t, const MeshRepairTriangleMapping*> mappingsBySource;
    for (const MeshRepairTriangleMapping& mapping : evidence.sourceMappings)
    {
        const std::size_t sourceIndex = static_cast<std::size_t>(mapping.sourceTriangleIndex);
        if (expectedSources.find(sourceIndex) == expectedSources.end()
            || !seenSources.insert(sourceIndex).second)
        {
            BlockValidation(
                validation,
                "blocked_source_mapping",
                MeshRepairErrorCode::OperationInvalid,
                "MESH_REPAIR_SOURCE_MAPPING_INVALID",
                "source mapping is missing, duplicated, or references an unknown source");
            return false;
        }
        mappingsBySource.emplace(sourceIndex, &mapping);
        if (mapping.disposition == MeshRepairTriangleDisposition::Retained)
        {
            if (!mapping.outputTriangleIndex.has_value())
            {
                BlockValidation(
                    validation,
                    "blocked_source_mapping",
                    MeshRepairErrorCode::OperationInvalid,
                    "MESH_REPAIR_SOURCE_MAPPING_INVALID",
                    "retained source mapping has no output triangle");
                return false;
            }
            const std::size_t outputIndex = static_cast<std::size_t>(
                mapping.outputTriangleIndex.value());
            if (outputIndex >= candidate.mesh.triangles.size()
                || !retainedOutputs.insert(outputIndex).second
                || originalTriangleBySource.find(sourceIndex)
                    == originalTriangleBySource.end())
            {
                BlockValidation(
                    validation,
                    "blocked_source_mapping",
                    MeshRepairErrorCode::OperationInvalid,
                    "MESH_REPAIR_SOURCE_MAPPING_INVALID",
                    "retained source mapping has an invalid or duplicate output triangle");
                return false;
            }
        }
        else if (mapping.outputTriangleIndex.has_value()
                 || !HasRemovalOperation(evidence, mapping))
        {
            BlockValidation(
                validation,
                "blocked_source_mapping",
                MeshRepairErrorCode::OperationInvalid,
                "MESH_REPAIR_SOURCE_MAPPING_INVALID",
                "removed source mapping is not explained by a matching operation");
            return false;
        }
    }
    if (seenSources != expectedSources)
    {
        BlockValidation(
            validation,
            "blocked_source_mapping",
            MeshRepairErrorCode::OperationInvalid,
            "MESH_REPAIR_SOURCE_MAPPING_INVALID",
            "not every original source triangle has exactly one mapping");
        return false;
    }
    for (const MeshRepairTriangleMapping& mapping : evidence.sourceMappings)
    {
        if (mapping.disposition != MeshRepairTriangleDisposition::RemovedExactDuplicate)
        {
            continue;
        }
        if (!mapping.retainedSourceTriangleIndex.has_value())
        {
            BlockValidation(
                validation,
                "blocked_source_mapping",
                MeshRepairErrorCode::OperationInvalid,
                "MESH_REPAIR_SOURCE_MAPPING_INVALID",
                "duplicate removal has no retained source triangle");
            return false;
        }
        const auto retained = mappingsBySource.find(static_cast<std::size_t>(
            mapping.retainedSourceTriangleIndex.value()));
        if (retained == mappingsBySource.end()
            || retained->second->disposition != MeshRepairTriangleDisposition::Retained)
        {
            BlockValidation(
                validation,
                "blocked_source_mapping",
                MeshRepairErrorCode::OperationInvalid,
                "MESH_REPAIR_SOURCE_MAPPING_INVALID",
                "duplicate removal does not reference a retained source triangle");
            return false;
        }
    }
    validation.sourceMappingPass = true;
    return true;
}

std::vector<std::set<std::size_t>> BuildVertexComponentOwnership(
    const TriangleMeshData& mesh)
{
    DisjointSet triangleSets(mesh.triangles.size());
    std::map<EdgeKey, std::vector<std::size_t>> usesByEdge;
    for (std::size_t triangleIndex{0U}; triangleIndex < mesh.triangles.size(); ++triangleIndex)
    {
        const std::array<int, 3>& triangle = mesh.triangles.at(triangleIndex);
        const std::array<EdgeKey, 3> edges{
            std::minmax(triangle.at(0U), triangle.at(1U)),
            std::minmax(triangle.at(1U), triangle.at(2U)),
            std::minmax(triangle.at(2U), triangle.at(0U))};
        for (const EdgeKey& edge : edges)
        {
            std::vector<std::size_t>& uses = usesByEdge[edge];
            for (const std::size_t other : uses)
            {
                triangleSets.Unite(triangleIndex, other);
            }
            uses.push_back(triangleIndex);
        }
    }

    std::map<std::size_t, std::size_t> componentByRoot;
    std::vector<std::set<std::size_t>> ownership(mesh.vertices.size());
    for (std::size_t triangleIndex{0U}; triangleIndex < mesh.triangles.size(); ++triangleIndex)
    {
        const std::size_t root = triangleSets.Find(triangleIndex);
        const auto [found, inserted] = componentByRoot.emplace(
            root,
            componentByRoot.size());
        (void)inserted;
        for (const int vertexIndex : mesh.triangles.at(triangleIndex))
        {
            ownership.at(static_cast<std::size_t>(vertexIndex)).insert(found->second);
        }
    }
    return ownership;
}

bool EqualPosition(const Vec3& left, const Vec3& right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool ValidateVertexMappings(
    const AdaptedTriangleMesh& original,
    const AdaptedTriangleMesh& candidate,
    const MeshRepairResult& evidence,
    MeshRepairEvidenceValidation& validation)
{
    if (evidence.vertexMappings.size() != candidate.mesh.vertices.size())
    {
        BlockValidation(
            validation,
            "blocked_vertex_mapping",
            MeshRepairErrorCode::OperationInvalid,
            "MESH_REPAIR_VERTEX_MAPPING_INVALID",
            "vertex mapping count does not match candidate vertices");
        return false;
    }
    const std::vector<std::set<std::size_t>> componentOwnership =
        BuildVertexComponentOwnership(original.mesh);
    std::vector<std::size_t> sourceCoverage(original.mesh.vertices.size(), 0U);
    std::set<std::size_t> outputIndices;
    for (const MeshRepairVertexMapping& mapping : evidence.vertexMappings)
    {
        const std::size_t outputIndex = static_cast<std::size_t>(mapping.outputVertexIndex);
        if (outputIndex >= candidate.mesh.vertices.size()
            || !outputIndices.insert(outputIndex).second
            || mapping.sourceVertexIndices.empty()
            || !std::is_sorted(
                mapping.sourceVertexIndices.begin(),
                mapping.sourceVertexIndices.end()))
        {
            BlockValidation(
                validation,
                "blocked_vertex_mapping",
                MeshRepairErrorCode::OperationInvalid,
                "MESH_REPAIR_VERTEX_MAPPING_INVALID",
                "vertex mapping has an invalid output or non-canonical source set");
            return false;
        }
        std::optional<std::set<std::size_t>> expectedOwnership;
        for (const std::uint64_t sourceValue : mapping.sourceVertexIndices)
        {
            const std::size_t sourceIndex = static_cast<std::size_t>(sourceValue);
            if (sourceIndex >= original.mesh.vertices.size()
                || ++sourceCoverage.at(sourceIndex) != 1U)
            {
                BlockValidation(
                    validation,
                    "blocked_vertex_mapping",
                    MeshRepairErrorCode::OperationInvalid,
                    "MESH_REPAIR_VERTEX_MAPPING_INVALID",
                    "source vertex is missing, duplicated, or out of range");
                return false;
            }
            if (!expectedOwnership.has_value())
            {
                expectedOwnership = componentOwnership.at(sourceIndex);
            }
            else if (expectedOwnership.value() != componentOwnership.at(sourceIndex))
            {
                BlockValidation(
                    validation,
                    "blocked_vertex_mapping",
                    MeshRepairErrorCode::AmbiguousTopology,
                    "MESH_REPAIR_VERTEX_COMPONENT_MERGE",
                    "one output vertex combines sources from different components");
                return false;
            }
        }
        const std::size_t representative = static_cast<std::size_t>(
            mapping.sourceVertexIndices.front());
        if (!EqualPosition(
                candidate.mesh.vertices.at(outputIndex),
                original.mesh.vertices.at(representative)))
        {
            BlockValidation(
                validation,
                "blocked_vertex_mapping",
                MeshRepairErrorCode::OperationInvalid,
                "MESH_REPAIR_VERTEX_MAPPING_INVALID",
                "candidate vertex does not match its canonical source representative");
            return false;
        }
    }
    if (std::any_of(
            sourceCoverage.begin(),
            sourceCoverage.end(),
            [](const std::size_t count)
            {
                return count != 1U;
            }))
    {
        BlockValidation(
            validation,
            "blocked_vertex_mapping",
            MeshRepairErrorCode::OperationInvalid,
            "MESH_REPAIR_VERTEX_MAPPING_INVALID",
            "not every source vertex is covered exactly once");
        return false;
    }
    validation.vertexMappingPass = true;
    return true;
}

bool ValidateGeneratedMappings(
    const AdaptedTriangleMesh& candidate,
    const MeshRepairResult& evidence,
    const std::set<std::size_t>& retainedOutputs,
    MeshRepairEvidenceValidation& validation,
    std::set<std::size_t>& generatedOutputs)
{
    const std::set<std::size_t> sourceIds = [&evidence]()
    {
        std::set<std::size_t> values;
        for (const MeshRepairTriangleMapping& mapping : evidence.sourceMappings)
        {
            values.insert(static_cast<std::size_t>(mapping.sourceTriangleIndex));
        }
        return values;
    }();
    std::set<std::size_t> generatedSourceIds;
    for (const MeshRepairGeneratedTriangleMapping& mapping :
         evidence.generatedTriangleMappings)
    {
        const std::size_t outputIndex = static_cast<std::size_t>(
            mapping.outputTriangleIndex);
        if (outputIndex >= candidate.mesh.triangles.size()
            || retainedOutputs.find(outputIndex) != retainedOutputs.end()
            || !generatedOutputs.insert(outputIndex).second
            || mapping.generatingBoundaryVertexIndices.size() != 3U
            || mapping.attributePolicy != "inherit_uniform_material_no_uv")
        {
            BlockValidation(
                validation,
                "blocked_generated_mapping",
                MeshRepairErrorCode::OperationInvalid,
                "MESH_REPAIR_GENERATED_MAPPING_INVALID",
                "generated triangle mapping is duplicated, out of range, or uses an unknown policy");
            return false;
        }
        const std::array<int, 3>& triangle = candidate.mesh.triangles.at(outputIndex);
        const std::size_t generatedSourceId =
            candidate.triangle_attributes.at(outputIndex).source_triangle_index;
        if (sourceIds.find(generatedSourceId) != sourceIds.end()
            || !generatedSourceIds.insert(generatedSourceId).second)
        {
            BlockValidation(
                validation,
                "blocked_generated_mapping",
                MeshRepairErrorCode::OperationInvalid,
                "MESH_REPAIR_GENERATED_MAPPING_INVALID",
                "generated triangle source id aliases original or generated provenance");
            return false;
        }
        for (std::size_t corner{0U}; corner < 3U; ++corner)
        {
            if (mapping.generatingBoundaryVertexIndices.at(corner)
                != static_cast<std::uint64_t>(triangle.at(corner)))
            {
                BlockValidation(
                    validation,
                    "blocked_generated_mapping",
                    MeshRepairErrorCode::OperationInvalid,
                    "MESH_REPAIR_GENERATED_MAPPING_INVALID",
                    "generated triangle provenance does not match candidate geometry");
                return false;
            }
        }
    }
    if (retainedOutputs.size() + generatedOutputs.size()
            != candidate.mesh.triangles.size()
        || evidence.attributePreservation.sourceMappedTriangles
            != retainedOutputs.size()
        || evidence.attributePreservation.newTriangles
            != generatedOutputs.size())
    {
        BlockValidation(
            validation,
            "blocked_generated_mapping",
            MeshRepairErrorCode::OperationInvalid,
            "MESH_REPAIR_GENERATED_MAPPING_INVALID",
            "candidate output triangles are not covered exactly once");
        return false;
    }
    validation.generatedMappingPass = true;
    return true;
}

bool EqualUv(const TexCoord& left, const TexCoord& right)
{
    return left.u == right.u && left.v == right.v;
}

bool IsFlippedOutput(
    const MeshRepairResult& evidence,
    const std::size_t outputIndex)
{
    return std::any_of(
        evidence.operations.begin(),
        evidence.operations.end(),
        [outputIndex](const MeshRepairOperation& operation)
        {
            return operation.type == MeshRepairOperationType::FlipTriangleWinding
                && ContainsElement(operation.outputElementIds, outputIndex);
        });
}

bool EqualMaterialInfo(const MaterialInfo& left, const MaterialInfo& right)
{
    return left.name == right.name
        && left.diffuse_rgb == right.diffuse_rgb
        && left.has_diffuse == right.has_diffuse
        && left.diffuse_texture_path.generic_u8string()
            == right.diffuse_texture_path.generic_u8string()
        && left.has_texture == right.has_texture
        && left.texture_exists == right.texture_exists
        && left.texture_source == right.texture_source;
}

bool MaterialResourcesEqual(
    const std::vector<MaterialInfo>& original,
    const std::vector<MaterialInfo>& candidate)
{
    if (original.size() != candidate.size())
    {
        return false;
    }
    std::vector<const MaterialInfo*> left;
    std::vector<const MaterialInfo*> right;
    left.reserve(original.size());
    right.reserve(candidate.size());
    for (const MaterialInfo& material : original)
    {
        left.push_back(&material);
    }
    for (const MaterialInfo& material : candidate)
    {
        right.push_back(&material);
    }
    const auto less = [](const MaterialInfo* first, const MaterialInfo* second)
    {
        return std::tuple{
                   first->name,
                   first->diffuse_rgb,
                   first->has_diffuse,
                   first->diffuse_texture_path.generic_string(),
                   first->has_texture,
                   first->texture_exists,
                   first->texture_source}
            < std::tuple{
                   second->name,
                   second->diffuse_rgb,
                   second->has_diffuse,
                   second->diffuse_texture_path.generic_string(),
                   second->has_texture,
                   second->texture_exists,
                   second->texture_source};
    };
    std::sort(left.begin(), left.end(), less);
    std::sort(right.begin(), right.end(), less);
    for (std::size_t index{0U}; index < left.size(); ++index)
    {
        if (!EqualMaterialInfo(*left.at(index), *right.at(index)))
        {
            return false;
        }
    }
    return true;
}

bool ValidateAttributes(
    const AdaptedTriangleMesh& original,
    const AdaptedTriangleMesh& candidate,
    const MeshRepairResult& evidence,
    const std::map<std::size_t, std::size_t>& originalTriangleBySource,
    MeshRepairEvidenceValidationResult& result)
{
    MeshRepairAttributePreservation& attributes = result.attributePreservation;
    attributes.status = "passed";
    attributes.sourceMappedTriangles = 0U;
    attributes.newTriangles = evidence.generatedTriangleMappings.size();
    attributes.maxUvDelta = 0.0;
    for (const MeshRepairTriangleMapping& mapping : evidence.sourceMappings)
    {
        if (mapping.disposition != MeshRepairTriangleDisposition::Retained)
        {
            continue;
        }
        ++attributes.sourceMappedTriangles;
        const std::size_t sourceIndex = static_cast<std::size_t>(
            mapping.sourceTriangleIndex);
        const std::size_t outputIndex = static_cast<std::size_t>(
            mapping.outputTriangleIndex.value());
        const SurfaceTriangleAttributes& before = original.triangle_attributes.at(
            originalTriangleBySource.at(sourceIndex));
        const SurfaceTriangleAttributes& after = candidate.triangle_attributes.at(outputIndex);
        const bool flipped = IsFlippedOutput(evidence, outputIndex);
        if (after.source_triangle_index != sourceIndex
            || after.material_name != before.material_name)
        {
            ++attributes.materialConflicts;
        }
        if (after.has_uv != before.has_uv)
        {
            ++attributes.uvConflicts;
        }
        else if (after.has_uv)
        {
            for (std::size_t corner{0U}; corner < 3U; ++corner)
            {
                const std::size_t expectedCorner = flipped
                    ? (corner == 1U ? 2U : (corner == 2U ? 1U : 0U))
                    : corner;
                if (!EqualUv(after.uv.at(corner), before.uv.at(expectedCorner)))
                {
                    ++attributes.uvConflicts;
                    break;
                }
            }
        }
    }
    for (const MeshRepairGeneratedTriangleMapping& mapping :
         evidence.generatedTriangleMappings)
    {
        const std::size_t outputIndex = static_cast<std::size_t>(
            mapping.outputTriangleIndex);
        const SurfaceTriangleAttributes& generated =
            candidate.triangle_attributes.at(outputIndex);
        if (mapping.attributePolicy != "inherit_uniform_material_no_uv"
            || mapping.hasUv
            || generated.has_uv
            || mapping.materialName.empty()
            || generated.material_name != mapping.materialName)
        {
            ++attributes.fallbackTriangles;
        }
    }
    if (!MaterialResourcesEqual(original.material_infos, candidate.material_infos))
    {
        attributes.missingTextureResources = 1U;
    }

    if (attributes.materialConflicts > 0U || attributes.uvConflicts > 0U)
    {
        attributes.status = "blocked_attribute_preservation";
        attributes.pass = false;
        attributes.issues.push_back(MakeValidationIssue(
            "MESH_REPAIR_ATTRIBUTE_PRESERVATION_FAILED",
            ValidationSeverity::Error,
            "retained triangle material or UV data changed without an explicit policy"));
        BlockValidation(
            result.validation,
            "blocked_attribute_preservation",
            MeshRepairErrorCode::AttributeMismatch,
            "MESH_REPAIR_ATTRIBUTE_PRESERVATION_FAILED",
            "retained triangle attributes are not preserved");
        return false;
    }
    if (attributes.fallbackTriangles > 0U)
    {
        attributes.status = "blocked_generated_attribute";
        attributes.pass = false;
        attributes.issues.push_back(MakeValidationIssue(
            "MESH_REPAIR_GENERATED_ATTRIBUTE_INVALID",
            ValidationSeverity::Error,
            "generated triangle attributes do not match the explicit policy"));
        BlockValidation(
            result.validation,
            "blocked_generated_attribute",
            MeshRepairErrorCode::AttributeMismatch,
            "MESH_REPAIR_GENERATED_ATTRIBUTE_INVALID",
            "generated triangle attributes do not match the explicit policy");
        return false;
    }
    if (attributes.missingTextureResources > 0U)
    {
        attributes.status = "blocked_attribute_resources";
        attributes.pass = false;
        attributes.issues.push_back(MakeValidationIssue(
            "MESH_REPAIR_ATTRIBUTE_RESOURCE_LOST",
            ValidationSeverity::Error,
            "candidate material resources differ from the original mesh"));
        BlockValidation(
            result.validation,
            "blocked_attribute_resources",
            MeshRepairErrorCode::AttributeMismatch,
            "MESH_REPAIR_ATTRIBUTE_RESOURCE_LOST",
            "candidate material resources differ from the original mesh");
        return false;
    }
    attributes.pass = true;
    result.validation.attributePass = true;
    return true;
}

bool HasIncompleteIntersectionEvidence(const MeshRepairResult& post)
{
    return std::any_of(
        post.eligibility.decisions.begin(),
        post.eligibility.decisions.end(),
        [](const MeshRepairEligibilityDecision& decision)
        {
            return decision.issueCode == "MESH_SELF_INTERSECTION_SAMPLED"
                || decision.issueCode == "MESH_SELF_INTERSECTION_BUDGET_BLOCKED";
        });
}

bool ValidatePostStrict(
    const MeshRepairEvidenceValidationRequest& request,
    MeshRepairEvidenceValidationResult& result)
{
    MeshRepairPreflightRequest postRequest;
    postRequest.mesh = request.candidateMesh;
    postRequest.input = request.evidence->input;
    postRequest.options = request.evidence->options;
    postRequest.options.enabled = false;
    postRequest.options.mode = "strict_closed";
    postRequest.robustnessOptions = request.robustnessOptions;
    postRequest.sourceHash = request.evidence->hashes.sourceHash;
    const MeshRepairResult post = EvaluateMeshRepairPreflight(postRequest);
    result.postRepair = post.preRepair;
    if (HasIncompleteIntersectionEvidence(post))
    {
        BlockValidation(
            result.validation,
            "blocked_incomplete_post_strict",
            MeshRepairErrorCode::PostStrictFailed,
            "MESH_REPAIR_POST_STRICT_INCOMPLETE",
            "post-repair self-intersection evidence is incomplete");
        return false;
    }
    result.validation.postStrictComplete = true;
    const MeshRepairDiagnosticsSummary& diagnostics = result.postRepair;
    const bool strictPass = post.status == MeshRepairStatus::StrictPassNoRepair
        && diagnostics.strictPass
        && diagnostics.boundaryEdges == 0U
        && diagnostics.nonManifoldEdges == 0U
        && diagnostics.duplicateFaces == 0U
        && diagnostics.oppositeDuplicateFaces == 0U
        && diagnostics.localWindingIssues == 0U
        && diagnostics.degenerateTriangles == 0U
        && diagnostics.confirmedSelfIntersectionPairs == 0U;
    if (!strictPass)
    {
        BlockValidation(
            result.validation,
            "blocked_post_strict",
            MeshRepairErrorCode::PostStrictFailed,
            "MESH_REPAIR_POST_STRICT_FAILED",
            "candidate still has strict topology or robustness blockers");
        return false;
    }
    result.validation.postStrictPass = true;
    return true;
}

bool ValidateHashes(
    const MeshRepairEvidenceValidationRequest& request,
    MeshRepairEvidenceValidation& validation)
{
    const MeshRepairHashes& hashes = request.evidence->hashes;
    const bool pass = hashes.preRepairGeometryHash.has_value()
        && hashes.preRepairGeometryHash.value()
            == ComputeMeshRepairGeometryHash(request.originalMesh->mesh)
        && hashes.preRepairAttributeHash.has_value()
        && hashes.preRepairAttributeHash.value()
            == ComputeMeshRepairAttributeHash(*request.originalMesh)
        && hashes.postRepairGeometryHash.has_value()
        && hashes.postRepairGeometryHash.value()
            == ComputeMeshRepairGeometryHash(request.candidateMesh->mesh)
        && hashes.postRepairAttributeHash.has_value()
        && hashes.postRepairAttributeHash.value()
            == ComputeMeshRepairAttributeHash(*request.candidateMesh)
        && hashes.repairOperationHash.has_value()
        && hashes.repairOperationHash.value()
            == ComputeMeshRepairOperationsHash(request.evidence->operations)
        && hashes.optionsHash.has_value()
        && hashes.optionsHash.value()
            == ComputeMeshRepairOptionsHash(request.evidence->options);
    if (!pass)
    {
        BlockValidation(
            validation,
            "blocked_hash_consistency",
            MeshRepairErrorCode::HashNondeterministic,
            "MESH_REPAIR_HASH_INCONSISTENT",
            "stored repair hashes do not match independently recomputed canonical hashes");
        return false;
    }
    validation.hashConsistencyPass = true;
    return true;
}

void ValidateRequest(const MeshRepairEvidenceValidationRequest& request)
{
    if (request.originalMesh == nullptr
        || request.candidateMesh == nullptr
        || request.evidence == nullptr)
    {
        throw MeshRepairError(
            MeshRepairErrorCode::InputInvalid,
            "mesh repair evidence validation requires original, candidate, and evidence");
    }
    if (request.originalMesh->triangle_attributes.size()
            != request.originalMesh->mesh.triangles.size()
        || request.candidateMesh->triangle_attributes.size()
            != request.candidateMesh->mesh.triangles.size())
    {
        throw MeshRepairError(
            MeshRepairErrorCode::AttributeMismatch,
            "mesh repair evidence validation requires one attribute record per triangle");
    }
}

}  // namespace

MeshRepairEvidenceValidationResult ValidateMeshRepairEvidence(
    const MeshRepairEvidenceValidationRequest& request)
{
    ValidateRequest(request);
    MeshRepairEvidenceValidationResult result;
    const Clock::time_point attributeStart = Clock::now();

    std::set<std::size_t> retainedOutputs;
    std::set<std::size_t> generatedOutputs;
    std::map<std::size_t, std::size_t> originalTriangleBySource;
    if (!ValidateOperationSequence(*request.evidence, result.validation)
        || !ValidateSourceMappings(
            *request.originalMesh,
            *request.candidateMesh,
            *request.evidence,
            result.validation,
            retainedOutputs,
            originalTriangleBySource)
        || !ValidateVertexMappings(
            *request.originalMesh,
            *request.candidateMesh,
            *request.evidence,
            result.validation)
        || !ValidateGeneratedMappings(
            *request.candidateMesh,
            *request.evidence,
            retainedOutputs,
            result.validation,
            generatedOutputs)
        || !ValidateAttributes(
            *request.originalMesh,
            *request.candidateMesh,
            *request.evidence,
            originalTriangleBySource,
            result))
    {
        result.attributeValidationMs = ElapsedMilliseconds(attributeStart);
        return result;
    }
    result.attributeValidationMs = ElapsedMilliseconds(attributeStart);

    const Clock::time_point postStart = Clock::now();
    if (!ValidatePostStrict(request, result))
    {
        result.postDiagnosticsMs = ElapsedMilliseconds(postStart);
        return result;
    }
    result.postDiagnosticsMs = ElapsedMilliseconds(postStart);

    if (!ValidateHashes(request, result.validation))
    {
        return result;
    }
    if (request.evidence->productionOutputWritten
        || request.evidence->admission.productionAllowed)
    {
        BlockValidation(
            result.validation,
            "blocked_non_production_safety",
            MeshRepairErrorCode::OperationInvalid,
            "MESH_REPAIR_NON_PRODUCTION_SAFETY_FAILED",
            "R2 evidence validation must not admit or write production output");
        return result;
    }

    result.validation.status = "passed";
    result.validation.pass = true;
    result.validation.candidateAccepted = true;
    return result;
}

}  // namespace slicer_core
