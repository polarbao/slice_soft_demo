#pragma once

#include "HostSliceSettings.h"

class QSettings;

/** @brief 持久化宿主设置中带版本的多材质纵深体积部分。 */
class HostWorkspaceMatvolState final
{
public:
    /**
     * @brief 将多材质纵深设置写入当前工作区分组。
     * @param settings 目标设置存储。
     * @param matvol 待持久化的多材质纵深设置。
     */
    static void Save(
        QSettings& settings,
        const hostmaterialvolumesettings& matvol);

    /**
     * @brief 从当前分组恢复并校验多材质纵深设置。
     * @param settings 源设置存储。
     * @param matvol 接收恢复后的设置。
     * @return 所有字段均有效时返回 true。
     */
    static bool Restore(
        QSettings& settings,
        hostmaterialvolumesettings* matvol);
};
