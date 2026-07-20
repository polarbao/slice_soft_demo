#include "slicer_core/diagnostics/MeshRepairReport.h"

#include <optional>
#include <string>

namespace slicer_core
{
namespace
{

Json OptionalString(const std::optional<std::string>& value)
{
    return value.has_value() ? Json{value.value()} : Json{nullptr};
}

Json OptionalDouble(const std::optional<double>& value)
{
    return value.has_value() ? Json{value.value()} : Json{nullptr};
}

Json OptionalUnsigned(const std::optional<std::uint64_t>& value)
{
    return value.has_value() ? Json{value.value()} : Json{nullptr};
}

Json StringsToJson(const std::vector<std::string>& values)
{
    Json::Array array;
    for (const std::string& value : values)
    {
        array.emplace_back(value);
    }
    return Json{std::move(array)};
}

Json BuildInput(const MeshRepairInputSummary& input)
{
    Json::Array transform;
    for (const double value : input.finalTransform)
    {
        transform.emplace_back(value);
    }
    return Json::object({
        {"sourcePath", input.sourcePath},
        {"inputFormat", input.inputFormat},
        {"finalTransform", Json{std::move(transform)}},
        {"vertexCount", input.vertexCount},
        {"triangleCount", input.triangleCount},
        {"componentCount", input.componentCount},
        {"materialCount", input.materialCount},
        {"textureResourceCount", input.textureResourceCount},
    });
}

Json BuildOptions(const MeshRepairOptions& options)
{
    return Json::object({
        {"enabled", options.enabled},
        {"mode", options.mode},
        {"allowVertexWeld", options.allowVertexWeld},
        {"weldToleranceMm", options.weldToleranceMm},
        {"allowWindingRepair", options.allowWindingRepair},
        {"allowBoundaryFill", options.allowBoundaryFill},
        {"maxBoundaryLoopEdges", options.maxBoundaryLoopEdges},
        {"maxBoundaryLoopDiameterMm", options.maxBoundaryLoopDiameterMm},
        {"maxBoundaryLoopPerimeterMm", options.maxBoundaryLoopPerimeterMm},
        {"maxBoundaryPlanarityErrorMm", options.maxBoundaryPlanarityErrorMm},
        {"maxHoleAreaMm2", options.maxHoleAreaMm2},
        {"maxAffectedFaceRatio", options.maxAffectedFaceRatio},
        {"allowNewFaces", options.allowNewFaces},
        {"newFaceAttributePolicy", options.newFaceAttributePolicy},
    });
}

Json BuildHashes(const MeshRepairHashes& hashes)
{
    return Json::object({
        {"algorithm", hashes.algorithm},
        {"canonicalizationVersion", hashes.canonicalizationVersion},
        {"sourceHash", OptionalString(hashes.sourceHash)},
        {"preRepairGeometryHash", OptionalString(hashes.preRepairGeometryHash)},
        {"preRepairAttributeHash", OptionalString(hashes.preRepairAttributeHash)},
        {"postRepairGeometryHash", OptionalString(hashes.postRepairGeometryHash)},
        {"postRepairAttributeHash", OptionalString(hashes.postRepairAttributeHash)},
        {"repairOperationHash", OptionalString(hashes.repairOperationHash)},
        {"optionsHash", OptionalString(hashes.optionsHash)},
    });
}

Json BuildDiagnostics(const MeshRepairDiagnosticsSummary& diagnostics)
{
    return Json::object({
        {"available", diagnostics.available},
        {"strictPass", diagnostics.strictPass},
        {"boundaryEdges", diagnostics.boundaryEdges},
        {"nonManifoldEdges", diagnostics.nonManifoldEdges},
        {"duplicateFaces", diagnostics.duplicateFaces},
        {"oppositeDuplicateFaces", diagnostics.oppositeDuplicateFaces},
        {"localWindingIssues", diagnostics.localWindingIssues},
        {"degenerateTriangles", diagnostics.degenerateTriangles},
        {"connectedComponents", diagnostics.connectedComponents},
        {"confirmedSelfIntersectionPairs", diagnostics.confirmedSelfIntersectionPairs},
        {"issues", ValidationIssuesToJson(diagnostics.issues)},
    });
}

Json BuildEligibility(const MeshRepairEligibility& eligibility)
{
    Json::Array decisions;
    for (const MeshRepairEligibilityDecision& decision : eligibility.decisions)
    {
        decisions.push_back(Json::object({
            {"issueCode", decision.issueCode},
            {"classification", MeshRepairEligibilityClassName(decision.classification)},
            {"eligible", decision.eligible},
            {"reasonCode", decision.reasonCode},
            {"affectedCount", decision.affectedCount},
            {"threshold", OptionalDouble(decision.threshold)},
            {"suggestedAction", decision.suggestedAction},
        }));
    }
    return Json::object({
        {"status", MeshRepairStatusName(eligibility.status)},
        {"automaticRepairAllowed", eligibility.automaticRepairAllowed},
        {"decisions", Json{std::move(decisions)}},
    });
}

Json BuildOperations(const std::vector<MeshRepairOperation>& operations)
{
    Json::Array array;
    for (const MeshRepairOperation& operation : operations)
    {
        Json::Array inputIds;
        for (const std::uint64_t id : operation.inputElementIds)
        {
            inputIds.emplace_back(id);
        }
        Json::Array outputIds;
        for (const std::uint64_t id : operation.outputElementIds)
        {
            outputIds.emplace_back(id);
        }
        array.push_back(Json::object({
            {"operationId", operation.operationId},
            {"type", MeshRepairOperationTypeName(operation.type)},
            {"reasonCode", operation.reasonCode},
            {"inputElementIds", Json{std::move(inputIds)}},
            {"outputElementIds", Json{std::move(outputIds)}},
            {"parameters", operation.parameters},
            {"attributeDecision", MeshRepairAttributeDecisionName(operation.attributeDecision)},
            {"affectedVertices", operation.affectedVertices},
            {"affectedEdges", operation.affectedEdges},
            {"affectedFaces", operation.affectedFaces},
            {"durationMs", operation.durationMs},
        }));
    }
    return Json{std::move(array)};
}

Json BuildSourceMappings(const std::vector<MeshRepairTriangleMapping>& mappings)
{
    Json::Array array;
    for (const MeshRepairTriangleMapping& mapping : mappings)
    {
        array.push_back(Json::object({
            {"sourceTriangleIndex", mapping.sourceTriangleIndex},
            {"outputTriangleIndex", OptionalUnsigned(mapping.outputTriangleIndex)},
            {"disposition", MeshRepairTriangleDispositionName(mapping.disposition)},
            {"retainedSourceTriangleIndex", OptionalUnsigned(mapping.retainedSourceTriangleIndex)},
        }));
    }
    return Json{std::move(array)};
}

Json BuildVertexMappings(const std::vector<MeshRepairVertexMapping>& mappings)
{
    Json::Array array;
    for (const MeshRepairVertexMapping& mapping : mappings)
    {
        Json::Array sources;
        for (const std::uint64_t sourceIndex : mapping.sourceVertexIndices)
        {
            sources.push_back(sourceIndex);
        }
        array.push_back(Json::object({
            {"outputVertexIndex", mapping.outputVertexIndex},
            {"sourceVertexIndices", Json{std::move(sources)}},
        }));
    }
    return Json{std::move(array)};
}

Json BuildGeneratedTriangleMappings(
    const std::vector<MeshRepairGeneratedTriangleMapping>& mappings)
{
    Json::Array array;
    for (const MeshRepairGeneratedTriangleMapping& mapping : mappings)
    {
        Json::Array vertices;
        for (const std::uint64_t vertexIndex : mapping.generatingBoundaryVertexIndices)
        {
            vertices.push_back(vertexIndex);
        }
        array.push_back(Json::object({
            {"outputTriangleIndex", mapping.outputTriangleIndex},
            {"generatingBoundaryVertexIndices", Json{std::move(vertices)}},
            {"attributePolicy", mapping.attributePolicy},
            {"materialName", mapping.materialName},
            {"hasUv", mapping.hasUv},
        }));
    }
    return Json{std::move(array)};
}

Json BuildAttributePreservation(const MeshRepairAttributePreservation& attributes)
{
    return Json::object({
        {"status", attributes.status},
        {"sourceMappedTriangles", attributes.sourceMappedTriangles},
        {"newTriangles", attributes.newTriangles},
        {"unknownSourceTriangles", attributes.unknownSourceTriangles},
        {"materialConflicts", attributes.materialConflicts},
        {"uvConflicts", attributes.uvConflicts},
        {"missingTextureResources", attributes.missingTextureResources},
        {"fallbackTriangles", attributes.fallbackTriangles},
        {"maxUvDelta", OptionalDouble(attributes.maxUvDelta)},
        {"pass", attributes.pass},
        {"issues", ValidationIssuesToJson(attributes.issues)},
    });
}

Json BuildAdmission(const MeshRepairAdmission& admission)
{
    return Json::object({
        {"mode", admission.mode},
        {"status", admission.status},
        {"postRepairStrictPass", admission.postRepairStrictPass},
        {"productionAllowed", admission.productionAllowed},
        {"blockerCodes", StringsToJson(admission.blockerCodes)},
        {"warningCodes", StringsToJson(admission.warningCodes)},
        {"suggestedActions", StringsToJson(admission.suggestedActions)},
    });
}

Json BuildPerformance(const MeshRepairPerformance& performance)
{
    return Json::object({
        {"diagnosticsMs", OptionalDouble(performance.diagnosticsMs)},
        {"eligibilityMs", OptionalDouble(performance.eligibilityMs)},
        {"repairMs", OptionalDouble(performance.repairMs)},
        {"attributeValidationMs", OptionalDouble(performance.attributeValidationMs)},
        {"postDiagnosticsMs", OptionalDouble(performance.postDiagnosticsMs)},
        {"hashMs", OptionalDouble(performance.hashMs)},
        {"totalRepairCoreMs", OptionalDouble(performance.totalRepairCoreMs)},
        {"peakWorkingSetBytes", OptionalUnsigned(performance.peakWorkingSetBytes)},
    });
}

}  // namespace

Json BuildMeshRepairReportSkeleton(
    const MeshRepairInputSummary& input,
    const MeshRepairOptions& options,
    const MeshRepairHashes& hashes)
{
    MeshRepairResult result;
    result.mode = options.mode;
    result.repairEnabled = options.enabled;
    result.input = input;
    result.options = options;
    result.hashes = hashes;
    return BuildMeshRepairReport(result);
}

Json BuildMeshRepairReport(const MeshRepairResult& result)
{
    return Json::object({
        {"schema", "slicesoft.mesh_repair.12e_08c.1"},
        {"status", MeshRepairStatusName(result.status)},
        {"mode", result.mode},
        {"repairEnabled", result.repairEnabled},
        {"repairAttempted", result.repairAttempted},
        {"productionOutputWritten", result.productionOutputWritten},
        {"input", BuildInput(result.input)},
        {"options", BuildOptions(result.options)},
        {"hashes", BuildHashes(result.hashes)},
        {"preRepair", BuildDiagnostics(result.preRepair)},
        {"eligibility", BuildEligibility(result.eligibility)},
        {"operations", BuildOperations(result.operations)},
        {"sourceMappings", BuildSourceMappings(result.sourceMappings)},
        {"vertexMappings", BuildVertexMappings(result.vertexMappings)},
        {"generatedTriangleMappings", BuildGeneratedTriangleMappings(
            result.generatedTriangleMappings)},
        {"attributePreservation", BuildAttributePreservation(result.attributePreservation)},
        {"postRepair", BuildDiagnostics(result.postRepair)},
        {"admission", BuildAdmission(result.admission)},
        {"performance", BuildPerformance(result.performance)},
        {"issues", ValidationIssuesToJson(result.issues)},
    });
}

}  // namespace slicer_core
