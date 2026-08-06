#include "slicer_worker/runtime/WorkerRequestParser.h"

#include "slicer_core/json_value.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace slicesoft::worker
{
namespace
{

constexpr std::string_view ContractName{"file_contract"};
constexpr std::string_view SliceCapability{"slice.rgbwsv"};
constexpr std::string_view PreflightCapability{"geometry.preflight.full"};
constexpr std::string_view RepairCapability{"geometry.repair"};
constexpr std::string_view ProductionContract{"p0.rgbwsv.2"};
constexpr std::uint32_t SupportedMajor{1U};
constexpr std::uint32_t SupportedMinor{0U};
constexpr std::int64_t MinimumTimeoutMs{1000};
constexpr std::int64_t MaximumTimeoutMs{86400000};

[[noreturn]] void Reject(
    const WorkerRequestParseErrorCode code,
    const std::string& message)
{
    throw WorkerRequestParseError(code, message);
}

std::filesystem::path ValidatePath(const std::filesystem::path& requestPath)
{
    if (!requestPath.is_absolute())
    {
        Reject(WorkerRequestParseErrorCode::InvalidPath,
            "request path must be absolute");
    }
    std::error_code error;
    const bool regularFile = std::filesystem::is_regular_file(requestPath, error);
    if (error || !regularFile)
    {
        Reject(WorkerRequestParseErrorCode::InvalidPath,
            "request path must identify an existing regular file");
    }
    if (requestPath.filename() != "request.json")
    {
        Reject(WorkerRequestParseErrorCode::InvalidPath,
            "request file name must be request.json");
    }
    return requestPath.lexically_normal();
}

std::optional<std::size_t> Utf8CodePointCount(const std::string_view bytes)
{
    std::size_t offset{0U};
    std::size_t count{0U};
    while (offset < bytes.size())
    {
        const auto lead = static_cast<unsigned char>(bytes.at(offset));
        std::size_t length{0U};
        std::uint32_t value{0U};
        if (lead <= 0x7FU)
        {
            length = 1U;
            value = lead;
        }
        else if (lead >= 0xC2U && lead <= 0xDFU)
        {
            length = 2U;
            value = lead & 0x1FU;
        }
        else if (lead >= 0xE0U && lead <= 0xEFU)
        {
            length = 3U;
            value = lead & 0x0FU;
        }
        else if (lead >= 0xF0U && lead <= 0xF4U)
        {
            length = 4U;
            value = lead & 0x07U;
        }
        else
        {
            return std::nullopt;
        }
        if (offset + length > bytes.size())
        {
            return std::nullopt;
        }
        for (std::size_t index{1U}; index < length; ++index)
        {
            const auto continuation =
                static_cast<unsigned char>(bytes.at(offset + index));
            if ((continuation & 0xC0U) != 0x80U)
            {
                return std::nullopt;
            }
            value = (value << 6U) | (continuation & 0x3FU);
        }
        const bool overlong = (length == 2U && value < 0x80U)
            || (length == 3U && value < 0x800U)
            || (length == 4U && value < 0x10000U);
        if (overlong || value > 0x10FFFFU
            || (value >= 0xD800U && value <= 0xDFFFU))
        {
            return std::nullopt;
        }
        offset += length;
        ++count;
    }
    return count;
}

std::string ReadRequestBytes(const std::filesystem::path& requestPath)
{
    std::ifstream input{requestPath, std::ios::binary};
    if (!input)
    {
        Reject(WorkerRequestParseErrorCode::ReadFailure,
            "request file could not be opened");
    }
    const std::string bytes{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
    if (input.bad())
    {
        Reject(WorkerRequestParseErrorCode::ReadFailure,
            "request file could not be read completely");
    }
    if (bytes.empty())
    {
        Reject(WorkerRequestParseErrorCode::InvalidEncoding,
            "request file must not be empty");
    }
    if (bytes.size() >= 3U
        && static_cast<unsigned char>(bytes.at(0)) == 0xEFU
        && static_cast<unsigned char>(bytes.at(1)) == 0xBBU
        && static_cast<unsigned char>(bytes.at(2)) == 0xBFU)
    {
        Reject(WorkerRequestParseErrorCode::InvalidEncoding,
            "request file must be UTF-8 without BOM");
    }
    if (!Utf8CodePointCount(bytes).has_value())
    {
        Reject(WorkerRequestParseErrorCode::InvalidEncoding,
            "request file must contain valid UTF-8");
    }
    return bytes;
}

slicer_core::Json ParseDocument(const std::string& bytes)
{
    try
    {
        std::istringstream input{bytes};
        slicer_core::Json document = slicer_core::Json::parse(input);
        if (!document.is_object())
        {
            Reject(WorkerRequestParseErrorCode::ContractViolation,
                "request root must be an object");
        }
        return document;
    }
    catch (const WorkerRequestParseError&)
    {
        throw;
    }
    catch (const std::exception& error)
    {
        Reject(WorkerRequestParseErrorCode::InvalidJson,
            std::string{"request JSON is invalid: "} + error.what());
    }
}

std::string RequireString(
    const slicer_core::Json& document,
    const std::string& key)
{
    if (!document.contains(key) || !document.at(key).is_string())
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "required string is missing or invalid: " + key);
    }
    return document.at(key).as_string();
}

std::uint32_t RequireVersion(
    const slicer_core::Json& document,
    const std::string& key)
{
    if (!document.contains(key) || !document.at(key).is_number())
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "required version is missing or invalid: " + key);
    }
    const double number = document.at(key).as_double();
    if (!std::isfinite(number) || number < 0.0 || std::floor(number) != number
        || number > static_cast<double>((std::numeric_limits<std::uint32_t>::max)()))
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "version must be a finite unsigned integer: " + key);
    }
    return static_cast<std::uint32_t>(number);
}

