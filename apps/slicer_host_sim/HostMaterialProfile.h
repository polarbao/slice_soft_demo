#pragma once

#include "HostRequestBuilder.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Builds the material-related root members for an effective Profile.
 * @param settings Valid host-owned material and support settings.
 * @param escapedProfileId JSON-escaped Profile identity without quotes.
 * @param canonical Receives the canonical indented fragment.
 * @param compact Receives the compact response fragment.
 * @return Non-zero on success; both output strings are caller-owned.
 */
int HostBuildMaterialProfileFragments(
    const struct hosteffectiveprofilesettings* settings,
    const char* escapedProfileId,
    char** canonical,
    char** compact);

#ifdef __cplusplus
}
#endif
