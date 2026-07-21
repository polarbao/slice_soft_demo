#pragma once

#include "slicer_core/diagnostics/ValidationIssue.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Stable lifecycle states for the 12E mesh-repair evidence contract.
 */
enum class MeshRepairStatus
{
    NotEvaluated,
    StrictPassNoRepair,
    RepairCandidate,
    RepairedStrictPass,
    ManualRepairRequired,
    RejectedSelfIntersection,
    RepairFailed,
    DiagnosticOnly,
};

/**
 * @brief Stable eligibility classes used before any mesh mutation.
 */
enum class MeshRepairEligibilityClass
{
    Eligible,
    Conditional,
    ManualOnly,
    FailFast,
};

/**
 * @brief Stable operation kinds for deterministic repair evidence.
 */
enum class MeshRepairOperationType
{
    RemoveDegenerateTriangle,
    RemoveExactDuplicateFace,
    WeldVertex,
    FlipTriangleWinding,
    StitchBoundaryLoop,
    FillBoundaryLoop,
    SplitEdgeFan,
};

/**
 * @brief Attribute decision recorded for one repair operation.
 */
enum class MeshRepairAttributeDecision
{
    NotEvaluated,
    Preserved,
    GeneratedByPolicy,
    Conflict,
};

/**
 * @brief Stable disposition for one source triangle after a repair operation set.
 */
enum class MeshRepairTriangleDisposition
{
    Retained,
    RemovedDegenerate,
    RemovedExactDuplicate,
};

/**
 * @brief Stable primary patterns for one non-manifold edge.
 */
enum class MeshNonManifoldPattern
{
    DuplicateShellOrExporterDuplicate,
    SeparableLocalEdgeFan,
    OverlappingComponent,
    MixedWindingFan,
    AttributeConflictingFan,
    Unclassified,
};

/**
 * @brief Stable machine-readable errors for the 12E repair prerequisite.
 */
enum class MeshRepairErrorCode
{
    InputInvalid,
    RepairNotEnabled,
    Ineligible,
    AmbiguousTopology,
    BudgetExceeded,
    AttributeMismatch,
    PostStrictFailed,
    SelfIntersection,
    HashNondeterministic,
    ManualRequired,
    OperationInvalid,
};

/**
 * @brief Exception carrying a stable mesh-repair error code.
 */
class MeshRepairError : public std::runtime_error
{
public:
    /**
     * @brief Construct a mesh-repair contract error.
     * @param code Stable error code.
     * @param message Human-readable diagnostic message.
     */
    MeshRepairError(MeshRepairErrorCode code, const std::string& message);

    /**
     * @brief Return the stable error code.
     * @return Error code associated with this exception.
     */
    MeshRepairErrorCode Code() const noexcept;

private:
    MeshRepairErrorCode m_code;
};

/**
 * @brief Explicit options whose canonical form participates in repair evidence.
 */
struct MeshRepairOptions
{
    bool enabled{false};
    std::string mode{"strict_closed"};
    bool allowVertexWeld{false};
    double weldToleranceMm{0.0};
    bool allowWindingRepair{false};
    bool allowBoundaryFill{false};
    std::uint64_t maxBoundaryLoopEdges{0U};
    double maxBoundaryLoopDiameterMm{0.0};
    double maxBoundaryLoopPerimeterMm{0.0};
    double maxBoundaryPlanarityErrorMm{0.0};
    double maxHoleAreaMm2{0.0};
    double maxAffectedFaceRatio{0.0};
    bool allowNewFaces{false};
    std::string newFaceAttributePolicy{"reject"};
    bool validatePostRepairEvidence{false};
    bool classifyNonManifoldPatterns{false};
    bool analyzeCompleteSelfIntersections{false};
    std::uint64_t maxCompleteSelfIntersectionCandidatePairs{5000000U};
};

/**
 * @brief Stable input summary for a mesh-repair report.
 */
struct MeshRepairInputSummary
{
    std::string sourcePath;
    std::string inputFormat;
    std::array<double, 16> finalTransform{
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0};
    std::uint64_t vertexCount{0U};
    std::uint64_t triangleCount{0U};
    std::uint64_t componentCount{0U};
    std::uint64_t materialCount{0U};
    std::uint64_t textureResourceCount{0U};
};

/**
 * @brief Canonical SHA-256 values for repair input and decisions.
 */
struct MeshRepairHashes
{
    std::string algorithm{"sha256"};
    std::string canonicalizationVersion{"mesh_repair_canonical.1"};
    std::optional<std::string> sourceHash;
    std::optional<std::string> preRepairGeometryHash;
    std::optional<std::string> preRepairAttributeHash;
    std::optional<std::string> postRepairGeometryHash;
    std::optional<std::string> postRepairAttributeHash;
    std::optional<std::string> repairOperationHash;
    std::optional<std::string> optionsHash;
};

/**
 * @brief One issue-level automatic-repair eligibility decision.
 */
