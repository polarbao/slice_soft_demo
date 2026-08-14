#pragma once

#include "slicer_core/api/SliceFacade.h"

#include <filesystem>
#include <functional>
#include <string>

namespace slicer_core::engine
{

/** @brief 生产切片前所需的已解析不可变标识。 */
struct SliceSubmissionContract
{
    std::string scenehash;
    std::filesystem::path packagedir;
};

/** @brief 从有效配置解析已提交场景及输出标识。 */
using SliceSubmissionContractResolver = std::function<
    api::ApiResult<SliceSubmissionContract>(
        const std::filesystem::path&)>;

/** @brief 调用现有生产场景切片器并返回生产包摘要。 */
using SliceProductionRunner = std::function<api::ApiResult<api::SliceResult>(
    const std::filesystem::path&,
    const api::ProgressSink&)>;

/** @brief 使用显式同步取消令牌调用生产切片。 */
using CancellableSliceProductionRunner =
    std::function<api::ApiResult<api::SliceResult>(
        const api::SliceRequest&,
        const api::ICancelToken&,
        const api::ProgressSink&)>;

/**
 * @brief 封装现有生产入口的引擎侧 SliceFacade 适配器。
 *
 * 适配器验证调用方持有的标识、转发单调进度，并将协作式取消请求转换为
 * 冻结 PM 错误；它不持有材质、栅格、TIFF 或发布策略。
 */
class SliceFacadeAdapter final : public api::SliceFacade
{
public:
    /**
     * @brief 使用显式生产绑定创建适配器。
     * @param contractResolver 有效配置标识解析器。
     * @param productionRunner 现有生产切片入口适配器。
     */
    SliceFacadeAdapter(
        SliceSubmissionContractResolver contractResolver,
        SliceProductionRunner productionRunner);

    /**
     * @brief 使用可感知取消的生产绑定创建适配器。
     * @param contractResolver 有效配置标识解析器。
     * @param productionRunner 接收调用方令牌的生产入口。
     */
    SliceFacadeAdapter(
        SliceSubmissionContractResolver contractResolver,
        CancellableSliceProductionRunner productionRunner);

    /**
     * @brief 对一个已提交场景执行生产切片。
     * @param request 调用方持有的作业、场景、配置和 Package 标识。
     * @param cancelToken 协作式取消源。
     * @param progressSink 单调进度观察器。
     * @return 生产包摘要或稳定的 PM-SLICER 错误。
     */
    [[nodiscard]] api::ApiResult<api::SliceResult> Run(
        const api::SliceRequest& request,
        const api::ICancelToken& cancelToken,
        const api::ProgressSink& progressSink) noexcept override;

private:
    SliceSubmissionContractResolver m_contractResolver;
    CancellableSliceProductionRunner m_productionRunner;
};

}  // namespace slicer_core::engine
