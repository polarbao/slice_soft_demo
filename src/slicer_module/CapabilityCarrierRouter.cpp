#include "slicer_module/CapabilityCarrierRouter.h"

#include "slicer_module/CapabilityJsonAdapter.h"

#include <exception>
#include <utility>

namespace slicesoft::module
{
namespace
{

constexpr std::string_view PreflightCapability{"geometry.preflight"};
constexpr std::string_view WorkerPreflightCapability{"geometry.preflight.full"};
constexpr std::string_view RepairCapability{"geometry.repair"};
constexpr std::string_view SliceCapability{"slice.rgbwsv"};
constexpr std::string_view TransferSliceCapability{"slice.rgbwsvt"};

CapabilityRoute Reject(
    std::string capability,
    std::string code,
    std::string message,
    std::string detail)
{
    CapabilityRoute route;
    route.publicCapability = std::move(capability);
    route.errorCode = std::move(code);
    route.errorMessage = std::move(message);
    route.errorDetail = std::move(detail);
    return route;
}

void CopyRequired(
    slicer_core::Json::Object& target,
    const slicer_core::Json& request,
    const std::string& name)
{
    target.emplace(name, RequireField(request, name));
}

void CopyOptional(
    slicer_core::Json::Object& target,
    const slicer_core::Json& request,
    const std::string& name)
{
    if (request.contains(name))
    {
        target.emplace(name, request.at(name));
    }
}

void ReadWorkerIdentity(
    const slicer_core::Json& request,
    CapabilityRoute& route,
    const bool required)
{
    if (required || request.contains("jobId"))
    {
        route.jobId = RequireString(request, "jobId");
    }
    if (required || request.contains("correlationId"))
    {
        route.correlationId = RequireString(request, "correlationId");
    }
}

slicer_core::Json NormalizeWorkerOnlyBackend(const slicer_core::Json& request)
{
    slicer_core::Json::Object normalized = request.as_object();
    if (!request.contains("options"))
    {
        normalized.emplace(
            "options",
            slicer_core::Json::object({{"backend", "worker"}}));
        return slicer_core::Json{std::move(normalized)};
    }

    const slicer_core::Json& options = request.at("options");
    if (!options.is_object())
    {
        throw CapabilityRequestError("field must be an object: options");
    }
    slicer_core::Json::Object normalizedOptions = options.as_object();
    if (!options.contains("backend"))
    {
        normalizedOptions.emplace("backend", "worker");
        normalized.insert_or_assign(
            "options",
            slicer_core::Json{std::move(normalizedOptions)});
        return slicer_core::Json{std::move(normalized)};
    }
    if (!options.at("backend").is_string())
    {
        throw CapabilityRequestError(
            "field must be the string value worker: options.backend");
    }
    if (options.at("backend").as_string() != "worker")
    {
        throw std::domain_error(
            "options.backend only accepts the exact value worker");
    }
    return slicer_core::Json{std::move(normalized)};
}

CapabilityRoute RoutePreflight(slicer_core::Json request)
{
    const std::string mode = RequireString(request, "mode");
    if (mode == "fast")
    {
        CapabilityRoute route;
        route.accepted = true;
        route.publicCapability = std::string{PreflightCapability};
        return route;
    }
    if (mode != "full")
    {
        return Reject(
            std::string{PreflightCapability},
            "PM-SLICER-INPUT-0002",
            "geometry.preflight mode is invalid",
            mode);
    }

    CapabilityRoute route;
    route.accepted = true;
    route.carrier = CapabilityCarrier::Worker;
    route.publicCapability = std::string{PreflightCapability};
    route.workerCapability = std::string{WorkerPreflightCapability};
    ReadWorkerIdentity(request, route, false);

    slicer_core::Json::Object input;
    CopyRequired(input, request, "mode");
    CopyRequired(input, request, "scene");
    CopyRequired(input, request, "sceneHash");
    CopyRequired(input, request, "expectedSceneRevision");
    CopyRequired(input, request, "profile");
    CopyRequired(input, request, "profileHash");
    CopyRequired(input, request, "targetMode");
    CopyOptional(input, request, "buildVolume");
    route.workerPayload = slicer_core::Json::object({
        {"input", slicer_core::Json{std::move(input)}}});
    return route;
}

CapabilityRoute RouteRepair(const slicer_core::Json& request)
{
    CapabilityRoute route;
    route.accepted = true;
    route.carrier = CapabilityCarrier::Worker;
    route.publicCapability = std::string{RepairCapability};
    route.workerCapability = std::string{RepairCapability};
    ReadWorkerIdentity(request, route, true);

    slicer_core::Json::Object input;
    CopyOptional(input, request, "modelId");
    CopyRequired(input, request, "modelPath");
    CopyRequired(input, request, "modelFormat");
    CopyRequired(input, request, "outputPath");
    CopyRequired(input, request, "profileHash");
    CopyRequired(input, request, "sourceResourceScope");
    CopyRequired(input, request, "repairOutputFormat");
    CopyRequired(input, request, "policy");
    CopyRequired(input, request, "requireStrictPass");
    route.workerPayload = slicer_core::Json::object({
        {"profile", RequireObject(request, "profile")},
        {"input", slicer_core::Json{std::move(input)}}});
    return route;
}

CapabilityRoute RouteSlice(
    slicer_core::Json request,
    const std::string_view capability)
{
    request = NormalizeWorkerOnlyBackend(request);

    CapabilityRoute route;
    route.accepted = true;
    route.carrier = CapabilityCarrier::Worker;
    route.publicCapability = std::string{capability};
    route.workerCapability = std::string{capability};
    route.contractMinor = capability == TransferSliceCapability ? 1U : 0U;
    ReadWorkerIdentity(request, route, true);

    slicer_core::Json::Object payload;
    CopyRequired(payload, request, "sceneHash");
    CopyRequired(payload, request, "scene");
    CopyRequired(payload, request, "profile");
    CopyRequired(payload, request, "output");
    CopyRequired(payload, request, "options");
    route.workerPayload = slicer_core::Json{std::move(payload)};
    return route;
}

}  // namespace

CapabilityRoute CapabilityCarrierRouter::Route(const std::string_view requestText)
{
    try
    {
        slicer_core::Json request = ParseCapabilityRequest(requestText);
        const std::string capability = RequireString(request, "capability");
        if (capability == PreflightCapability)
        {
            return RoutePreflight(std::move(request));
        }
        if (capability == RepairCapability)
        {
            return RouteRepair(request);
        }
        if (capability == SliceCapability
            || capability == TransferSliceCapability)
        {
            return RouteSlice(std::move(request), capability);
        }

        CapabilityRoute route;
        route.accepted = true;
        route.publicCapability = capability;
        return route;
    }
    catch (const std::domain_error& error)
    {
        return Reject(
            std::string{SliceCapability},
            "PM-SLICER-PROFILE-0031",
            "slice backend is not supported",
            error.what());
    }
    catch (const CapabilityRequestError& error)
    {
        return Reject(
            {},
            "PM-SLICER-INPUT-0002",
            "invalid capability carrier request",
            error.what());
    }
    catch (const std::exception& error)
    {
        return Reject(
            {},
            "PM-SLICER-INTERNAL-0099",
            "capability carrier routing failed",
            error.what());
    }
    catch (...)
    {
        return Reject(
            {},
            "PM-SLICER-INTERNAL-0099",
            "capability carrier routing failed",
            "unknown exception");
    }
}

}  // namespace slicesoft::module
