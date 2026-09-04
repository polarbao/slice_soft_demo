#include "HostVolumetricProfile.h"

#include "JsonText.h"

#include <stdlib.h>
#include <string.h>

/*
 * 规范化规则（须与 Worker 侧 Profile 文档哈希的规范化输出逐字节一致）：
 *   键序为字典序；每个键值对独占一行且冒号后有一个空格；
 *   空数组是 [] 而不是 [\n]；整数不带小数点。
 * 方括号一律由本文件的片段自身产出，空分支显式返回 "[]" —— 这是 H-F-04
 * 遗留缺陷的修复口径，不得回退为把方括号写在外层模板里。
 */

static int HasText(const char* text)
{
    return text != NULL && text[0] != '\0';
}

/*
 * MATOPQ：以下三个片段一律【条件产出】——取默认值时返回与修订前逐字节相同的内容，
 * 故既有预设的 profileHash 不变。键序仍须字典序，与规范化输出约定一致。
 * 数字用 %.15g 而非 %f：规范化输出取 defaultfloat + setprecision(15)
 * （json_value.cpp 的 dump_impl），%f 的尾零会让 Profile hash 不闭合。
 */
static const char* OverlapModeText(
    const struct hosteffectiveprofilesettings* settings)
{
    return settings->materialvolumeoverlapauto != 0
        ? "auto_by_material_name" : "explicit_priority";
}

static char* BuildOpacityVarnishCanonical(
    const struct hosteffectiveprofilesettings* settings)
{
    if (settings->materialvolumeopacityvarnishenabled == 0)
    {
        return HostFormat("");
    }
    return HostFormat(
        "\"opacityVarnish\": {\n"
        "\"enabled\": true,\n"
        "\"opacityMax\": %.15g,\n"
        "\"semiTransparentRole\": \"rgb\"\n},\n",
        settings->materialvolumeopacityvarnishmax);
}

static char* BuildOpacityVarnishCompact(
    const struct hosteffectiveprofilesettings* settings)
{
    if (settings->materialvolumeopacityvarnishenabled == 0)
    {
        return HostFormat("");
    }
    return HostFormat(
        "\"opacityVarnish\":{\"enabled\":true,\"opacityMax\":%.15g,"
        "\"semiTransparentRole\":\"rgb\"},",
        settings->materialvolumeopacityvarnishmax);
}

static char* BuildVolumeRulesCanonical(
    const struct hosteffectiveprofilesettings* settings)
{
    const int hasPrimary = HasText(settings->materialvolumeprimaryname);
    const int hasSecondary = HasText(settings->materialvolumesecondaryname);
    if (hasPrimary != 0 && hasSecondary != 0)
    {
        return HostFormat(
            "[\n{\n\"matchMaterialName\": \"%s\",\n"
            "\"priority\": %d\n},\n"
            "{\n\"matchMaterialName\": \"%s\",\n"
            "\"priority\": %d\n}\n]",
            settings->materialvolumeprimaryname,
            settings->materialvolumeprimarypriority,
            settings->materialvolumesecondaryname,
            settings->materialvolumesecondarypriority);
    }
    if (hasPrimary != 0)
    {
        return HostFormat(
            "[\n{\n\"matchMaterialName\": \"%s\",\n"
            "\"priority\": %d\n}\n]",
            settings->materialvolumeprimaryname,
            settings->materialvolumeprimarypriority);
    }
    return HostFormat("[]");
}

static char* BuildVolumeRulesCompact(
    const struct hosteffectiveprofilesettings* settings)
{
    const int hasPrimary = HasText(settings->materialvolumeprimaryname);
    const int hasSecondary = HasText(settings->materialvolumesecondaryname);
    if (hasPrimary != 0 && hasSecondary != 0)
    {
        return HostFormat(
            "[{\"matchMaterialName\":\"%s\",\"priority\":%d},"
            "{\"matchMaterialName\":\"%s\",\"priority\":%d}]",
            settings->materialvolumeprimaryname,
            settings->materialvolumeprimarypriority,
            settings->materialvolumesecondaryname,
            settings->materialvolumesecondarypriority);
    }
    if (hasPrimary != 0)
    {
        return HostFormat(
            "[{\"matchMaterialName\":\"%s\",\"priority\":%d}]",
            settings->materialvolumeprimaryname,
            settings->materialvolumeprimarypriority);
    }
    return HostFormat("[]");
}

