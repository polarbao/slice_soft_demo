#include "HostMaterialProfile.h"

#include "JsonText.h"

#include <stdlib.h>

static const char* RoleId(const enum hostmaterialrole role)
{
    switch (role)
    {
    case HOST_MATERIAL_ROLE_RGB: return "rgb";
    case HOST_MATERIAL_ROLE_WHITE: return "white";
    case HOST_MATERIAL_ROLE_VARNISH: return "varnish";
    case HOST_MATERIAL_ROLE_IGNORE: return "ignore";
    case HOST_MATERIAL_ROLE_SUPPORT_CANDIDATE:
        return "support_candidate";
    }
    return NULL;
}

static char* BuildRulesCanonical(
    const int mapWhite,
    const int mapVarnish)
{
    if (mapWhite != 0 && mapVarnish != 0)
    {
        return HostFormat(
            "[\n{\n\"matchNameContains\": \"white\",\n"
            "\"role\": \"white\"\n},\n"
            "{\n\"matchNameContains\": \"varnish\",\n"
            "\"role\": \"varnish\"\n}\n]");
    }
    if (mapWhite != 0)
    {
        return HostFormat(
            "[\n{\n\"matchNameContains\": \"white\",\n"
            "\"role\": \"white\"\n}\n]");
    }
    if (mapVarnish != 0)
    {
        return HostFormat(
            "[\n{\n\"matchNameContains\": \"varnish\",\n"
            "\"role\": \"varnish\"\n}\n]");
    }
    return HostFormat("[]");
}

static char* BuildRulesCompact(
    const int mapWhite,
    const int mapVarnish)
{
    if (mapWhite != 0 && mapVarnish != 0)
    {
        return HostFormat(
            "[{\"matchNameContains\":\"white\",\"role\":\"white\"},"
            "{\"matchNameContains\":\"varnish\","
            "\"role\":\"varnish\"}]");
    }
    if (mapWhite != 0)
    {
        return HostFormat(
            "[{\"matchNameContains\":\"white\",\"role\":\"white\"}]");
    }
    if (mapVarnish != 0)
    {
        return HostFormat(
            "[{\"matchNameContains\":\"varnish\","
            "\"role\":\"varnish\"}]");
    }
    return HostFormat("[]");
}

