#include "WorkerContract.h"

#include "slicer_core/json_value.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace slicesoft::module
{
namespace
{

constexpr std::string_view ContractName{"file_contract"};
constexpr std::string_view ContractError{"PM-SLICER-CONTRACT-0060"};
constexpr std::string_view InternalError{"PM-SLICER-INTERNAL-0099"};
constexpr std::string_view LegacySliceCapability{"slice.rgbwsv"};
constexpr std::string_view TransferSliceCapability{"slice.rgbwsvt"};
constexpr std::string_view LegacyPackageContract{"p0.rgbwsv.2"};
constexpr std::string_view TransferPackageContract{"p0.rgbwsvt.1"};
constexpr auto ContractInfoTimeout = std::chrono::milliseconds{5000};

const std::unordered_set<std::string> KnownCapabilities{
    std::string{LegacySliceCapability},
    std::string{TransferSliceCapability},
    "geometry.preflight.full",
    "geometry.repair"};

void AddUnique(std::vector<std::string>* values, const std::string_view value)
{
    if (std::find(values->begin(), values->end(), value) == values->end())
    {
        values->emplace_back(value);
    }
}

WorkerContractRequirement ApplyCapabilityRequirements(
    WorkerContractRequirement requirement)
{
    for (const std::string& capability : requirement.requiredCapabilities)
    {
        if (capability == LegacySliceCapability)
        {
            AddUnique(&requirement.requiredProduces, LegacyPackageContract);
        }
        else if (capability == TransferSliceCapability)
        {
            requirement.minor = (std::max)(requirement.minor, 1U);
            AddUnique(&requirement.requiredProduces, TransferPackageContract);
        }
    }
    return requirement;
}

bool ReadVersion(
    const slicer_core::Json& document,
    const std::string& key,
    std::uint32_t* value)
{
    if (!document.contains(key) || !document.at(key).is_number())
    {
        return false;
    }
    const double number = document.at(key).as_double();
    if (!std::isfinite(number) || number < 0.0 || std::floor(number) != number
        || number > static_cast<double>((std::numeric_limits<std::uint32_t>::max)()))
    {
        return false;
    }
    *value = static_cast<std::uint32_t>(number);
    return true;
}

bool ReadNonEmptyString(
    const slicer_core::Json& document,
    const std::string& key,
    std::string* value)
{
    if (!document.contains(key) || !document.at(key).is_string())
    {
        return false;
    }
    *value = document.at(key).as_string();
    return !value->empty();
}

bool ReadStringArray(
    const slicer_core::Json& document,
    const std::string& key,
    const bool requireNonEmpty,
    const bool requireUnique,
    std::vector<std::string>* values)
{
    if (!document.contains(key) || !document.at(key).is_array())
    {
        return false;
    }
    std::unordered_set<std::string> unique;
    for (const slicer_core::Json& item : document.at(key).as_array())
    {
        if (!item.is_string())
        {
            return false;
        }
        std::string value = item.as_string();
        if (requireUnique && !unique.insert(value).second)
        {
            return false;
        }
        values->push_back(std::move(value));
    }
    return !requireNonEmpty || !values->empty();
}

bool ParseContractInfo(
    const std::string& payload,
    WorkerContractInfo* info,
    std::string* errorMessage)
{
    try
    {
        std::istringstream input{payload};
        const slicer_core::Json document = slicer_core::Json::parse(input);
        std::string contract;
        if (!document.is_object()
            || !ReadNonEmptyString(document, "contract", &contract)
            || contract != ContractName
            || !ReadVersion(document, "major", &info->major)
            || !ReadVersion(document, "minor", &info->minor)
            || !ReadNonEmptyString(document, "engineVersion", &info->engineVersion)
            || !ReadStringArray(document, "produces", true, false, &info->produces)
            || !ReadStringArray(document, "capabilities", true, true, &info->capabilities))
        {
            *errorMessage = "--contract-info does not satisfy the required file_contract_v1 fields";
            return false;
        }
        if (!std::all_of(info->capabilities.begin(), info->capabilities.end(),
                [](const std::string& capability)
                {
                    return KnownCapabilities.contains(capability);
                }))
        {
            *errorMessage = "--contract-info declares an unknown worker capability";
            return false;
        }
        return true;
    }
    catch (const std::exception& error)
    {
        *errorMessage = std::string{"invalid --contract-info JSON: "} + error.what();
        return false;
    }
}

bool ContainsAll(
    const std::vector<std::string>& available,
    const std::vector<std::string>& required,
    std::string* missing)
{
    for (const std::string& value : required)
    {
        if (std::find(available.begin(), available.end(), value) == available.end())
        {
            *missing = value;
            return false;
        }
    }
    return true;
}

WorkerContractResult Reject(
    WorkerContractResult result,
    const WorkerContractDecision decision,
    const std::string_view errorCode,
    std::string message)
{
    result.compatible = false;
    result.decision = decision;
    result.errorCode.assign(errorCode);
    result.errorMessage = std::move(message);
    return result;
}

}  // namespace

WorkerContractNegotiator::WorkerContractNegotiator(WorkerClient& client) noexcept
    : m_client(client)
{
}

WorkerContractResult WorkerContractNegotiator::Negotiate(
    const std::filesystem::path& workerExecutable,
    const WorkerContractRequirement& requestedRequirement) const
{
    const WorkerContractRequirement requirement =
        ApplyCapabilityRequirements(requestedRequirement);
    WorkerContractResult result;
    WorkerLaunchOptions options;
    options.executablePath = workerExecutable;
    options.arguments = {"--contract-info"};
    options.workingDirectory = workerExecutable.parent_path();
    options.timeout = ContractInfoTimeout;
    options.requireTerminalProgress = false;
    result.transport = m_client.Run(options);
    if (result.transport.exitCategory != WorkerExitCategory::Ok)
    {
        const std::string errorCode = result.transport.errorCode.empty()
            ? std::string{InternalError} : result.transport.errorCode;
        const std::string errorMessage = result.transport.errorMessage.empty()
            ? "worker contract discovery process failed" : result.transport.errorMessage;
        return Reject(std::move(result), WorkerContractDecision::TransportFailure,
            errorCode, errorMessage);
    }
    if (result.transport.stdoutLogLines.size() != 1)
    {
        return Reject(std::move(result), WorkerContractDecision::InvalidDocument, ContractError,
            "--contract-info stdout must contain exactly one JSON object and no ordinary log lines");
    }
    if (!ParseContractInfo(
            result.transport.stdoutLogLines.front(), &result.info, &result.errorMessage))
    {
        return Reject(std::move(result), WorkerContractDecision::InvalidDocument, ContractError,
            result.errorMessage);
    }
    if (result.info.major != requirement.major)
    {
        return Reject(std::move(result), WorkerContractDecision::MajorMismatch, InternalError,
            "worker file-contract major does not match the module requirement");
    }
    if (result.info.minor < requirement.minor)
    {
        return Reject(std::move(result), WorkerContractDecision::MinorTooOld, InternalError,
            "worker file-contract minor is older than the module requirement");
    }
    std::string missing;
    if (!ContainsAll(result.info.produces, requirement.requiredProduces, &missing))
    {
        return Reject(std::move(result), WorkerContractDecision::MissingProductionContract,
            ContractError, "worker does not produce required contract: " + missing);
    }
    if (!ContainsAll(result.info.capabilities, requirement.requiredCapabilities, &missing))
    {
        return Reject(std::move(result), WorkerContractDecision::MissingCapability,
            ContractError, "worker does not declare required capability: " + missing);
    }
    result.compatible = true;
    result.decision = WorkerContractDecision::Compatible;
    result.errorCode = "PM-SLICER-OK-0000";
    result.errorMessage.clear();
    return result;
}

}  // namespace slicesoft::module
