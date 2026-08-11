#pragma once

#include "HostSliceSettings.h"

#include <QString>
#include <QVector>

/** @brief One host-owned common process preset shown by the reference UI. */
struct hostprocesspreset
{
    QString id;
    QString displayname;
    QString description;
    HostMaterialStrategy materialstrategy{HostMaterialStrategy::RgbSolid};
    hostmaterialprocesssettings materialprocess;
    hosttexturesettings texture;
    hostsupportsettings support;
};

/** @brief Provides common process presets without reading slicer fixtures. */
class HostProcessPresetCatalog final
{
public:
    /**
     * @brief Returns the stable common-process presets in display order.
     * @return Host-owned presets equivalent to the old UI's common workflows.
     */
    static QVector<hostprocesspreset> Presets();

    /**
     * @brief Resolves one preset by its stable identity.
     * @param presetId Stable preset identity.
     * @param preset Receives the resolved preset when found.
     * @return True when the preset exists.
     */
    static bool Resolve(
        const QString& presetId,
        hostprocesspreset* preset);
};