std::chrono::milliseconds RequireTimeout(const slicer_core::Json& document)
{
    if (!document.contains("timeoutMs") || !document.at("timeoutMs").is_number())
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "required timeoutMs is missing or invalid");
    }
    const double number = document.at("timeoutMs").as_double();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < static_cast<double>(MinimumTimeoutMs)
        || number > static_cast<double>(MaximumTimeoutMs))
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "timeoutMs must be an integer from 1000 through 86400000");
    }
    return std::chrono::milliseconds{static_cast<std::int64_t>(number)};
}

bool IsAsciiAlphaNumeric(const char value)
{
    return (value >= 'A' && value <= 'Z')
        || (value >= 'a' && value <= 'z')
        || (value >= '0' && value <= '9');
}

bool IsValidJobId(const std::string& jobId)
{
    if (jobId.empty() || jobId.size() > 128U || !IsAsciiAlphaNumeric(jobId.front()))
    {
        return false;
    }
    for (const char value : jobId)
    {
        if (!IsAsciiAlphaNumeric(value)
            && value != '_' && value != '.' && value != '-')
        {
            return false;
        }
    }
    return true;
}

bool IsHexDigit(const char value)
{
    return (value >= '0' && value <= '9')
        || (value >= 'A' && value <= 'F')
        || (value >= 'a' && value <= 'f');
}

bool IsValidSceneHash(const std::string& sceneHash)
{
    constexpr std::string_view Prefix{"sha256:"};
    if (!sceneHash.starts_with(Prefix))
    {
        return false;
    }
    const std::string_view digest{sceneHash.data() + Prefix.size(),
        sceneHash.size() - Prefix.size()};
    if (digest.size() < 8U || digest.size() > 64U)
    {
        return false;
    }
    for (const char value : digest)
    {
        if (!IsHexDigit(value))
        {
            return false;
        }
    }
    return true;
}

slicer_core::Json OptionalObject(
    const slicer_core::Json& document,
    const std::string& key)
{
    if (!document.contains(key))
    {
        return nullptr;
    }
    if (!document.at(key).is_object())
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "optional request branch must be an object: " + key);
    }
    return document.at(key);
}

void RequireObject(const slicer_core::Json& document, const std::string& key)
{
    if (!document.contains(key) || !document.at(key).is_object())
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "required request branch is missing or invalid: " + key);
    }
}

void ValidateSliceBranch(const slicer_core::Json& document)
{
    const std::string sceneHash = RequireString(document, "sceneHash");
    if (!IsValidSceneHash(sceneHash))
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "sceneHash does not satisfy the file-contract pattern");
    }
    RequireObject(document, "scene");
    RequireObject(document, "profile");
    RequireObject(document, "output");
    const slicer_core::Json& output = document.at("output");
    if (RequireString(output, "contract") != ProductionContract)
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "slice output contract must be p0.rgbwsv.2");
    }
    if (RequireString(output, "packageDir").empty())
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "slice output packageDir must not be empty");
    }
}

bool IsKnownCapability(const std::string& capability)
{
    return capability == SliceCapability
        || capability == PreflightCapability
        || capability == RepairCapability;
}

}  // namespace

WorkerRequestParseError::WorkerRequestParseError(
    const WorkerRequestParseErrorCode code,
    const std::string& message)
    : std::runtime_error(message), m_code(code)
{
}

WorkerRequestParseErrorCode WorkerRequestParseError::Code() const noexcept
{
    return m_code;
}

WorkerRequestEnvelope WorkerRequestParser::Parse(
    const std::filesystem::path& requestPath)
{
    const std::filesystem::path canonicalPath = ValidatePath(requestPath);
    const slicer_core::Json document = ParseDocument(ReadRequestBytes(canonicalPath));

    if (RequireString(document, "contract") != ContractName)
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "request contract must be file_contract");
    }
    const std::uint32_t major = RequireVersion(document, "major");
    const std::uint32_t minor = RequireVersion(document, "minor");
    if (major != SupportedMajor || minor != SupportedMinor)
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "request file-contract version is not supported");
    }

    const std::string jobId = RequireString(document, "jobId");
    const std::string correlationId = RequireString(document, "correlationId");
    const std::string capability = RequireString(document, "capability");
    if (!IsValidJobId(jobId))
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "jobId does not satisfy the file-contract pattern");
    }
    const std::optional<std::size_t> correlationLength =
        Utf8CodePointCount(correlationId);
    if (!correlationLength.has_value()
        || *correlationLength < 1U || *correlationLength > 128U)
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "correlationId length must be from 1 through 128 Unicode code points");
    }
    if (!IsKnownCapability(capability))
    {
        Reject(WorkerRequestParseErrorCode::ContractViolation,
            "request capability is not supported by file_contract_v1");
    }

    if (capability == SliceCapability)
    {
        ValidateSliceBranch(document);
    }
    else
    {
        RequireObject(document, "input");
    }

    std::optional<std::string> sceneHash;
    if (document.contains("sceneHash"))
    {
        sceneHash = RequireString(document, "sceneHash");
        if (!IsValidSceneHash(*sceneHash))
        {
            Reject(WorkerRequestParseErrorCode::ContractViolation,
                "sceneHash does not satisfy the file-contract pattern");
        }
    }

    return WorkerRequestEnvelope(
        WorkerJobIdentity(jobId, correlationId, capability, canonicalPath),
        major,
        minor,
        RequireTimeout(document),
        std::move(sceneHash),
        OptionalObject(document, "scene"),
        OptionalObject(document, "profile"),
        OptionalObject(document, "input"),
        OptionalObject(document, "output"));
}

}  // namespace slicesoft::worker
