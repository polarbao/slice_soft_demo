#pragma once

#include "slicer_worker/runtime/WorkerRequestEnvelope.h"

#include <filesystem>
#include <stdexcept>

namespace slicesoft::worker
{

/** @brief Stable parser failure categories before a trusted job identity exists. */
enum class WorkerRequestParseErrorCode
{
    InvalidPath,
    ReadFailure,
    InvalidEncoding,
    InvalidJson,
    ContractViolation
};

/** @brief Describes a fail-closed request parsing or validation failure. */
class WorkerRequestParseError final : public std::runtime_error
{
public:
    /**
     * @brief Creates a parser error with a stable category.
     * @param code Stable parser failure category.
     * @param message Human-readable diagnostic without request secrets.
     */
    WorkerRequestParseError(
        WorkerRequestParseErrorCode code,
        const std::string& message);

    /** @brief Returns the stable parser failure category. */
    [[nodiscard]] WorkerRequestParseErrorCode Code() const noexcept;

private:
    WorkerRequestParseErrorCode m_code;
};

/** @brief Strict file_contract_v1 request parser and semantic validator. */
class WorkerRequestParser final
{
public:
    /**
     * @brief Parses one absolute request.json into an immutable envelope.
     * @param requestPath Existing absolute regular-file path.
     * @return Validated request envelope with canonical owned paths.
     * @throws WorkerRequestParseError When path, encoding, JSON, or contract validation fails.
     */
    [[nodiscard]] static WorkerRequestEnvelope Parse(
        const std::filesystem::path& requestPath);
};

}  // namespace slicesoft::worker
