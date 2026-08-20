#pragma once

// MATVOL MV-02：稳定错误码三件套。
// 取值与 DEV_MATVOL §8 冻结清单一致；what() 形如 `E_MATVOL_XXX: <message>`。

#include <stdexcept>
#include <string>

namespace slicer_core
{

/// @brief MATVOL 稳定错误码。
enum class MaterialVolumeErrorCode
{
    UnsupportedPipeline,
    MaterialMissing,
    OpenSurfaceRequiresPolicy,
    TopologyInvalid,
    IntersectionUnpaired,
    OverlapUnresolved,
    ModelPixelUnowned,
    ReplayMismatch,
    BudgetExceeded,
};

/// @brief 稳定错误码字符串，形如 `E_MATVOL_UNSUPPORTED_PIPELINE`。
[[nodiscard]] std::string MaterialVolumeErrorCodeName(MaterialVolumeErrorCode code);

/// @brief 携带稳定错误码的异常。
class MaterialVolumeError : public std::runtime_error
{
public:
    MaterialVolumeError(MaterialVolumeErrorCode code, const std::string& message);

    [[nodiscard]] MaterialVolumeErrorCode Code() const noexcept;

private:
    MaterialVolumeErrorCode code_;
};

}  // namespace slicer_core
