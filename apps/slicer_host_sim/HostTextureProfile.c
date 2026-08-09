#include "HostTextureProfile.h"

#include "JsonText.h"

#include <stdlib.h>

static const char* ApplyModeId(const enum hosttextureapplymode mode)
{
    switch (mode)
    {
    case HOST_TEXTURE_SOLID_VOLUME_FROM_TOP:
        return "solid_volume_from_top_surface";
    case HOST_TEXTURE_TOP_SURFACE_ONLY:
        return "top_surface_only";
    case HOST_TEXTURE_TOP_SURFACE_BAND:
        return "top_surface_band";
    }
    return NULL;
}

static const char* SamplerId(const enum hosttexturesampler sampler)
{
    switch (sampler)
    {
    case HOST_TEXTURE_NEAREST: return "nearest";
    case HOST_TEXTURE_BILINEAR: return "bilinear";
    }
    return NULL;
}

static const char* AddressModeId(
    const enum hosttextureuvaddressmode mode)
{
    switch (mode)
    {
    case HOST_TEXTURE_UV_CLAMP: return "clamp";
    case HOST_TEXTURE_UV_REPEAT: return "repeat";
    }
    return NULL;
}

static const char* MissingPolicyId(
    const enum hosttexturemissingpolicy policy)
{
    switch (policy)
    {
    case HOST_TEXTURE_WARN_AND_FALLBACK: return "warn_and_fallback";
    case HOST_TEXTURE_FAIL_FAST: return "fail_fast";
    }
    return NULL;
}

static const char* NonSurfacePolicyId(
    const enum hosttexturenonsurfacepolicy policy)
{
    switch (policy)
    {
    case HOST_TEXTURE_NON_SURFACE_MODEL_MATERIAL:
        return "model_material";
    case HOST_TEXTURE_NON_SURFACE_EMPTY:
        return "empty";
    }
    return NULL;
}

static const char* WhitePolicyId(
    const enum hosttexturewhitepolicy policy)
{
    switch (policy)
    {
    case HOST_TEXTURE_WHITE_FAIL_CLOSED: return "fail_closed";
    case HOST_TEXTURE_WHITE_UNDERBASE: return "white_underbase";
    }
    return NULL;
}

int HostBuildTextureProfileFragments(
    const struct hosteffectiveprofilesettings* settings,
    char** canonical,
    char** compact)
{
    const char* applyMode;
    const char* sampler;
    const char* addressMode;
    const char* missingPolicy;
    const char* nonSurfacePolicy;
    const char* whitePolicy;
    if (settings == NULL || canonical == NULL || compact == NULL)
    {
        return 0;
    }
    *canonical = NULL;
    *compact = NULL;
    applyMode = ApplyModeId(settings->textureapplymode);
    sampler = SamplerId(settings->texturesampler);
    addressMode = AddressModeId(settings->textureuvaddressmode);
    missingPolicy = MissingPolicyId(settings->texturemissingpolicy);
    nonSurfacePolicy = NonSurfacePolicyId(
        settings->texturenonsurfacepolicy);
    whitePolicy = WhitePolicyId(settings->texturewhitepolicy);
    if (applyMode == NULL || sampler == NULL || addressMode == NULL
        || missingPolicy == NULL || nonSurfacePolicy == NULL
        || whitePolicy == NULL
        || settings->texturetopsurfacelayers <= 0
        || settings->texturetopsurfacelayers > 100000
        || settings->texturefallbackred < 0
        || settings->texturefallbackred > 255
        || settings->texturefallbackgreen < 0
        || settings->texturefallbackgreen > 255
        || settings->texturefallbackblue < 0
        || settings->texturefallbackblue > 255
        || settings->texturewhiteinkthreshold < 0
        || settings->texturewhiteinkthreshold > 255
        || settings->texturewhitevalue < 0
        || settings->texturewhitevalue > 255)
    {
        return 0;
    }
    *canonical = HostFormat(
        "\"texture\": {\n\"applyMode\": \"%s\",\n"
        "\"enabled\": %s,\n\"fallbackRgb\": [\n%d,\n%d,\n%d\n],\n"
        "\"flipV\": %s,\n\"missingTexturePolicy\": \"%s\",\n"
        "\"nonSurfaceRgbPolicy\": \"%s\",\n"
        "\"sampler\": \"%s\",\n\"topSurfaceLayers\": %d,\n"
        "\"unprintableWhiteInkThreshold\": %d,\n"
        "\"unprintableWhitePolicy\": \"%s\",\n"
        "\"unprintableWhiteValue\": %d,\n"
        "\"uvAddressMode\": \"%s\"\n}\n",
        applyMode,
        settings->textureenabled != 0 ? "true" : "false",
        settings->texturefallbackred,
        settings->texturefallbackgreen,
        settings->texturefallbackblue,
        settings->textureflipv != 0 ? "true" : "false",
        missingPolicy, nonSurfacePolicy, sampler,
        settings->texturetopsurfacelayers,
        settings->texturewhiteinkthreshold, whitePolicy,
        settings->texturewhitevalue, addressMode);
    *compact = HostFormat(
        "\"texture\":{"
        "\"applyMode\":\"%s\",\"enabled\":%s,"
        "\"fallbackRgb\":[%d,%d,%d],\"flipV\":%s,"
        "\"missingTexturePolicy\":\"%s\","
        "\"nonSurfaceRgbPolicy\":\"%s\",\"sampler\":\"%s\","
        "\"topSurfaceLayers\":%d,"
        "\"unprintableWhiteInkThreshold\":%d,"
        "\"unprintableWhitePolicy\":\"%s\","
        "\"unprintableWhiteValue\":%d,"
        "\"uvAddressMode\":\"%s\"}",
        applyMode,
        settings->textureenabled != 0 ? "true" : "false",
        settings->texturefallbackred,
        settings->texturefallbackgreen,
        settings->texturefallbackblue,
        settings->textureflipv != 0 ? "true" : "false",
        missingPolicy, nonSurfacePolicy, sampler,
        settings->texturetopsurfacelayers,
        settings->texturewhiteinkthreshold, whitePolicy,
        settings->texturewhitevalue, addressMode);
    if (*canonical == NULL || *compact == NULL)
    {
        free(*canonical);
        free(*compact);
        *canonical = NULL;
        *compact = NULL;
        return 0;
    }
    return 1;
}