struct MeshRepairEligibilityDecision
{
    std::string issueCode;
    MeshRepairEligibilityClass classification{MeshRepairEligibilityClass::ManualOnly};
    bool eligible{false};
    std::string reasonCode;
    std::uint64_t affectedCount{0U};
    std::optional<double> threshold;
    std::string suggestedAction;
};

/**
 * @brief Aggregate repair eligibility without performing a repair.
 */
struct MeshRepairEligibility
{
    MeshRepairStatus status{MeshRepairStatus::NotEvaluated};
    bool automaticRepairAllowed{false};
    std::vector<MeshRepairEligibilityDecision> decisions;
};

/**
 * @brief Deterministic record for one candidate repair operation.
 */
struct MeshRepairOperation
{
    std::uint64_t operationId{0U};
    MeshRepairOperationType type{MeshRepairOperationType::RemoveDegenerateTriangle};
    std::string reasonCode;
    std::vector<std::uint64_t> inputElementIds;
    std::vector<std::uint64_t> outputElementIds;
    Json parameters{Json::object({})};
    MeshRepairAttributeDecision attributeDecision{MeshRepairAttributeDecision::NotEvaluated};
    std::uint64_t affectedVertices{0U};
    std::uint64_t affectedEdges{0U};
    std::uint64_t affectedFaces{0U};
    double durationMs{0.0};
};

/**
 * @brief Deterministic source-to-output triangle provenance.
 */
struct MeshRepairTriangleMapping
{
    std::uint64_t sourceTriangleIndex{0U};
    std::optional<std::uint64_t> outputTriangleIndex;
    MeshRepairTriangleDisposition disposition{MeshRepairTriangleDisposition::Retained};
    std::optional<std::uint64_t> retainedSourceTriangleIndex;
};

/**
 * @brief Deterministic output vertex provenance after a repair operation set.
 */
struct MeshRepairVertexMapping
{
    std::uint64_t outputVertexIndex{0U};
    std::vector<std::uint64_t> sourceVertexIndices;
};

/**
 * @brief Provenance and attribute policy for one generated boundary-fill face.
 */
struct MeshRepairGeneratedTriangleMapping
{
    std::uint64_t outputTriangleIndex{0U};
    std::vector<std::uint64_t> generatingBoundaryVertexIndices;
    std::string attributePolicy;
    std::string materialName;
    bool hasUv{false};
};

/**
 * @brief Topology facts captured before or after a repair candidate.
 */
