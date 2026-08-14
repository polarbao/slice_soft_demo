#pragma once

#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"

namespace slicer_core
{

/**
 * @brief 验证成对的 capability-v1.2 生产包摘要证据。
 * @param request RGBWSV 生产包写入请求。
 * @throws std::invalid_argument 证据不完整或格式错误时抛出。
 */
void ValidateRgbwsvCapabilitySummary(
    const RgbwsvProductionPackageWriteRequest& request);

/**
 * @brief 将已验证能力摘要字段追加到 manifest 对象。
 * @param manifestObject 可变的生产 manifest 对象。
 * @param request RGBWSV 生产包写入请求。
 */
void AppendRgbwsvCapabilitySummary(
    Json::Object& manifestObject,
    const RgbwsvProductionPackageWriteRequest& request);

}  // namespace slicer_core
