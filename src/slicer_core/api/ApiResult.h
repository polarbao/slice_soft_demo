#pragma once

#include <optional>
#include <string>
#include <utility>

namespace slicer_core::api {

/** @brief Stable error payload returned by every facade boundary. */
struct ApiError
{
    std::string code;
    std::string message;
    std::string detail;
};

/** @brief Value-or-error result that prevents exceptions crossing a facade. */
template <class T>
class ApiResult
{
public:
    /** @brief Creates a successful result. @param value Returned value. @return Success result. */
    static ApiResult Success(T value)
    {
        return ApiResult(
            true,
            std::optional<T>(std::move(value)),
            std::nullopt);
    }

    /** @brief Creates a failed result. @param error Stable PM-SLICER error. @return Failure result. */
    static ApiResult Failure(ApiError error)
    {
        return ApiResult(
            false,
            std::nullopt,
            std::optional<ApiError>(std::move(error)));
    }

    /** @brief Reports whether the call succeeded. @return True for a value result. */
    [[nodiscard]] bool IsOk() const noexcept
    {
        return m_ok;
    }

    /** @brief Returns the value without throwing. @return Value pointer or nullptr. */
    [[nodiscard]] const T* Value() const noexcept
    {
        return m_value ? &*m_value : nullptr;
    }

    /** @brief Returns the error without throwing. @return Error pointer or nullptr. */
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

/** @brief Void specialization used by release and command operations. */
template <>
class ApiResult<void>
{
public:
    /** @brief Creates a successful void result. @return Success result. */
    static ApiResult Success()
    {
        return ApiResult(true, std::nullopt);
    }

    /** @brief Creates a failed void result. @param error Stable PM-SLICER error. @return Failure result. */
    static ApiResult Failure(ApiError error)
    {
        return ApiResult(
            false,
            std::optional<ApiError>(std::move(error)));
    }

    /** @brief Reports whether the call succeeded. @return True on success. */
    [[nodiscard]] bool IsOk() const noexcept
    {
        return m_ok;
    }

    /** @brief Returns the error without throwing. @return Error pointer or nullptr. */
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
