#pragma once

#include "slicer_core/json_value.h"

#include <chrono>
#include <string>
#include <string_view>

namespace slicesoft::module
{

/** @brief 为一个公共能力请求选择的执行载体。 */
enum class CapabilityCarrier
{
    InProcess,
    Worker
};

/** @brief 已验证的载体决策和规范化 Worker 请求载荷。 */
struct CapabilityRoute
{
    bool accepted{false};
    CapabilityCarrier carrier{CapabilityCarrier::InProcess};
    std::string publicCapability;
    std::string workerCapability;
    std::string jobId;
    std::string correlationId;
    std::chrono::milliseconds timeout{3600000};
    slicer_core::Json workerPayload;
    std::string errorCode;
    std::string errorMessage;
    std::string errorDetail;
};

/**
 * @brief 在不执行算法的前提下分类公共能力请求。
 *
 * 路由器冻结 Stage 14 载体边界：轻量能力留在进程内，完整预检、修复和
 * RGBWSV 切片由 Worker 执行。
 */
class CapabilityCarrierRouter final
{
public:
    /**
     * @brief 解析并分类一个 UTF-8 公共能力请求。
     * @param requestText 公共 SPI 请求 JSON。
     * @return 已接受的载体决策或稳定的失败即拒绝诊断。
     */
    [[nodiscard]] static CapabilityRoute Route(std::string_view requestText);
};

}  // namespace slicesoft::module