int HostBuildVolumetricProfileFragments(
    const struct hosteffectiveprofilesettings* settings,
    const char* escapedProfileId,
    char** canonical,
    char** compact)
{
    char* rulesCanonical = NULL;
    char* rulesCompact = NULL;
    if (settings == NULL || escapedProfileId == NULL
        || canonical == NULL || compact == NULL)
    {
        return 0;
    }
    *canonical = NULL;
    *compact = NULL;
    if (settings->materialvolumeenabled == 0
        || settings->materialrolemappingenabled != 0)
    {
        return 0;
    }
    rulesCanonical = BuildVolumeRulesCanonical(settings);
    rulesCompact = BuildVolumeRulesCompact(settings);
    if (rulesCanonical == NULL || rulesCompact == NULL)
    {
        free(rulesCanonical);
        free(rulesCompact);
        return 0;
    }
    char* varnishCanonical = BuildOpacityVarnishCanonical(settings);
    char* varnishCompact = BuildOpacityVarnishCompact(settings);
    if (varnishCanonical == NULL || varnishCompact == NULL)
    {
        free(rulesCanonical);
        free(rulesCompact);
        free(varnishCanonical);
        free(varnishCompact);
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
        "\"source\": \"modelMaterial\"\n},\n"
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
        "\"materialVolumePolicy\": {\n"
        "\"enabled\": true,\n"
        "\"missingMaterial\": \"fail_closed\",\n"
        "\"mode\": \"closed_intervals\",\n"
        "%s"
        "\"openSurface\": {\n"
        "\"mode\": \"reject\",\n"
        "\"placement\": \"below_surface\",\n"
        "\"thicknessMm\": 0\n},\n"
        "\"overlap\": {\n"
        "\"mode\": \"%s\",\n"
        "\"rules\": %s\n},\n"
        "\"topology\": {\n"
        "\"maxBoundaryEdges\": 8,\n"
        "\"maxSelfIntersectionPairs\": 64,\n"
        "\"selfIntersectionPolicy\": \"tolerate_closed_self_intersection\"\n}\n},\n"
        "\"modelFill\": {\n\"emptyAllowedInProduction\": false,\n"
        "\"enabled\": true,\n\"legacyRgbFallback\": false,\n"
        "\"material\": \"rgb\",\n"
        "\"scope\": \"all_model\",\n\"value\": 0\n},\n"
        "\"modelMaterial\": {\n\"applyMode\": \"solid_volume\",\n"
        "\"materialChannel\": \"RGB\",\n"
        "\"rgb\": [\n0,\n0,\n0\n],\n"
        "\"varnishValue\": 255,\n\"whiteValue\": 255\n},\n",
        escapedProfileId,
        settings->supportenabled != 0 ? "true" : "false",
        settings->maxunexpectedoverlappixels,
        settings->supportenabled != 0 ? "true" : "false",
        settings->texturewhitevalue,
        varnishCanonical,
        OverlapModeText(settings),
        rulesCanonical);
    *compact = HostFormat(
        "\"materialPolicy\":{\"conflictPolicy\":"
        "\"model_material_over_support\",\"enabled\":false,"
        "\"rgb\":{\"enabled\":false,\"source\":\"modelMaterial\"},"
        "\"varnish\":{\"enabled\":false,\"mode\":\"disabled\","
        "\"topLayers\":1,\"value\":0},\"white\":{\"enabled\":false,"
        "\"layers\":\"all_model\",\"mode\":\"disabled\",\"value\":0}},"
        "\"materialProcessProfile\":{\"enabled\":true,\"name\":\"%s\","
        "\"rgb\":{\"enabled\":true,\"source\":\"modelMaterial\"},"
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
        "\"materialVolumePolicy\":{\"enabled\":true,"
        "\"missingMaterial\":\"fail_closed\","
        "\"mode\":\"closed_intervals\",%s\"openSurface\":{"
        "\"mode\":\"reject\",\"placement\":\"below_surface\","
        "\"thicknessMm\":0},\"overlap\":{"
        "\"mode\":\"%s\",\"rules\":%s},"
        "\"topology\":{\"maxBoundaryEdges\":8,"
        "\"maxSelfIntersectionPairs\":64,"
        "\"selfIntersectionPolicy\":\"tolerate_closed_self_intersection\"}},"
        "\"modelFill\":{\"emptyAllowedInProduction\":false,"
        "\"enabled\":true,\"legacyRgbFallback\":false,"
        "\"material\":\"rgb\",\"scope\":\"all_model\","
        "\"value\":0},\"modelMaterial\":{"
        "\"applyMode\":\"solid_volume\",\"materialChannel\":\"RGB\","
        "\"rgb\":[0,0,0],\"varnishValue\":255,\"whiteValue\":255},",
        escapedProfileId,
        settings->supportenabled != 0 ? "true" : "false",
        settings->maxunexpectedoverlappixels,
        settings->supportenabled != 0 ? "true" : "false",
        settings->texturewhitevalue,
        varnishCompact,
        OverlapModeText(settings),
        rulesCompact);
    free(rulesCanonical);
    free(rulesCompact);
    free(varnishCanonical);
    free(varnishCompact);
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
