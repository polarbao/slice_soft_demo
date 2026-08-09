#pragma once

#include "HostSliceSettings.h"

class QSettings;

/** @brief Persists the versioned production-texture portion of host settings. */
class HostWorkspaceTextureState final
{
public:
    /**
     * @brief Writes texture settings inside the active workspace group.
     * @param settings Destination settings store.
     * @param texture Texture settings to persist.
     */
    static void Save(
        QSettings& settings,
        const hosttexturesettings& texture);

    /**
     * @brief Restores and validates texture settings from the active group.
     * @param settings Source settings store.
     * @param texture Receives restored settings.
     * @return True when every texture field is valid.
     */
    static bool Restore(
        QSettings& settings,
        hosttexturesettings* texture);
};
