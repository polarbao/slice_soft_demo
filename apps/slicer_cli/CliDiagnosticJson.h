#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/system/ProcessMemoryStats.h"
#include "slicer_core/SliceRunTelemetry.h"
#include "slicer_core/diagnostics/ProductionAdmissionPolicy.h"

namespace slicer_cli
{

/**
 * @brief 把进程内存统计序列化为诊断 JSON。
 * @param memory 采样到的进程内存统计。
 * @return 诊断 JSON 对象。
 */
[[nodiscard]] slicer_core::Json MemoryStatsToJson(
    const slicer_core::ProcessMemoryStats& memory);

/**
 * @brief 把切片运行耗时剖面序列化为诊断 JSON。
 *
 * 该剖面仅供诊断，不属于 RGBWSV 生产包协议的一部分。
 *
 * @param profile 本次运行的耗时剖面。
 * @return 诊断 JSON 对象。
 */
[[nodiscard]] slicer_core::Json SliceRunProfileToJson(
    const slicer_core::SliceRunProfile& profile);

/**
 * @brief 把生产准入裁定序列化为诊断 JSON。
 * @param decision 准入裁定结果。
 * @param mode 本次运行的准入模式。
 * @return 诊断 JSON 对象。
 */
[[nodiscard]] slicer_core::Json AdmissionDecisionToJson(
    const slicer_core::ProductionAdmissionDecision& decision,
    slicer_core::AdmissionMode mode);

}  // namespace slicer_cli
