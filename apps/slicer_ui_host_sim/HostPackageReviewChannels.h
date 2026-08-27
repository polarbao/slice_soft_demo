#pragma once

// 信道名与冻结信道集的判定。原先与 HostPackageReviewController 同处一个 TU，
// MATVOL-T 的 T-09 把该文件从 468 行推到 505 行、越过宿主 500 行边界门。
// 这组助手自成一体且只依赖 Qt 基本类型，抽出后两侧都回到限额内，
// 比把文件计入债务台账更合适——台账是记账，不是修复。

#include "HostPackageReviewController.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

class QComboBox;

/// @brief 按信道名把 JSON 计数读入固定槽位；出现未知信道名即判失败。
bool ReadChannelCounts(
    const QJsonObject& object,
    const QStringList& channels,
    hostchannelcounts* counts);

/// @brief 信道集是否恰为冻结的 RGBWSV 六通道或 RGBWSVT 七通道。
bool IsFrozenChannelSet(const QStringList& channels);

/// @brief 单个信道名是否属于冻结集合。
bool IsFrozenChannelName(const QString& channel);

/// @brief 按生产包实际通道集自动选定预览模式，并禁用该包不具备的通道项。
///
/// 预览模式此前固定默认为六通道组合（硬编码 setCurrentIndex(4)），
/// 于是七通道包打开后缩裹默认不可见；而 T 相关项在六通道包上又是可选但必然失败的。
/// 两者都要求用户先知道包是几通道、再手工挑对项——这个知识不该由用户承担。
void SelectDefaultPreviewMode(
    QComboBox* combo,
    const QStringList& packageChannels);
