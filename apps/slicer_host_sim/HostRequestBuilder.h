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

/** @brief C-compatible texture application mode. */
enum hosttextureapplymode
{
    HOST_TEXTURE_SOLID_VOLUME_FROM_TOP = 0,
    HOST_TEXTURE_TOP_SURFACE_ONLY = 1,
    HOST_TEXTURE_TOP_SURFACE_BAND = 2
};

/** @brief C-compatible texture sampler. */
enum hosttexturesampler
{
    HOST_TEXTURE_NEAREST = 0,
    HOST_TEXTURE_BILINEAR = 1
};

/** @brief C-compatible UV address mode. */
enum hosttextureuvaddressmode
{
    HOST_TEXTURE_UV_CLAMP = 0,
    HOST_TEXTURE_UV_REPEAT = 1
};

/** @brief C-compatible missing-texture policy. */
enum hosttexturemissingpolicy
{
    HOST_TEXTURE_WARN_AND_FALLBACK = 0,
    HOST_TEXTURE_FAIL_FAST = 1
};

/** @brief C-compatible non-surface RGB policy. */
enum hosttexturenonsurfacepolicy
{
    HOST_TEXTURE_NON_SURFACE_MODEL_MATERIAL = 0,
    HOST_TEXTURE_NON_SURFACE_EMPTY = 1
};

/** @brief C-compatible unprintable-white policy. */
enum hosttexturewhitepolicy
{
    HOST_TEXTURE_WHITE_FAIL_CLOSED = 0,
    HOST_TEXTURE_WHITE_UNDERBASE = 1
};

/** @brief C-compatible geometry occupancy sampling strategy. */
enum hostgeometrysamplingstrategy
{
    HOST_GEOMETRY_SAMPLING_LEGACY_CENTER = 0,
    HOST_GEOMETRY_SAMPLING_SLAB_2X2_AT_LEAST_TWO = 1
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
    int textureenabled;
    enum hosttextureapplymode textureapplymode;
    int texturetopsurfacelayers;
    enum hosttexturesampler texturesampler;
    enum hosttextureuvaddressmode textureuvaddressmode;
    int textureflipv;
    int texturefallbackred;
    int texturefallbackgreen;
    int texturefallbackblue;
    enum hosttexturemissingpolicy texturemissingpolicy;
    enum hosttexturenonsurfacepolicy texturenonsurfacepolicy;
    enum hosttexturewhitepolicy texturewhitepolicy;
    int texturewhiteinkthreshold;
    int texturewhitevalue;
    enum hostgeometrysamplingstrategy geometrysamplingstrategy;
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
