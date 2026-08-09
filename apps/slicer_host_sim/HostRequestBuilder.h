#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Host-owned material strategy for an effective slice Profile. */
enum hostmaterialstrategy
{
    HOST_MATERIAL_RGB_SOLID = 0,
    HOST_MATERIAL_RGB_WHITE = 1,
    HOST_MATERIAL_RGB_VARNISH = 2,
    HOST_MATERIAL_RGB_WHITE_VARNISH = 3,
    HOST_MATERIAL_WHITE_SOLID = 4,
    HOST_MATERIAL_VARNISH_SOLID = 5
};

/** @brief Host-owned default role for input material mapping. */
enum hostmaterialrole
{
    HOST_MATERIAL_ROLE_RGB = 0,
    HOST_MATERIAL_ROLE_WHITE = 1,
    HOST_MATERIAL_ROLE_VARNISH = 2,
    HOST_MATERIAL_ROLE_IGNORE = 3,
    HOST_MATERIAL_ROLE_SUPPORT_CANDIDATE = 4
};

/** @brief Host-owned support mode written to the effective Profile. */
enum hostsupportmode
{
    HOST_SUPPORT_NONE = 0,
    HOST_SUPPORT_BOTTOM_PROJECTION = 1,
    HOST_SUPPORT_UNSUPPORTED_ONLY = 2,
    HOST_SUPPORT_BOTTOM_PLUS_UNSUPPORTED = 3,
    HOST_SUPPORT_FULL_VERTICAL_PROJECTION = 4
};

/** @brief C-compatible inputs used to build one effective slice Profile. */
struct hosteffectiveprofilesettings
{
    const char* modelpath;
    const char* modelformat;
    const char* packagedirectory;
    const char* profileid;
    int dpix;
    int dpiy;
    double layerthicknessmm;
    enum hostmaterialstrategy materialstrategy;
    int materialrolemappingenabled;
    enum hostmaterialrole materialdefaultrole;
    int mapwhitenames;
    int mapvarnishnames;
    int allowinputsupportmaterial;
    int whiteexpandpx;
    int whiteshrinkpx;
    int varnishtoplayers;
    int maxunexpectedoverlappixels;
    int supportenabled;
    enum hostsupportmode supportmode;
    double supportoffsetmm;
    int supportminareapx;
    int internalvoidenabled;
    int internalvoidminareapx;
    int baseprojectionenabled;
    int baseprojectionlayercount;
};

/**
 * @brief Builds a self-hashed effective Profile for the reference slice.
 * @param modelPath Normalized absolute model path.
 * @param packageDirectory Normalized absolute package directory.
 * @param profileHash Receives `sha256:` plus 64 lowercase hex characters.
 * @param profileHashCapacity Output buffer capacity.
 * @return Heap JSON string owned by the caller, or NULL on failure.
 */
char* HostBuildProfile(
    const char* modelPath,
    const char* packageDirectory,
    char* profileHash,
    unsigned long profileHashCapacity);

/**
 * @brief Builds a self-hashed effective Profile with an explicit layer height.
 * @param modelPath Normalized absolute model path.
 * @param packageDirectory Normalized absolute package directory.
 * @param layerThicknessMm Positive layer thickness in millimetres.
 * @param profileHash Receives `sha256:` plus 64 lowercase hex characters.
 * @param profileHashCapacity Output buffer capacity.
 * @return Heap JSON string owned by the caller, or NULL on failure.
 */
char* HostBuildProfileWithLayerThickness(
    const char* modelPath,
    const char* packageDirectory,
    double layerThicknessMm,
    char* profileHash,
    unsigned long profileHashCapacity);

/**
 * @brief Builds a parameterized self-hashed effective Profile.
 * @param settings Valid host-owned model, output and material settings.
 * @param profileHash Receives `sha256:` plus 64 lowercase hex characters.
 * @param profileHashCapacity Output buffer capacity.
 * @return Heap JSON string owned by the caller, or NULL on validation failure.
 */
char* HostBuildEffectiveProfile(
    const struct hosteffectiveprofilesettings* settings,
    char* profileHash,
    unsigned long profileHashCapacity);

#ifdef __cplusplus
}
#endif
