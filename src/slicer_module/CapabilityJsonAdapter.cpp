#include "slicer_module/CapabilityJsonAdapter.h"

#include <sstream>

namespace slicesoft::module
{

CapabilityRequestError::CapabilityRequestError(const std::string& message)
    : std::runtime_error(message)
{
}

slicer_core::Json ParseCapabilityRequest(const std::string_view text)
{
    try
    {
        std::istringstream input{std::string{text}};
        slicer_core::Json document = slicer_core::Json::parse(input);
        if (!document.is_object())
        {
            throw CapabilityRequestError("capability request must be a JSON object");
        }
        return document;
    }
    catch (const CapabilityRequestError&)
    {
        throw;
    }
    catch (const std::exception& error)
    {
        throw CapabilityRequestError(
            std::string{"capability request is not valid JSON: "} + error.what());
    }
}

const slicer_core::Json& RequireField(
    const slicer_core::Json& object,
    const std::string& name)
{
    if (!object.is_object() || !object.contains(name))
    {
        throw CapabilityRequestError("missing required field: " + name);
    }
    return object.at(name);
}

std::string RequireString(
    const slicer_core::Json& object,
    const std::string& name)
{
    const slicer_core::Json& field = RequireField(object, name);
    if (!field.is_string())
    {
        throw CapabilityRequestError("field must be a string: " + name);
    }
    const std::string value = field.as_string();
    if (value.empty())
    {
        throw CapabilityRequestError("field must not be empty: " + name);
    }
    return value;
}

std::string OptionalString(
    const slicer_core::Json& object,
    const std::string& name,
    const std::string& fallback)
{
    if (!object.contains(name))
    {
        return fallback;
    }
    if (!object.at(name).is_string())
    {
        throw CapabilityRequestError("field must be a string: " + name);
    }
    return object.at(name).as_string();
}

int RequireInteger(
    const slicer_core::Json& object,
    const std::string& name)
{
    const slicer_core::Json& field = RequireField(object, name);
    if (!field.is_number())
    {
        throw CapabilityRequestError("field must be an integer: " + name);
    }
    return field.as_int();
}

std::uint64_t RequireUnsigned(
    const slicer_core::Json& object,
    const std::string& name)
{
    const int value = RequireInteger(object, name);
    if (value < 0)
    {
        throw CapabilityRequestError("field must not be negative: " + name);
    }
    return static_cast<std::uint64_t>(value);
}

bool RequireBoolean(
    const slicer_core::Json& object,
    const std::string& name)
{
    const slicer_core::Json& field = RequireField(object, name);
    if (!field.is_bool())
    {
        throw CapabilityRequestError("field must be a boolean: " + name);
    }
    return field.as_bool();
}

double RequireNumber(
    const slicer_core::Json& object,
    const std::string& name)
{
    const slicer_core::Json& field = RequireField(object, name);
    if (!field.is_number())
    {
        throw CapabilityRequestError("field must be a number: " + name);
    }
    return field.as_double();
}

const slicer_core::Json::Array& RequireArray(
    const slicer_core::Json& object,
    const std::string& name)
{
    const slicer_core::Json& field = RequireField(object, name);
    if (!field.is_array())
    {
        throw CapabilityRequestError("field must be an array: " + name);
    }
    return field.as_array();
}

const slicer_core::Json& RequireObject(
    const slicer_core::Json& object,
    const std::string& name)
{
    const slicer_core::Json& field = RequireField(object, name);
    if (!field.is_object())
    {
        throw CapabilityRequestError("field must be an object: " + name);
    }
    return field;
}

std::array<double, 3> ReadNumber3(const slicer_core::Json& value)
{
    if (!value.is_array() || value.size() != 3U)
    {
        throw CapabilityRequestError("field must contain exactly three numbers");
    }
    std::array<double, 3> result{};
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        if (!value.at(index).is_number())
        {
            throw CapabilityRequestError("array item must be a number");
        }
        result[index] = value.at(index).as_double();
    }
    return result;
}

slicer_core::Json MakeStringArray(const std::vector<std::string>& values)
{
    slicer_core::Json::Array result;
    result.reserve(values.size());
    for (const std::string& value : values)
    {
        result.emplace_back(value);
    }
    return slicer_core::Json{std::move(result)};
}

slicer_core::Json MakeBounds(const slicer_core::api::Bounds3d& bounds)
{
    return slicer_core::Json::object({
        {"min", MakeNumberArray(bounds.min_mm)},
        {"max", MakeNumberArray(bounds.max_mm)}});
}

slicer_core::Json MakeMatrix(const slicer_core::api::Matrix4d& matrix)
{
    return MakeNumberArray(matrix.values);
}

slicer_core::Json MakeSuccess(slicer_core::Json::Object fields)
{
    fields.emplace("ok", true);
    fields.emplace("code", "PM-SLICER-OK-0000");
    return slicer_core::Json{std::move(fields)};
}

slicer_core::Json MakeFailure(
    const std::string_view code,
    const std::string_view message,
    const std::string_view detail)
{
    slicer_core::Json::Object fields{
        {"ok", false},
        {"code", std::string{code}},
        {"message", std::string{message}}};
    if (!detail.empty())
    {
        fields.emplace("detail", std::string{detail});
    }
    return slicer_core::Json{std::move(fields)};
}

slicer_core::Json MakeFailure(const slicer_core::api::ApiError& error)
{
    return MakeFailure(error.code, error.message, error.detail);
}

CapabilityOutput MakeCapabilityOutput(const slicer_core::Json& result)
{
    CapabilityOutput output;
    output.succeeded = result.is_object()
        && result.contains("ok")
        && result.at("ok").is_bool()
        && result.at("ok").as_bool();
    output.bytes = result.dump(0);
    return output;
}

slicer_core::Json ParseStructuredObject(
    const slicer_core::api::StructuredJsonObject& value)
{
    try
    {
        std::istringstream input{value.utf8_json};
        slicer_core::Json result = slicer_core::Json::parse(input);
        return result.is_object()
            ? result
            : slicer_core::Json::object({});
    }
    catch (...)
    {
        return slicer_core::Json::object({});
    }
}

std::uint64_t ParseModelId(const std::string& value)
{
    try
    {
        std::size_t consumed{0U};
        const std::uint64_t result = std::stoull(value, &consumed);
        if (consumed != value.size() || result == 0U)
        {
            throw CapabilityRequestError("modelId is invalid");
        }
        return result;
    }
    catch (const CapabilityRequestError&)
    {
        throw;
    }
    catch (...)
    {
        throw CapabilityRequestError("modelId is invalid");
    }
}

}  // namespace slicesoft::module
