#include "slicer_core/pipeline/TextureFillPartitionDiagnosticFacade.h"

#include <cmath>
#include <limits>
#include <utility>

namespace
{

using slicer_core::Json;
using slicer_core::TextureFillPartitionDiagnosticIssueDto;
using slicer_core::TextureFillPartitionDiagnosticState;
using slicer_core::TextureFillPartitionDiagnosticUiDto;

constexpr const char* kTextureFillPartitionSchema =
    "slicesoft.texture_fill_partition.12e.1";

const Json* Find(const Json& object, const std::string& key)
{
    if (!object.is_object())
    {
        return nullptr;
    }
    const auto& values = object.as_object();
    const auto found = values.find(key);
    return found == values.end() ? nullptr : &found->second;
}

const Json* FindObject(const Json& object, const std::string& key)
{
    const Json* value = Find(object, key);
    return value != nullptr && value->is_object() ? value : nullptr;
}

std::optional<std::string> ReadString(
    const Json& object,
    const std::string& key)
{
    const Json* value = Find(object, key);
    if (value == nullptr || !value->is_string())
    {
        return std::nullopt;
    }
    return value->as_string();
}

std::optional<bool> ReadBool(const Json& object, const std::string& key)
{
    const Json* value = Find(object, key);
    if (value == nullptr || !value->is_bool())
    {
        return std::nullopt;
    }
    return value->as_bool();
}

std::optional<double> ReadDouble(const Json& object, const std::string& key)
{
    const Json* value = Find(object, key);
    if (value == nullptr || !value->is_number())
    {
        return std::nullopt;
    }
    const double number = value->as_double();
    return std::isfinite(number) ? std::optional<double>{number} : std::nullopt;
}

std::optional<std::uint64_t> ReadCount(
    const Json& object,
    const std::string& key)
{
    const std::optional<double> number = ReadDouble(object, key);
    if (!number.has_value()
        || number.value() < 0.0
        || std::floor(number.value()) != number.value()
        || number.value()
            >= static_cast<double>(std::numeric_limits<std::uint64_t>::max()))
    {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(number.value());
}

void AppendFacadeIssue(
    TextureFillPartitionDiagnosticUiDto& dto,
    const std::string& code,
    const std::string& message)
{
    TextureFillPartitionDiagnosticIssueDto issue;
    issue.code = code;
    issue.severity = "error";
    issue.message = message;
    issue.context = Json::object({});
    dto.issues.push_back(std::move(issue));
}

void MarkMissingSection(
    TextureFillPartitionDiagnosticUiDto& dto,
    const std::string& sectionName)
{
    dto.schemavalid = false;
    AppendFacadeIssue(
        dto,
        "E_12E_DIAGNOSTIC_REPORT_INVALID",
        "texture/fill diagnostic report is missing object section: "
            + sectionName);
}

void ReadIssues(
    const Json& report,
    TextureFillPartitionDiagnosticUiDto& dto)
{
    const Json* issues = Find(report, "issues");
    if (issues == nullptr || !issues->is_array())
    {
        AppendFacadeIssue(
            dto,
            "E_12E_DIAGNOSTIC_REPORT_INVALID",
            "texture/fill diagnostic report issues must be an array");
        dto.schemavalid = false;
        return;
    }

    for (const Json& value : issues->as_array())
    {
        if (!value.is_object())
        {
            AppendFacadeIssue(
                dto,
                "E_12E_DIAGNOSTIC_REPORT_INVALID",
                "texture/fill diagnostic report contains a non-object issue");
            dto.schemavalid = false;
            continue;
        }
        const std::optional<std::string> code = ReadString(value, "code");
        const std::optional<std::string> severity = ReadString(value, "severity");
        const std::optional<std::string> message = ReadString(value, "message");
        if (!code.has_value() || !severity.has_value() || !message.has_value())
        {
            AppendFacadeIssue(
                dto,
                "E_12E_DIAGNOSTIC_REPORT_INVALID",
                "texture/fill diagnostic issue is missing code, severity, or message");
            dto.schemavalid = false;
            continue;
        }
        if (severity.value() != "info"
            && severity.value() != "warning"
            && severity.value() != "error")
        {
            AppendFacadeIssue(
                dto,
                "E_12E_DIAGNOSTIC_REPORT_INVALID",
                "texture/fill diagnostic issue contains an unsupported severity");
            dto.schemavalid = false;
            continue;
        }

        TextureFillPartitionDiagnosticIssueDto issue;
        issue.code = code.value();
        issue.severity = severity.value();
        issue.message = message.value();
        const Json* context = Find(value, "context");
        issue.context = context == nullptr ? Json::object({}) : *context;
        dto.issues.push_back(std::move(issue));
    }
}

void ReadWidth(
    const Json& report,
    TextureFillPartitionDiagnosticUiDto& dto)
{
    const Json* width = FindObject(report, "width");
    if (width == nullptr)
    {
        MarkMissingSection(dto, "width");
        return;
    }

    dto.widthmetrics.requestedwidthmm = ReadDouble(*width, "requestedWidthMm");
    dto.widthmetrics.widthstepmm = ReadDouble(*width, "widthStepMm");
    if (dto.state != TextureFillPartitionDiagnosticState::Diagnostic)
    {
        return;
    }
    dto.widthmetrics.effectiveminimumwidthmm =
        ReadDouble(*width, "effectiveMinimumWidthMm");
    dto.widthmetrics.effectivewidthmm = ReadDouble(*width, "effectiveWidthMm");
    dto.widthmetrics.maxinteriordistancemm =
        ReadDouble(*width, "maxInteriorDistanceMm");
    dto.widthmetrics.alltexturethresholdmm =
        ReadDouble(*width, "allTextureThresholdMm");
    dto.widthmetrics.alltexture = ReadBool(*width, "allTexture");
}

void ReadPartition(
    const Json& report,
    TextureFillPartitionDiagnosticUiDto& dto)
{
    const Json* partition = FindObject(report, "partition");
    if (partition == nullptr)
    {
        MarkMissingSection(dto, "partition");
        return;
    }

    const std::optional<bool> partitionPass = ReadBool(*partition, "partitionPass");
    if (dto.state == TextureFillPartitionDiagnosticState::Diagnostic
        && (!partitionPass.has_value() || !partitionPass.value()))
    {
        dto.schemavalid = false;
        dto.state = TextureFillPartitionDiagnosticState::Blocked;
        AppendFacadeIssue(
            dto,
            "E_12E_DIAGNOSTIC_REPORT_INVALID",
            "diagnostic report requires a passing texture/fill partition");
        return;
    }
    dto.partitionstats.evaluated =
        dto.state == TextureFillPartitionDiagnosticState::Diagnostic
        && partitionPass.has_value();
    if (!dto.partitionstats.evaluated)
    {
        return;
    }

    dto.partitionstats.modelvoxels = ReadCount(*partition, "modelVoxels");
    dto.partitionstats.texturesurfacevoxels =
        ReadCount(*partition, "textureSurfaceVoxels");
    dto.partitionstats.modelfillvoxels = ReadCount(*partition, "modelFillVoxels");
    dto.partitionstats.overlapvoxels =
        ReadCount(*partition, "overlapTextureFillVoxels");
    dto.partitionstats.unassignedvoxels =
        ReadCount(*partition, "unassignedModelVoxels");
    dto.partitionstats.texturecoverageratio =
        ReadDouble(*partition, "textureCoverageRatio");
    dto.partitionstats.modelfillcoverageratio =
        ReadDouble(*partition, "modelFillCoverageRatio");
    dto.partitionstats.partitionpass = partitionPass;
}

bool IsEvaluatedSection(const Json& section)
{
    const std::optional<std::string> availability =
        ReadString(section, "availability");
    const std::optional<std::string> status = ReadString(section, "status");
    return availability == std::optional<std::string>{"available"}
        && (status == std::optional<std::string>{"diagnostic"}
            || status == std::optional<std::string>{"pass"});
}

void ReadRasterMapping(
    const Json& report,
    TextureFillPartitionDiagnosticUiDto& dto)
{
    const Json* mapping = FindObject(report, "rasterMapping");
    if (mapping == nullptr)
    {
        MarkMissingSection(dto, "rasterMapping");
        return;
    }
    dto.rastermapping.availability =
        ReadString(*mapping, "availability").value_or("unavailable");
    dto.rastermapping.status =
        ReadString(*mapping, "status").value_or("not_evaluated");
    if (dto.rastermapping.status == "blocked"
        || dto.rastermapping.status == "fail")
    {
        dto.state = TextureFillPartitionDiagnosticState::Blocked;
        return;
    }
    dto.rastermapping.evaluated =
        dto.state == TextureFillPartitionDiagnosticState::Diagnostic
        && IsEvaluatedSection(*mapping);
    if (!dto.rastermapping.evaluated)
    {
        return;
    }
    dto.rastermapping.rastervoxelcount = ReadCount(*mapping, "rasterVoxelCount");
    dto.rastermapping.modelrastervoxels = ReadCount(*mapping, "modelRasterVoxels");
    dto.rastermapping.texturesurfacerastervoxels =
        ReadCount(*mapping, "textureSurfaceRasterVoxels");
    dto.rastermapping.modelfillrastervoxels =
        ReadCount(*mapping, "modelFillRasterVoxels");
    dto.rastermapping.overlaprastervoxels =
        ReadCount(*mapping, "overlapRasterVoxels");
    dto.rastermapping.unassignedmodelrastervoxels =
        ReadCount(*mapping, "unassignedModelRasterVoxels");
    dto.rastermapping.partitionpass = ReadBool(*mapping, "partitionPass");
    dto.rastermapping.layercount = ReadCount(*mapping, "layerCount");
    if (!dto.rastermapping.partitionpass.has_value()
        || !dto.rastermapping.partitionpass.value())
    {
        dto.schemavalid = false;
        dto.state = TextureFillPartitionDiagnosticState::Blocked;
        dto.rastermapping.evaluated = false;
        AppendFacadeIssue(
            dto,
            "E_12E_DIAGNOSTIC_REPORT_INVALID",
            "diagnostic raster mapping requires a passing partition");
    }
}

void ReadFullClosure(
    const Json& report,
    TextureFillPartitionDiagnosticUiDto& dto)
{
    const Json* closure = FindObject(report, "fullClosureLinkage");
    if (closure == nullptr)
    {
        MarkMissingSection(dto, "fullClosureLinkage");
        return;
    }
    dto.fullclosurelinkage.availability =
        ReadString(*closure, "availability").value_or("unavailable");
    dto.fullclosurelinkage.status =
        ReadString(*closure, "status").value_or("not_evaluated");
    if (dto.fullclosurelinkage.status == "blocked"
        || dto.fullclosurelinkage.status == "fail")
    {
        dto.state = TextureFillPartitionDiagnosticState::Blocked;
    }
    dto.fullclosurelinkage.modelclosurestatus =
        ReadString(*closure, "modelClosureStatus").value_or("not_evaluated");
    dto.fullclosurelinkage.supportclosurestatus =
        ReadString(*closure, "supportClosureStatus").value_or("not_evaluated");
    dto.fullclosurelinkage.varnishclosurestatus =
        ReadString(*closure, "varnishClosureStatus").value_or("not_evaluated");
    dto.fullclosurelinkage.productionoutputwritten =
        ReadBool(*closure, "productionOutputWritten").value_or(false);
    dto.productionoutputwritten =
        dto.productionoutputwritten
        || dto.fullclosurelinkage.productionoutputwritten;
    dto.fullclosurelinkage.evaluated =
        dto.state == TextureFillPartitionDiagnosticState::Diagnostic
        && IsEvaluatedSection(*closure);
    if (!dto.fullclosurelinkage.evaluated)
    {
        return;
    }
    dto.fullclosurelinkage.fullclosurepass =
        ReadBool(*closure, "fullClosurePass");
    dto.fullclosurelinkage.expecteddomaingappixels =
        ReadCount(*closure, "expectedDomainGapPixels");
    dto.fullclosurelinkage.modeldomaingappixels =
        ReadCount(*closure, "modelDomainGapPixels");
    dto.fullclosurelinkage.supportrequiredgappixels =
        ReadCount(*closure, "supportRequiredGapPixels");
    dto.fullclosurelinkage.outervarnishgappixels =
        ReadCount(*closure, "outerVarnishGapPixels");
    dto.fullclosurelinkage.unexpectedoccupiedpixels =
        ReadCount(*closure, "unexpectedOccupiedPixels");
    if (!dto.fullclosurelinkage.fullclosurepass.has_value()
        || !dto.fullclosurelinkage.fullclosurepass.value())
    {
        dto.schemavalid = false;
        dto.state = TextureFillPartitionDiagnosticState::Blocked;
        dto.fullclosurelinkage.evaluated = false;
        AppendFacadeIssue(
            dto,
            "E_12E_DIAGNOSTIC_REPORT_INVALID",
            "diagnostic full-material closure requires a passing invariant");
    }
}

void ReadPerformance(
    const Json& report,
    TextureFillPartitionDiagnosticUiDto& dto)
{
    const Json* performance = FindObject(report, "performance");
    if (performance == nullptr)
    {
        MarkMissingSection(dto, "performance");
        return;
    }
    dto.performance.evaluated =
        dto.state == TextureFillPartitionDiagnosticState::Diagnostic;
    if (!dto.performance.evaluated)
    {
        return;
    }
    dto.performance.preflightms = ReadDouble(*performance, "preflightMs");
    dto.performance.topologyms = ReadDouble(*performance, "topologyMs");
    dto.performance.levelsetms = ReadDouble(*performance, "levelSetMs");
    dto.performance.gridsamplems = ReadDouble(*performance, "gridSampleMs");
    dto.performance.occupancyms = ReadDouble(*performance, "occupancyMs");
    dto.performance.distancems = ReadDouble(*performance, "distanceMs");
    dto.performance.partitionms = ReadDouble(*performance, "partitionMs");
    dto.performance.texturetransferms =
        ReadDouble(*performance, "textureTransferMs");
    dto.performance.rastermappingms =
        ReadDouble(*performance, "rasterMappingMs");
    dto.performance.fullclosurems = ReadDouble(*performance, "fullClosureMs");
    dto.performance.totalcorems = ReadDouble(*performance, "totalCoreMs");
    dto.performance.gridvoxelcount = ReadCount(*performance, "gridVoxelCount");
    dto.performance.peakworkingsetbytes =
        ReadCount(*performance, "peakWorkingSetBytes");
}

void ReadProductionOutputFlag(
    const Json& report,
    const std::string& sectionName,
    TextureFillPartitionDiagnosticUiDto& dto)
{
    const Json* section = FindObject(report, sectionName);
    if (section == nullptr)
    {
        return;
    }
    dto.productionoutputwritten = dto.productionoutputwritten
        || ReadBool(*section, "productionOutputWritten").value_or(false);
}

}  // namespace

namespace slicer_core
{

TextureFillPartitionDiagnosticUiDto
TextureFillPartitionDiagnosticFacade::Pending()
{
    return {};
}

TextureFillPartitionDiagnosticUiDto
TextureFillPartitionDiagnosticFacade::Unavailable(const std::string& reason)
{
    TextureFillPartitionDiagnosticUiDto dto;
    dto.state = TextureFillPartitionDiagnosticState::Unavailable;
    dto.status = "unavailable";
    AppendFacadeIssue(
        dto,
        "E_12E_DIAGNOSTIC_REPORT_UNAVAILABLE",
        reason.empty() ? "texture/fill diagnostic report is unavailable" : reason);
    return dto;
}

TextureFillPartitionDiagnosticUiDto
TextureFillPartitionDiagnosticFacade::Inspect(const Json& report)
{
    TextureFillPartitionDiagnosticUiDto dto;
    dto.reportavailable = true;
    dto.schemavalid = report.is_object();
    if (!report.is_object())
    {
        dto.state = TextureFillPartitionDiagnosticState::Blocked;
        dto.status = "blocked";
        AppendFacadeIssue(
            dto,
            "E_12E_DIAGNOSTIC_REPORT_INVALID",
            "texture/fill diagnostic report root must be an object");
        return dto;
    }

    dto.schema = ReadString(report, "schema").value_or("");
    dto.availability = ReadString(report, "availability").value_or("");
    dto.status = ReadString(report, "status").value_or("");
    dto.backend = ReadString(report, "backend").value_or("");
    dto.backendrole = ReadString(report, "backendRole").value_or("");
    dto.productionacceptance =
        ReadString(report, "productionAcceptance").value_or("");

    if (dto.schema != kTextureFillPartitionSchema
        || dto.availability.empty()
        || dto.status.empty()
        || dto.backend.empty()
        || dto.backendrole.empty()
        || dto.productionacceptance.empty())
    {
        dto.schemavalid = false;
        dto.state = TextureFillPartitionDiagnosticState::Blocked;
        AppendFacadeIssue(
            dto,
            "E_12E_DIAGNOSTIC_REPORT_INVALID",
            "texture/fill diagnostic report schema or required root fields are invalid");
    }
    else if (dto.availability == "unavailable" || dto.status == "unavailable")
    {
        dto.state = TextureFillPartitionDiagnosticState::Unavailable;
    }
    else if (dto.status == "blocked" || dto.status == "fail")
    {
        dto.state = TextureFillPartitionDiagnosticState::Blocked;
    }
    else if (dto.availability == "available"
             && (dto.status == "diagnostic" || dto.status == "pass"))
    {
        dto.state = TextureFillPartitionDiagnosticState::Diagnostic;
    }
    else
    {
        dto.schemavalid = false;
        dto.state = TextureFillPartitionDiagnosticState::Blocked;
        AppendFacadeIssue(
            dto,
            "E_12E_DIAGNOSTIC_REPORT_INVALID",
            "texture/fill diagnostic report contains an unsupported state combination");
    }

    ReadIssues(report, dto);
    ReadWidth(report, dto);
    ReadPartition(report, dto);
    ReadRasterMapping(report, dto);
    ReadFullClosure(report, dto);
    ReadPerformance(report, dto);
    ReadProductionOutputFlag(report, "closureLinkage", dto);
    ReadProductionOutputFlag(report, "diagnosticComposer", dto);
    ReadProductionOutputFlag(report, "rasterMapping", dto);

    if (!dto.schemavalid)
    {
        dto.state = TextureFillPartitionDiagnosticState::Blocked;
    }
    if (dto.productionoutputwritten)
    {
        dto.state = TextureFillPartitionDiagnosticState::Blocked;
        AppendFacadeIssue(
            dto,
            "E_12E_DIAGNOSTIC_REPORT_PRODUCTION_OUTPUT",
            "diagnostic UI report unexpectedly declares production output");
    }
    return dto;
}

const char* TextureFillPartitionDiagnosticFacade::StateName(
    const TextureFillPartitionDiagnosticState state)
{
    switch (state)
    {
    case TextureFillPartitionDiagnosticState::Pending:
        return "pending";
    case TextureFillPartitionDiagnosticState::Unavailable:
        return "unavailable";
    case TextureFillPartitionDiagnosticState::Blocked:
        return "blocked";
    case TextureFillPartitionDiagnosticState::Diagnostic:
        return "diagnostic";
    }
    return "blocked";
}

}  // namespace slicer_core