static int BuildWhiteCarrierFragments(
    const struct hosteffectiveprofilesettings* settings,
    const char* escapedProfileId,
    char** canonical,
    char** compact)
{
    if (settings->materialstrategy != HOST_MATERIAL_RGB_SOLID
        || settings->materialrolemappingenabled != 0
        || settings->textureenabled == 0
        || settings->textureapplymode
            != HOST_TEXTURE_SOLID_VOLUME_FROM_TOP)
    {
        return 0;
    }
    *canonical = HostFormat(
        "\"materialPolicy\": {\n"
        "\"conflictPolicy\": \"model_material_over_support\",\n"
        "\"enabled\": false,\n"
        "\"rgb\": {\n\"enabled\": false,\n"
        "\"source\": \"modelMaterial\"\n},\n"
        "\"varnish\": {\n\"enabled\": false,\n"
        "\"mode\": \"disabled\",\n\"topLayers\": 1,\n"
        "\"value\": 0\n},\n"
        "\"white\": {\n\"enabled\": false,\n"
        "\"layers\": \"all_model\",\n\"mode\": \"disabled\",\n"
        "\"value\": 0\n}\n},\n"
        "\"materialProcessProfile\": {\n\"enabled\": true,\n"
        "\"name\": \"%s\",\n"
        "\"rgb\": {\n\"enabled\": true,\n"
        "\"source\": \"texture_or_color\"\n},\n"
        "\"support\": {\n\"expected\": %s,\n"
        "\"mode\": \"existing_support_pipeline\"\n},\n"
        "\"target\": \"host-reference\",\n"
        "\"validation\": {\n"
        "\"maxUnexpectedOverlapPixels\": %d,\n"
        "\"requireRgbPixels\": true,\n"
        "\"requireSupportPixels\": %s,\n"
        "\"requireVarnishPixels\": false,\n"
        "\"requireWhitePixels\": false\n},\n"
        "\"varnish\": {\n\"coverage\": \"model_surface\",\n"
        "\"enabled\": false,\n\"mode\": \"disabled\",\n"
        "\"topLayers\": 1,\n\"value\": 0\n},\n"
        "\"white\": {\n"
        "\"coverage\": \"texture_unprintable_white\",\n"
        "\"enabled\": true,\n\"expandPx\": 0,\n"
        "\"mode\": \"unprintable_white_underbase\",\n"
        "\"shrinkPx\": 0,\n\"value\": %d\n}\n},\n"
        "\"materialRoleMapping\": {\n"
        "\"allowInputSupportMaterial\": false,\n"
        "\"defaultRole\": \"rgb\",\n\"enabled\": false,\n"
        "\"mode\": \"rules_then_default\",\n\"rules\": []\n},\n"
        "\"modelFill\": {\n\"emptyAllowedInProduction\": false,\n"
        "\"enabled\": true,\n\"legacyRgbFallback\": false,\n"
        "\"material\": \"rgb\",\n"
        "\"scope\": \"below_texture_surface\",\n\"value\": 0\n},\n"
        "\"modelMaterial\": {\n\"applyMode\": \"solid_volume\",\n"
        "\"materialChannel\": \"RGB\",\n"
        "\"rgb\": [\n0,\n0,\n0\n],\n"
        "\"varnishValue\": 255,\n\"whiteValue\": 255\n},\n",
        escapedProfileId,
        settings->supportenabled != 0 ? "true" : "false",
        settings->maxunexpectedoverlappixels,
        settings->supportenabled != 0 ? "true" : "false",
        settings->texturewhitevalue);
    *compact = HostFormat(
        "\"materialPolicy\":{\"conflictPolicy\":"
        "\"model_material_over_support\",\"enabled\":false,"
        "\"rgb\":{\"enabled\":false,\"source\":\"modelMaterial\"},"
        "\"varnish\":{\"enabled\":false,\"mode\":\"disabled\","
        "\"topLayers\":1,\"value\":0},\"white\":{\"enabled\":false,"
        "\"layers\":\"all_model\",\"mode\":\"disabled\",\"value\":0}},"
        "\"materialProcessProfile\":{\"enabled\":true,\"name\":\"%s\","
        "\"rgb\":{\"enabled\":true,\"source\":\"texture_or_color\"},"
        "\"support\":{\"expected\":%s,"
        "\"mode\":\"existing_support_pipeline\"},"
        "\"target\":\"host-reference\",\"validation\":{"
        "\"maxUnexpectedOverlapPixels\":%d,\"requireRgbPixels\":true,"
        "\"requireSupportPixels\":%s,\"requireVarnishPixels\":false,"
        "\"requireWhitePixels\":false},\"varnish\":{"
        "\"coverage\":\"model_surface\",\"enabled\":false,"
        "\"mode\":\"disabled\",\"topLayers\":1,\"value\":0},"
        "\"white\":{\"coverage\":\"texture_unprintable_white\","
        "\"enabled\":true,\"expandPx\":0,"
        "\"mode\":\"unprintable_white_underbase\",\"shrinkPx\":0,"
        "\"value\":%d}},\"materialRoleMapping\":{"
        "\"allowInputSupportMaterial\":false,\"defaultRole\":\"rgb\","
        "\"enabled\":false,\"mode\":\"rules_then_default\",\"rules\":[]},"
        "\"modelFill\":{\"emptyAllowedInProduction\":false,"
        "\"enabled\":true,\"legacyRgbFallback\":false,"
        "\"material\":\"rgb\",\"scope\":\"below_texture_surface\","
        "\"value\":0},\"modelMaterial\":{"
        "\"applyMode\":\"solid_volume\",\"materialChannel\":\"RGB\","
        "\"rgb\":[0,0,0],\"varnishValue\":255,\"whiteValue\":255},",
        escapedProfileId,
        settings->supportenabled != 0 ? "true" : "false",
        settings->maxunexpectedoverlappixels,
        settings->supportenabled != 0 ? "true" : "false",
        settings->texturewhitevalue);
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

int HostBuildMaterialProfileFragments(
    const struct hosteffectiveprofilesettings* settings,
    const char* escapedProfileId,
    char** canonical,
    char** compact)
{
    int rgbEnabled = 0;
    int whiteEnabled = 0;
    int varnishEnabled = 0;
    const char* materialChannel = NULL;
    const char* modelFillMaterial = "white";
    const char* defaultRole;
    const char* whiteMode;
    const char* varnishMode;
    int red = 255;
    int green = 255;
    int blue = 255;
    int whiteValue = 255;
    int varnishValue = 255;
    char* rulesCanonical = NULL;
    char* rulesCompact = NULL;
    if (settings == NULL || escapedProfileId == NULL
        || canonical == NULL || compact == NULL)
    {
        return 0;
    }
    *canonical = NULL;
    *compact = NULL;
    if (settings->texturewhitepolicy
        == HOST_TEXTURE_WHITE_UNDERBASE)
    {
        return BuildWhiteCarrierFragments(
            settings, escapedProfileId, canonical, compact);
    }
    switch (settings->materialstrategy)
    {
    case HOST_MATERIAL_RGB_SOLID:
        rgbEnabled = 1;
        materialChannel = "RGB";
        red = green = blue = 0;
        break;
    case HOST_MATERIAL_RGB_WHITE:
        rgbEnabled = whiteEnabled = 1;
        materialChannel = "RGB";
        red = green = blue = 0;
        break;
    case HOST_MATERIAL_RGB_VARNISH:
        rgbEnabled = varnishEnabled = 1;
        modelFillMaterial = "varnish";
        materialChannel = "RGB";
        red = green = blue = 0;
        break;
    case HOST_MATERIAL_RGB_WHITE_VARNISH:
        rgbEnabled = whiteEnabled = varnishEnabled = 1;
        materialChannel = "RGB";
        red = green = blue = 0;
        break;
    case HOST_MATERIAL_WHITE_SOLID:
        whiteEnabled = 1;
        materialChannel = "W";
        whiteValue = 0;
        break;
    case HOST_MATERIAL_VARNISH_SOLID:
        varnishEnabled = 1;
        modelFillMaterial = "varnish";
        materialChannel = "V";
        varnishValue = 0;
        break;
    default:
        return 0;
    }
    defaultRole = RoleId(settings->materialdefaultrole);
    if (defaultRole == NULL)
    {
        return 0;
    }
    whiteMode = whiteEnabled != 0
        ? (rgbEnabled != 0 ? "underbase" : "all_model")
        : "disabled";
    varnishMode = varnishEnabled != 0
        ? (rgbEnabled != 0 ? "top_n_layers" : "all_model")
        : "disabled";
    rulesCanonical = BuildRulesCanonical(
        settings->mapwhitenames, settings->mapvarnishnames);
    rulesCompact = BuildRulesCompact(
        settings->mapwhitenames, settings->mapvarnishnames);
    if (rulesCanonical == NULL || rulesCompact == NULL)
    {
        goto cleanup;
    }
    *canonical = HostFormat(
        "\"materialPolicy\": {\n"
        "\"conflictPolicy\": \"model_material_over_support\",\n"
        "\"enabled\": true,\n"
        "\"rgb\": {\n\"enabled\": %s,\n"
        "\"source\": \"modelMaterial\"\n},\n"
        "\"varnish\": {\n\"enabled\": %s,\n"
        "\"mode\": \"%s\",\n\"topLayers\": %d,\n"
        "\"value\": 0\n},\n"
        "\"white\": {\n\"enabled\": %s,\n"
        "\"layers\": \"all_model\",\n\"mode\": \"%s\",\n"
        "\"value\": 0\n}\n},\n"
        "\"materialProcessProfile\": {\n\"enabled\": true,\n"
        "\"name\": \"%s\",\n"
        "\"rgb\": {\n\"enabled\": %s,\n"
        "\"source\": \"modelMaterial\"\n},\n"
        "\"support\": {\n\"expected\": %s,\n"
        "\"mode\": \"existing_support_pipeline\"\n},\n"
        "\"target\": \"host-reference\",\n"
        "\"validation\": {\n"
        "\"maxUnexpectedOverlapPixels\": %d,\n"
        "\"requireRgbPixels\": %s,\n"
        "\"requireSupportPixels\": %s,\n"
        "\"requireVarnishPixels\": %s,\n"
        "\"requireWhitePixels\": %s\n},\n"
        "\"varnish\": {\n\"coverage\": \"model_surface\",\n"
        "\"enabled\": %s,\n\"mode\": \"%s\",\n"
        "\"topLayers\": %d,\n\"value\": 0\n},\n"
        "\"white\": {\n\"coverage\": \"all_model\",\n"
        "\"enabled\": %s,\n\"expandPx\": %d,\n"
        "\"mode\": \"%s\",\n\"shrinkPx\": %d,\n"
        "\"value\": 0\n}\n},\n"
        "\"materialRoleMapping\": {\n"
        "\"allowInputSupportMaterial\": %s,\n"
        "\"defaultRole\": \"%s\",\n\"enabled\": %s,\n"
        "\"mode\": \"rules_then_default\",\n"
        "\"rules\": %s\n},\n"
        "\"modelFill\": {\n\"emptyAllowedInProduction\": false,\n"
        "\"enabled\": %s,\n\"legacyRgbFallback\": false,\n"
        "\"material\": \"%s\",\n\"scope\": \"all_model\",\n"
        "\"value\": 0\n},\n"
        "\"modelMaterial\": {\n\"applyMode\": \"solid_volume\",\n"
        "\"materialChannel\": \"%s\",\n"
        "\"rgb\": [\n%d,\n%d,\n%d\n],\n"
        "\"varnishValue\": %d,\n\"whiteValue\": %d\n},\n",
        rgbEnabled != 0 ? "true" : "false",
        varnishEnabled != 0 ? "true" : "false", varnishMode,
        settings->varnishtoplayers,
        whiteEnabled != 0 ? "true" : "false", whiteMode,
        escapedProfileId, rgbEnabled != 0 ? "true" : "false",
        settings->supportenabled != 0 ? "true" : "false",
        settings->maxunexpectedoverlappixels,
        rgbEnabled != 0 ? "true" : "false",
        settings->supportenabled != 0 ? "true" : "false",
        varnishEnabled != 0 ? "true" : "false",
        whiteEnabled != 0 ? "true" : "false",
        varnishEnabled != 0 ? "true" : "false", varnishMode,
        settings->varnishtoplayers,
        whiteEnabled != 0 ? "true" : "false",
        settings->whiteexpandpx, whiteMode, settings->whiteshrinkpx,
        settings->allowinputsupportmaterial != 0 ? "true" : "false",
        defaultRole,
        settings->materialrolemappingenabled != 0 ? "true" : "false",
        rulesCanonical,
        (whiteEnabled != 0 || varnishEnabled != 0) ? "true" : "false",
        modelFillMaterial, materialChannel,
        red, green, blue, varnishValue, whiteValue);
    *compact = HostFormat(
        "\"materialPolicy\":{\"conflictPolicy\":"
        "\"model_material_over_support\",\"enabled\":true,"
        "\"rgb\":{\"enabled\":%s,\"source\":\"modelMaterial\"},"
        "\"varnish\":{\"enabled\":%s,\"mode\":\"%s\","
        "\"topLayers\":%d,\"value\":0},"
        "\"white\":{\"enabled\":%s,\"layers\":\"all_model\","
        "\"mode\":\"%s\",\"value\":0}},"
        "\"materialProcessProfile\":{\"enabled\":true,"
        "\"name\":\"%s\",\"rgb\":{\"enabled\":%s,"
        "\"source\":\"modelMaterial\"},\"support\":{"
        "\"expected\":%s,\"mode\":\"existing_support_pipeline\"},"
        "\"target\":\"host-reference\",\"validation\":{"
        "\"maxUnexpectedOverlapPixels\":%d,\"requireRgbPixels\":%s,"
        "\"requireSupportPixels\":%s,\"requireVarnishPixels\":%s,"
        "\"requireWhitePixels\":%s},\"varnish\":{"
        "\"coverage\":\"model_surface\",\"enabled\":%s,"
        "\"mode\":\"%s\",\"topLayers\":%d,\"value\":0},"
        "\"white\":{\"coverage\":\"all_model\",\"enabled\":%s,"
        "\"expandPx\":%d,\"mode\":\"%s\",\"shrinkPx\":%d,"
        "\"value\":0}},\"materialRoleMapping\":{"
        "\"allowInputSupportMaterial\":%s,\"defaultRole\":\"%s\","
        "\"enabled\":%s,\"mode\":\"rules_then_default\","
        "\"rules\":%s},\"modelFill\":{"
        "\"emptyAllowedInProduction\":false,\"enabled\":%s,"
        "\"legacyRgbFallback\":false,\"material\":\"%s\","
        "\"scope\":\"all_model\",\"value\":0},"
        "\"modelMaterial\":{\"applyMode\":\"solid_volume\","
        "\"materialChannel\":\"%s\",\"rgb\":[%d,%d,%d],"
        "\"varnishValue\":%d,\"whiteValue\":%d},",
        rgbEnabled != 0 ? "true" : "false",
        varnishEnabled != 0 ? "true" : "false", varnishMode,
        settings->varnishtoplayers,
        whiteEnabled != 0 ? "true" : "false", whiteMode,
        escapedProfileId, rgbEnabled != 0 ? "true" : "false",
        settings->supportenabled != 0 ? "true" : "false",
        settings->maxunexpectedoverlappixels,
        rgbEnabled != 0 ? "true" : "false",
        settings->supportenabled != 0 ? "true" : "false",
        varnishEnabled != 0 ? "true" : "false",
        whiteEnabled != 0 ? "true" : "false",
        varnishEnabled != 0 ? "true" : "false", varnishMode,
        settings->varnishtoplayers,
        whiteEnabled != 0 ? "true" : "false",
        settings->whiteexpandpx, whiteMode, settings->whiteshrinkpx,
        settings->allowinputsupportmaterial != 0 ? "true" : "false",
        defaultRole,
        settings->materialrolemappingenabled != 0 ? "true" : "false",
        rulesCompact,
        (whiteEnabled != 0 || varnishEnabled != 0) ? "true" : "false",
        modelFillMaterial, materialChannel,
        red, green, blue, varnishValue, whiteValue);

cleanup:
    free(rulesCanonical);
    free(rulesCompact);
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
