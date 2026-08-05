#include "slicer_core/api/Facades.h"

#include <iostream>
#include <string>
#include <type_traits>

namespace {

class TestCancelToken final : public slicer_core::api::ICancelToken
{
public:
    [[nodiscard]] bool IsCancelRequested() const noexcept override
    {
        return false;
    }
};

bool VerifyResultContract()
{
    using slicer_core::api::ApiError;
    using slicer_core::api::ApiResult;

    const ApiResult<int> success = ApiResult<int>::Success(42);
    if (!success.IsOk() || success.Value() == nullptr || *success.Value() != 42)
    {
        return false;
    }

    const ApiResult<int> failure = ApiResult<int>::Failure(
        ApiError{"PM-SLICER-INPUT-0001", "invalid input", "fixture"});
    return !failure.IsOk()
        && failure.Error() != nullptr
        && failure.Error()->code == "PM-SLICER-INPUT-0001";
}

bool VerifyDtoDefaults()
{
    const slicer_core::api::ModelImportRequest request;
    const slicer_core::api::SceneViewData viewData;
    const TestCancelToken cancelToken;
    return request.compute_bbox
        && request.extract_materials
        && viewData.view_mode == slicer_core::api::ViewMode::Top
        && !cancelToken.IsCancelRequested();
}

}  // namespace

int main()
{
    static_assert(std::is_abstract_v<slicer_core::api::ModelFacade>);
    static_assert(std::is_abstract_v<slicer_core::api::PackageQueryFacade>);
    static_assert(std::is_abstract_v<slicer_core::api::SceneFacade>);
    static_assert(std::is_abstract_v<slicer_core::api::SliceFacade>);

    if (!VerifyResultContract() || !VerifyDtoDefaults())
    {
        std::cerr << "Stage 14B facade contract unit tests: FAIL\n";
        return 1;
    }
    std::cout << "Stage 14B facade contract unit tests: PASS\n";
    return 0;
}
