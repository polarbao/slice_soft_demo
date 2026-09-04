#pragma once

// 预览模式选择依赖 Qt Widgets（QComboBox / QStandardItemModel），
// 而 HostPackageReviewChannels 只依赖 Qt Core——后者被 hostflow_hb07 等
// 仅链接 Core+Gui 的目标编译。把两者混在一处会让那些目标编译失败。
// 故按 Qt 模块边界拆开：本文件只注册进链接 Widgets 的目标。

#include <QStringList>

class QComboBox;

/// @brief 按生产包实际通道集自动选定预览模式，并禁用该包不具备的通道项。
///
/// 预览模式此前固定默认为六通道组合（硬编码 setCurrentIndex(4)），
/// 于是七通道包打开后缩裹默认不可见；而 T 相关项在六通道包上又是可选但必然失败的。
/// 两者都要求用户先知道包是几通道、再手工挑对项——这个知识不该由用户承担。
void SelectDefaultPreviewMode(
    QComboBox* combo,
    const QStringList& packageChannels);

/// @brief 给以伪彩色渲染的单通道项补上说明 tooltip。
///
/// W/S/V/T 在预览中都是伪彩色，不加说明会被误当成生产 TIFF 的像素值；
/// R/G/B 是真实材质色，不在此列。
void AttachPseudoColourTooltips(QComboBox* combo);
