#pragma once

#include <optional>
#include <string>
#include <utility>

namespace slicer_core::api {

/** @brief 每个 Facade 边界返回的稳定错误载荷。 */
struct ApiError
{
    std::string code;
    std::string message;
    std::string detail;
};

/** @brief 以值或错误表示调用结果，阻止异常跨越 Facade 边界。 */
template <class T>
class ApiResult
{
public:
    /** @brief 创建成功结果。 @param value 返回值。 @return 成功结果。 */
    static ApiResult Success(T value)
    {
        return ApiResult(
            true,
            std::optional<T>(std::move(value)),
            std::nullopt);
    }

    /** @brief 创建失败结果。 @param error 稳定的 PM-SLICER 错误。 @return 失败结果。 */
    static ApiResult Failure(ApiError error)
    {
        return ApiResult(
            false,
            std::nullopt,
            std::optional<ApiError>(std::move(error)));
    }

    /** @brief 报告调用是否成功。 @return 存在结果值时返回 true。 */
    [[nodiscard]] bool IsOk() const noexcept
    {
        return m_ok;
    }

    /** @brief 不抛异常地返回结果值。 @return 值指针或 nullptr。 */
    [[nodiscard]] const T* Value() const noexcept
    {
        return m_value ? &*m_value : nullptr;
    }

    /** @brief 不抛异常地返回错误。 @return 错误指针或 nullptr。 */
    [[nodiscard]] const ApiError* Error() const noexcept
    {
        return m_error ? &*m_error : nullptr;
    }

private:
    ApiResult(bool ok, std::optional<T> value, std::optional<ApiError> error)
        : m_ok(ok), m_value(std::move(value)), m_error(std::move(error))
    {
    }

    bool m_ok{false};
    std::optional<T> m_value;
    std::optional<ApiError> m_error;
};

/** @brief 供释放和命令操作使用的 void 特化。 */
template <>
class ApiResult<void>
{
public:
    /** @brief 创建成功的 void 结果。 @return 成功结果。 */
    static ApiResult Success()
    {
        return ApiResult(true, std::nullopt);
    }

    /** @brief 创建失败的 void 结果。 @param error 稳定的 PM-SLICER 错误。 @return 失败结果。 */
    static ApiResult Failure(ApiError error)
    {
        return ApiResult(
            false,
            std::optional<ApiError>(std::move(error)));
    }

    /** @brief 报告调用是否成功。 @return 成功时返回 true。 */
    [[nodiscard]] bool IsOk() const noexcept
    {
        return m_ok;
    }

    /** @brief 不抛异常地返回错误。 @return 错误指针或 nullptr。 */
    [[nodiscard]] const ApiError* Error() const noexcept
    {
        return m_error ? &*m_error : nullptr;
    }

private:
    ApiResult(bool ok, std::optional<ApiError> error)
        : m_ok(ok), m_error(std::move(error))
    {
    }

    bool m_ok{false};
    std::optional<ApiError> m_error;
};

}  // namespace slicer_core::api
