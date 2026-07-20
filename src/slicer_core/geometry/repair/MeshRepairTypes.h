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
    double weldToleranceMm{0.0};
    std::uint64_t maxBoundaryLoopEdges{0U};
    double maxBoundaryLoopDiameterMm{0.0};
    bool allowNewFaces{false};
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
    MeshRepairAttributePreservation attributePreservation;
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
 * @brief Convert a repair error code to its stable machine-readable name.
 * @param code Error code.
 * @return Stable E_12E_REPAIR_* name.
 */
std::string MeshRepairErrorCodeName(MeshRepairErrorCode code);

}  // namespace slicer_core
