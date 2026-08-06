#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/CommonDtos.h"
#include "slicer_core/json_value.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace slicesoft::module
{

/** @brief Terminal bytes and status produced by one accepted light capability. */
struct CapabilityOutput
{
    bool succeeded{false};
    bool binary{false};
    std::string bytes;
};

/** @brief Error raised when a capability request violates the frozen DTO. */
class CapabilityRequestError final : public std::runtime_error
{
public:
    /** @brief Creates one deterministic request error. @param message Diagnostic text. */
    explicit CapabilityRequestError(const std::string& message);
};

/** @brief Parses one UTF-8 JSON capability request. @param text Request bytes. @return JSON object. */
[[nodiscard]] slicer_core::Json ParseCapabilityRequest(std::string_view text);

/** @brief Reads a required object field. @param object Source object. @param name Field name. @return Field value. */
[[nodiscard]] const slicer_core::Json& RequireField(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief Reads a required non-empty string. @param object Source object. @param name Field name. @return String value. */
[[nodiscard]] std::string RequireString(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief Reads an optional string. @param object Source object. @param name Field name. @param fallback Missing value. @return String value. */
[[nodiscard]] std::string OptionalString(
    const slicer_core::Json& object,
    const std::string& name,
    const std::string& fallback = {});

/** @brief Reads a required integer. @param object Source object. @param name Field name. @return Integer value. */
[[nodiscard]] int RequireInteger(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief Reads a required unsigned integer. @param object Source object. @param name Field name. @return Unsigned value. */
[[nodiscard]] std::uint64_t RequireUnsigned(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief Reads a required boolean. @param object Source object. @param name Field name. @return Boolean value. */
[[nodiscard]] bool RequireBoolean(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief Reads a required number. @param object Source object. @param name Field name. @return Number value. */
[[nodiscard]] double RequireNumber(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief Reads a required array. @param object Source object. @param name Field name. @return Array value. */
[[nodiscard]] const slicer_core::Json::Array& RequireArray(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief Reads a required object. @param object Source object. @param name Field name. @return Object value. */
[[nodiscard]] const slicer_core::Json& RequireObject(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief Reads exactly three numbers. @param value JSON array. @return Three values. */
[[nodiscard]] std::array<double, 3> ReadNumber3(const slicer_core::Json& value);

/** @brief Serializes strings as a JSON array. @param values Strings. @return JSON array. */
[[nodiscard]] slicer_core::Json MakeStringArray(
    const std::vector<std::string>& values);

/** @brief Serializes a fixed numeric array. @param values Numbers. @return JSON array. */
template <typename T, std::size_t Size>
[[nodiscard]] slicer_core::Json MakeNumberArray(
    const std::array<T, Size>& values)
{
    slicer_core::Json::Array result;
    result.reserve(Size);
    for (const T value : values)
    {
        result.emplace_back(static_cast<double>(value));
    }
    return slicer_core::Json{std::move(result)};
}

/** @brief Serializes a numeric vector. @param values Numbers. @return JSON array. */
template <typename T>
[[nodiscard]] slicer_core::Json MakeNumberArray(const std::vector<T>& values)
{
    slicer_core::Json::Array result;
    result.reserve(values.size());
    for (const T value : values)
    {
        result.emplace_back(static_cast<double>(value));
    }
    return slicer_core::Json{std::move(result)};
}

/** @brief Serializes local bounds. @param bounds Bounds value. @return JSON object. */
[[nodiscard]] slicer_core::Json MakeBounds(
    const slicer_core::api::Bounds3d& bounds);

/** @brief Serializes a row-major matrix. @param matrix Matrix value. @return JSON array. */
[[nodiscard]] slicer_core::Json MakeMatrix(
    const slicer_core::api::Matrix4d& matrix);

/** @brief Builds a successful result envelope. @param fields Capability fields. @return Result object. */
[[nodiscard]] slicer_core::Json MakeSuccess(
    slicer_core::Json::Object fields = {});

/** @brief Builds a failed result envelope. @param code Stable code. @param message Message. @param detail Detail. @return Result object. */
[[nodiscard]] slicer_core::Json MakeFailure(
    std::string_view code,
    std::string_view message,
    std::string_view detail = {});

/** @brief Builds a failed result envelope. @param error API error. @return Result object. */
[[nodiscard]] slicer_core::Json MakeFailure(
    const slicer_core::api::ApiError& error);

/** @brief Converts a result envelope into terminal output. @param result Result envelope. @return Serialized output. */
[[nodiscard]] CapabilityOutput MakeCapabilityOutput(
    const slicer_core::Json& result);

/** @brief Parses an internal structured object. @param value UTF-8 object. @return Parsed object. */
[[nodiscard]] slicer_core::Json ParseStructuredObject(
    const slicer_core::api::StructuredJsonObject& value);

/** @brief Parses a decimal model identity. @param value Identity text. @return Numeric identity. */
[[nodiscard]] std::uint64_t ParseModelId(const std::string& value);

}  // namespace slicesoft::module
