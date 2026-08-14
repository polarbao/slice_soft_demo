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

/** @brief 一个已接受轻量能力请求的终态字节与状态。 */
struct CapabilityOutput
{
    bool succeeded{false};
    bool binary{false};
    std::string bytes;
};

/** @brief 能力请求不符合冻结 DTO 时抛出的错误。 */
class CapabilityRequestError final : public std::runtime_error
{
public:
    /** @brief 创建确定性的请求错误。 @param message 诊断文本。 */
    explicit CapabilityRequestError(const std::string& message);
};

/** @brief 解析 UTF-8 JSON 能力请求。 @param text 请求字节。 @return JSON 对象。 */
[[nodiscard]] slicer_core::Json ParseCapabilityRequest(std::string_view text);

/** @brief 读取必需的对象字段。 @param object 源对象。 @param name 字段名。 @return 字段值。 */
[[nodiscard]] const slicer_core::Json& RequireField(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief 读取必需的非空字符串。 @param object 源对象。 @param name 字段名。 @return 字符串值。 */
[[nodiscard]] std::string RequireString(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief 读取可选字符串。 @param object 源对象。 @param name 字段名。 @param fallback 缺省值。 @return 字符串值。 */
[[nodiscard]] std::string OptionalString(
    const slicer_core::Json& object,
    const std::string& name,
    const std::string& fallback = {});

/** @brief 读取必需的整数。 @param object 源对象。 @param name 字段名。 @return 整数值。 */
[[nodiscard]] int RequireInteger(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief 读取必需的无符号整数。 @param object 源对象。 @param name 字段名。 @return 无符号值。 */
[[nodiscard]] std::uint64_t RequireUnsigned(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief 读取必需的布尔值。 @param object 源对象。 @param name 字段名。 @return 布尔值。 */
[[nodiscard]] bool RequireBoolean(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief 读取必需的数值。 @param object 源对象。 @param name 字段名。 @return 数值。 */
[[nodiscard]] double RequireNumber(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief 读取必需的数组。 @param object 源对象。 @param name 字段名。 @return 数组值。 */
[[nodiscard]] const slicer_core::Json::Array& RequireArray(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief 读取必需的对象。 @param object 源对象。 @param name 字段名。 @return 对象值。 */
[[nodiscard]] const slicer_core::Json& RequireObject(
    const slicer_core::Json& object,
    const std::string& name);

/** @brief 精确读取三个数值。 @param value JSON 数组。 @return 三个数值。 */
[[nodiscard]] std::array<double, 3> ReadNumber3(const slicer_core::Json& value);

/** @brief 将字符串序列化为 JSON 数组。 @param values 字符串集合。 @return JSON 数组。 */
[[nodiscard]] slicer_core::Json MakeStringArray(
    const std::vector<std::string>& values);

/** @brief 序列化定长数值数组。 @param values 数值集合。 @return JSON 数组。 */
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

/** @brief 序列化数值向量。 @param values 数值集合。 @return JSON 数组。 */
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

/** @brief 序列化局部边界。 @param bounds 边界值。 @return JSON 对象。 */
[[nodiscard]] slicer_core::Json MakeBounds(
    const slicer_core::api::Bounds3d& bounds);

/** @brief 序列化行主序矩阵。 @param matrix 矩阵值。 @return JSON 数组。 */
[[nodiscard]] slicer_core::Json MakeMatrix(
    const slicer_core::api::Matrix4d& matrix);

/** @brief 构造成功结果对象。 @param fields 能力字段。 @return 结果对象。 */
[[nodiscard]] slicer_core::Json MakeSuccess(
    slicer_core::Json::Object fields = {});

/** @brief 构造失败结果对象。 @param code 稳定错误码。 @param message 消息。 @param detail 详情。 @return 结果对象。 */
[[nodiscard]] slicer_core::Json MakeFailure(
    std::string_view code,
    std::string_view message,
    std::string_view detail = {});

/** @brief 从 API 错误构造失败结果对象。 @param error API 错误。 @return 结果对象。 */
[[nodiscard]] slicer_core::Json MakeFailure(
    const slicer_core::api::ApiError& error);

/** @brief 将结果对象转换为终态输出。 @param result 结果对象。 @return 序列化输出。 */
[[nodiscard]] CapabilityOutput MakeCapabilityOutput(
    const slicer_core::Json& result);

/** @brief 解析内部结构化对象。 @param value UTF-8 对象。 @return 解析后的对象。 */
[[nodiscard]] slicer_core::Json ParseStructuredObject(
    const slicer_core::api::StructuredJsonObject& value);

/** @brief 解析十进制模型标识。 @param value 标识文本。 @return 数值标识。 */
[[nodiscard]] std::uint64_t ParseModelId(const std::string& value);

}  // namespace slicesoft::module
