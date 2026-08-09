#pragma once

#include "HostRequestBuilder.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Builds the texture root member for an effective Profile.
 * @param settings Valid host-owned texture settings.
 * @param canonical Receives the canonical indented member without a comma.
 * @param compact Receives the compact member without a comma.
 * @return Non-zero on success; both output strings are caller-owned.
 */
int HostBuildTextureProfileFragments(
    const struct hosteffectiveprofilesettings* settings,
    char** canonical,
    char** compact);

#ifdef __cplusplus
}
#endif
