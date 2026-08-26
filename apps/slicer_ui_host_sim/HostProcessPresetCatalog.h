#pragma once

#include "HostSliceSettings.h"

#include <QString>
#include <QVector>

/** @brief 参考 UI 展示的单个宿主侧常用工艺预设。 */
struct hostprocesspreset
{
    QString id;
    QString displayname;
    QString description;
    HostMaterialStrategy materialstrategy{HostMaterialStrategy::RgbSolid};
    hostmaterialprocesssettings materialprocess;
    hosttexturesettings texture;
    hostsupportsettings support;
    hostmaterialvolumesettings materialvolume;
    HostPackageProtocol packageprotocol{HostPackageProtocol::Rgbwsv};
    hosttransferchannelsettings transferchannel;
};

/** @brief 在不读取切片器 fixture 的前提下提供常用工艺预设。 */
class HostProcessPresetCatalog final
{
public:
    /**
     * @brief 返回默认生产工艺预设标识。
     * @return 稳定的 Stage 15 按需补白预设标识。
     */
    static QString DefaultPresetId();

    /**
     * @brief 按显示顺序返回稳定的常用工艺预设。
     * @return 与旧 UI 常用流程等价的宿主侧预设。
     */
    static QVector<hostprocesspreset> Presets();

    /**
     * @brief 按稳定标识解析一个预设。
     * @param presetId 稳定预设标识。
     * @param preset 找到时接收解析后的预设。
     * @return 预设存在时返回 true。
     */
    static bool Resolve(
        const QString& presetId,
        hostprocesspreset* preset);
};
