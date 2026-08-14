#pragma once

#include "HostSliceSettings.h"

class QSettings;

/** @brief 持久化宿主设置中带版本的生产纹理部分。 */
class HostWorkspaceTextureState final
{
public:
    /**
     * @brief 将纹理设置写入当前工作区分组。
     * @param settings 目标设置存储。
     * @param texture 待持久化的纹理设置。
     */
    static void Save(
        QSettings& settings,
        const hosttexturesettings& texture);

    /**
     * @brief 从当前分组恢复并校验纹理设置。
     * @param settings 源设置存储。
     * @param texture 接收恢复后的设置。
     * @return 所有纹理字段均有效时返回 true。
     */
    static bool Restore(
        QSettings& settings,
        hosttexturesettings* texture);
};