struct MeshRepairDiagnosticsSummary
{
    bool available{false};
    bool strictPass{false};
    std::uint64_t boundaryEdges{0U};
    std::uint64_t nonManifoldEdges{0U};
    std::uint64_t duplicateFaces{0U};
    std::uint64_t oppositeDuplicateFaces{0U};
    std::uint64_t localWindingIssues{0U};
    std::uint64_t degenerateTriangles{0U};
    std::uint64_t connectedComponents{0U};
    std::uint64_t confirmedSelfIntersectionPairs{0U};
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Attribute-preservation evidence for a repair candidate.
 */
struct MeshRepairAttributePreservation
{
    std::string status{"not_evaluated"};
    std::uint64_t sourceMappedTriangles{0U};
    std::uint64_t newTriangles{0U};
    std::uint64_t unknownSourceTriangles{0U};
    std::uint64_t materialConflicts{0U};
    std::uint64_t uvConflicts{0U};
    std::uint64_t missingTextureResources{0U};
    std::uint64_t fallbackTriangles{0U};
    std::optional<double> maxUvDelta;
    bool pass{false};
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Independent R2-04 validation gates for one post-repair candidate.
 */
struct MeshRepairEvidenceValidation
{
    std::string status{"not_evaluated"};
    bool pass{false};
    bool operationSequencePass{false};
    bool sourceMappingPass{false};
    bool vertexMappingPass{false};
    bool generatedMappingPass{false};
    bool attributePass{false};
    bool postStrictComplete{false};
    bool postStrictPass{false};
    bool hashConsistencyPass{false};
    bool candidateAccepted{false};
    std::vector<std::string> blockerCodes;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Deterministic structural evidence for one non-manifold edge.
 */
struct MeshNonManifoldEdgeAnalysis
{
    std::array<std::uint64_t, 2> edgeVertexIndices{0U, 0U};
    std::vector<std::uint64_t> incidentTriangleIndices;
    std::vector<std::uint64_t> incidentSourceTriangleIndices;
    std::vector<std::uint64_t> residualComponentIds;
    std::uint64_t forwardUses{0U};
    std::uint64_t reverseUses{0U};
    MeshNonManifoldPattern pattern{MeshNonManifoldPattern::Unclassified};
    bool duplicateGeometry{false};
    bool attributeConflict{false};
    bool mixedWinding{false};
    bool uniqueFanSplitFeasible{false};
    std::string reasonCode;
};

/**
 * @brief Aggregate R3-01 non-manifold pattern classification.
 */
struct MeshNonManifoldAnalysis
{
    std::string status{"not_evaluated"};
    bool complete{false};
    bool allEdgesClassified{false};
    bool allUniqueFanSplitsFeasible{false};
    std::uint64_t nonManifoldEdgeCount{0U};
    std::uint64_t duplicateShellOrExporterDuplicateEdges{0U};
    std::uint64_t separableLocalEdgeFanEdges{0U};
    std::uint64_t overlappingComponentEdges{0U};
    std::uint64_t mixedWindingFanEdges{0U};
    std::uint64_t attributeConflictingFanEdges{0U};
    std::uint64_t unclassifiedEdges{0U};
    std::vector<MeshNonManifoldEdgeAnalysis> edges;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Deterministic R3-01A complete self-intersection evidence.
 */
struct MeshCompleteSelfIntersectionAnalysis
{
    std::string status{"not_evaluated"};
    bool complete{false};
    std::uint64_t triangleCount{0U};
    std::uint64_t bvhNodeCount{0U};
    std::uint64_t candidatePairCount{0U};
    std::uint64_t testedPairCount{0U};
    std::uint64_t confirmedIntersectionPairs{0U};
    std::uint64_t coplanarOverlapPairs{0U};
    std::uint64_t touchingOnlyPairs{0U};
    std::uint64_t aabbOnlyPairs{0U};
    std::optional<std::string> candidatePairHash;
    std::optional<double> durationMs;
    std::optional<std::uint64_t> peakWorkingSetBytes;
    std::string blockerCode;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Non-production admission evidence produced by the repair prerequisite.
 */
struct MeshRepairAdmission
{
    std::string mode{"repair_then_strict"};
    std::string status{"non_production_only"};
    bool postRepairStrictPass{false};
    bool productionAllowed{false};
    std::vector<std::string> blockerCodes;
    std::vector<std::string> warningCodes;
    std::vector<std::string> suggestedActions;
};

/**
 * @brief Core-only timing and memory placeholders for repair evidence.
 */
struct MeshRepairPerformance
{
    std::optional<double> diagnosticsMs;
    std::optional<double> eligibilityMs;
    std::optional<double> repairMs;
    std::optional<double> attributeValidationMs;
    std::optional<double> postDiagnosticsMs;
    std::optional<double> hashMs;
    std::optional<double> totalRepairCoreMs;
    std::optional<std::uint64_t> peakWorkingSetBytes;
};

/**
 * @brief Backend-neutral in-memory result for the repair report contract.
 */
struct MeshRepairResult
{
    MeshRepairStatus status{MeshRepairStatus::NotEvaluated};
    std::string mode{"strict_closed"};
    bool repairEnabled{false};
    bool repairAttempted{false};
    bool productionOutputWritten{false};
    MeshRepairInputSummary input;
    MeshRepairOptions options;
    MeshRepairHashes hashes;
    MeshRepairDiagnosticsSummary preRepair;
    MeshRepairEligibility eligibility;
    std::vector<MeshRepairOperation> operations;
    std::vector<MeshRepairTriangleMapping> sourceMappings;
    std::vector<MeshRepairVertexMapping> vertexMappings;
    std::vector<MeshRepairGeneratedTriangleMapping> generatedTriangleMappings;
    MeshRepairAttributePreservation attributePreservation;
    MeshRepairEvidenceValidation evidenceValidation;
    MeshNonManifoldAnalysis nonManifoldAnalysis;
    MeshCompleteSelfIntersectionAnalysis completeSelfIntersectionAnalysis;
    MeshRepairDiagnosticsSummary postRepair;
    MeshRepairAdmission admission;
    MeshRepairPerformance performance;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Convert a repair status to its stable report name.
 * @param status Repair status.
 * @return Stable status text.
 */
std::string MeshRepairStatusName(MeshRepairStatus status);

/**
 * @brief Convert an eligibility class to its stable report name.
 * @param classification Eligibility class.
 * @return Stable classification text.
 */
std::string MeshRepairEligibilityClassName(MeshRepairEligibilityClass classification);

/**
 * @brief Convert an operation type to its stable report name.
 * @param type Operation type.
 * @return Stable operation type text.
 */
std::string MeshRepairOperationTypeName(MeshRepairOperationType type);

/**
 * @brief Convert an attribute decision to its stable report name.
 * @param decision Attribute decision.
 * @return Stable attribute decision text.
 */
std::string MeshRepairAttributeDecisionName(MeshRepairAttributeDecision decision);

/**
 * @brief Convert a triangle disposition to its stable report name.
 * @param disposition Triangle provenance disposition.
 * @return Stable disposition text.
 */
std::string MeshRepairTriangleDispositionName(MeshRepairTriangleDisposition disposition);

/**
 * @brief Convert a non-manifold primary pattern to its stable report name.
 * @param pattern Pattern value.
 * @return Stable pattern name.
 */
std::string MeshNonManifoldPatternName(MeshNonManifoldPattern pattern);

/**
 * @brief Convert a repair error code to its stable machine-readable name.
 * @param code Error code.
 * @return Stable E_12E_REPAIR_* name.
 */
std::string MeshRepairErrorCodeName(MeshRepairErrorCode code);

}  // namespace slicer_core
