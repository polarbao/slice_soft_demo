#pragma once

// 信道名与冻结信道集的判定。原先与 HostPackageReviewController 同处一个 TU，
// MATVOL-T 的 T-09 把该文件从 468 行推到 505 行、越过宿主 500 行边界门。
// 这组助手自成一体且只依赖 Qt 基本类型，抽出后两侧都回到限额内，
// 比把文件计入债务台账更合适——台账是记账，不是修复。

#include "HostPackageReviewController.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>


/// @brief 按信道名把 JSON 计数读入固定槽位；出现未知信道名即判失败。
bool ReadChannelCounts(
    const QJsonObject& object,
    const QStringList& channels,
    hostchannelcounts* counts);

/// @brief 信道集是否恰为冻结的 RGBWSV 六通道或 RGBWSVT 七通道。
bool IsFrozenChannelSet(const QStringList& channels);

/// @brief 单个信道名是否属于冻结集合。
bool IsFrozenChannelName(const QString& channel);

