#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <bcrypt.h>

#include "HostRequestBuilder.h"
#include "JsonText.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ComputeSha256(const char* input, char output[65])
{
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    DWORD objectBytes = 0U;
    DWORD hashBytes = 0U;
    DWORD copied = 0U;
    unsigned char* object = NULL;
    unsigned char digest[32];
    size_t index;
    int success = 0;
    if (input == NULL
        || BCryptOpenAlgorithmProvider(
               &algorithm,
               BCRYPT_SHA256_ALGORITHM,
               NULL,
               0U) < 0)
    {
        return 0;
    }
    if (BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            (PUCHAR)&objectBytes,
            sizeof(objectBytes),
            &copied,
            0U) < 0
        || BCryptGetProperty(
               algorithm,
               BCRYPT_HASH_LENGTH,
               (PUCHAR)&hashBytes,
               sizeof(hashBytes),
               &copied,
               0U) < 0
        || hashBytes != sizeof(digest))
    {
        goto cleanup;
    }
    object = (unsigned char*)malloc(objectBytes);
    if (object == NULL
        || BCryptCreateHash(
               algorithm,
               &hash,
               object,
               objectBytes,
               NULL,
               0U,
               0U) < 0
        || BCryptHashData(
               hash,
               (PUCHAR)input,
               (ULONG)strlen(input),
               0U) < 0
        || BCryptFinishHash(hash, digest, sizeof(digest), 0U) < 0)
    {
        goto cleanup;
    }
    for (index = 0U; index < sizeof(digest); ++index)
    {
        (void)snprintf(
            output + index * 2U,
            3U,
            "%02x",
            (unsigned int)digest[index]);
    }
    output[64] = '\0';
    success = 1;

cleanup:
    if (hash != NULL)
    {
        BCryptDestroyHash(hash);
    }
    free(object);
    BCryptCloseAlgorithmProvider(algorithm, 0U);
    return success;
}

char* HostBuildProfileWithLayerThickness(
    const char* modelPath,
    const char* packageDirectory,
    double layerThicknessMm,
    char* profileHash,
    unsigned long profileHashCapacity)
{
    char* escapedModel = NULL;
    char* escapedPackage = NULL;
    char* canonical = NULL;
    char* profile = NULL;
    char digest[65];
    if (modelPath == NULL || packageDirectory == NULL
        || layerThicknessMm <= 0.0
        || profileHash == NULL || profileHashCapacity < 72U)
    {
        return NULL;
    }
    escapedModel = HostJsonEscape(modelPath);
    escapedPackage = HostJsonEscape(packageDirectory);
    if (escapedModel == NULL || escapedPackage == NULL)
    {
        goto cleanup;
    }
    canonical = HostFormat(
        "{\n"
        "\"autoOrient\": {\n"
        "\"enabled\": true,\n"
        "\"maxHeightMm\": 9\n"
        "},\n"
        "\"background\": {\n"
        "\"value\": 255\n"
        "},\n"
        "\"input\": {\n"
        "\"format\": \"obj\",\n"
        "\"modelPath\": \"%s\"\n"
        "},\n"
        "\"materialProcessProfile\": {\n"
        "\"enabled\": true,\n"
        "\"name\": \"profile-stage14e01\",\n"
        "\"target\": \"stage14e01-fixture\"\n"
        "},\n"
        "\"modelMaterial\": {\n"
        "\"applyMode\": \"solid_volume\",\n"
        "\"materialChannel\": \"RGB\",\n"
        "\"rgb\": [\n"
        "0,\n"
        "0,\n"
        "0\n"
        "],\n"
        "\"varnishValue\": 255,\n"
        "\"whiteValue\": 255\n"
        "},\n"
        "\"output\": {\n"
        "\"bitDepth\": 8,\n"
        "\"channelOrder\": [\n"
        "\"R\",\n"
        "\"G\",\n"
        "\"B\",\n"
        "\"W\",\n"
        "\"S\",\n"
        "\"V\"\n"
        "],\n"
        "\"dpiX\": 127,\n"
        "\"dpiY\": 127,\n"
        "\"layerThicknessMm\": %.15g,\n"
        "\"packageDir\": \"%s\",\n"
        "\"planarConfig\": \"contiguous\",\n"
        "\"rowsPerStrip\": 64,\n"
        "\"storageMode\": \"stripped\"\n"
        "},\n"
        "\"preview\": {\n"
        "\"enabled\": false\n"
        "},\n"
        "\"profileVersion\": \"1.0\",\n"
        "\"slicePipeline\": {\n"
        "\"mode\": \"legacy\"\n"
        "},\n"
        "\"slicingMode\": \"closed_mesh_scanline\"\n"
        "}",
        escapedModel,
        layerThicknessMm,
        escapedPackage);
    if (canonical == NULL || !ComputeSha256(canonical, digest))
    {
        goto cleanup;
    }
    (void)snprintf(
        profileHash,
        profileHashCapacity,
        "sha256:%s",
        digest);
    profile = HostFormat(
        "{\"autoOrient\":{\"enabled\":true,\"maxHeightMm\":9},"
        "\"background\":{\"value\":255},"
        "\"input\":{\"format\":\"obj\",\"modelPath\":\"%s\"},"
        "\"materialProcessProfile\":{\"enabled\":true,"
        "\"name\":\"profile-stage14e01\",\"target\":\"stage14e01-fixture\"},"
        "\"modelMaterial\":{\"applyMode\":\"solid_volume\","
        "\"materialChannel\":\"RGB\",\"rgb\":[0,0,0],"
        "\"varnishValue\":255,\"whiteValue\":255},"
        "\"output\":{\"bitDepth\":8,"
        "\"channelOrder\":[\"R\",\"G\",\"B\",\"W\",\"S\",\"V\"],"
        "\"dpiX\":127,\"dpiY\":127,\"layerThicknessMm\":%.15g,"
        "\"packageDir\":\"%s\",\"planarConfig\":\"contiguous\","
        "\"rowsPerStrip\":64,\"storageMode\":\"stripped\"},"
        "\"preview\":{\"enabled\":false},\"profileHash\":\"%s\","
        "\"profileVersion\":\"1.0\","
        "\"slicePipeline\":{\"mode\":\"legacy\"},"
        "\"slicingMode\":\"closed_mesh_scanline\"}",
        escapedModel,
        layerThicknessMm,
        escapedPackage,
        profileHash);

cleanup:
    free(escapedModel);
    free(escapedPackage);
    free(canonical);
    return profile;
}

char* HostBuildProfile(
    const char* modelPath,
    const char* packageDirectory,
    char* profileHash,
    unsigned long profileHashCapacity)
{
    return HostBuildProfileWithLayerThickness(
        modelPath,
        packageDirectory,
        0.2,
        profileHash,
        profileHashCapacity);
}

char* HostBuildEffectiveProfile(
    const struct hosteffectiveprofilesettings* settings,
    char* profileHash,
    unsigned long profileHashCapacity)
{
    const char* materialChannel = NULL;
    int red = 255;
    int green = 255;
    int blue = 255;
    int whiteValue = 255;
    int varnishValue = 255;
    char* escapedModel = NULL;
    char* escapedFormat = NULL;
    char* escapedPackage = NULL;
    char* escapedProfile = NULL;
    char* canonical = NULL;
    char* profile = NULL;
    char digest[65];
    if (settings == NULL || settings->modelpath == NULL
        || settings->modelformat == NULL
        || settings->packagedirectory == NULL
        || settings->profileid == NULL
        || settings->modelpath[0] == '\0'
        || settings->packagedirectory[0] == '\0'
        || settings->profileid[0] == '\0'
        || (strcmp(settings->modelformat, "obj") != 0
            && strcmp(settings->modelformat, "3mf") != 0
            && strcmp(settings->modelformat, "stl") != 0)
        || settings->dpix < 72 || settings->dpix > 2400
        || settings->dpiy < 72 || settings->dpiy > 2400
        || settings->layerthicknessmm <= 0.0
        || settings->layerthicknessmm > 10.0
        || profileHash == NULL || profileHashCapacity < 72U)
    {
        return NULL;
    }

    switch (settings->materialstrategy)
    {
    case HOST_MATERIAL_RGB_SOLID:
        materialChannel = "RGB";
        red = 0;
        green = 0;
        blue = 0;
        break;
    case HOST_MATERIAL_WHITE_SOLID:
        materialChannel = "W";
        whiteValue = 0;
        break;
    case HOST_MATERIAL_VARNISH_SOLID:
        materialChannel = "V";
        varnishValue = 0;
        break;
    default:
        return NULL;
    }

    escapedModel = HostJsonEscape(settings->modelpath);
    escapedFormat = HostJsonEscape(settings->modelformat);
    escapedPackage = HostJsonEscape(settings->packagedirectory);
    escapedProfile = HostJsonEscape(settings->profileid);
    if (escapedModel == NULL || escapedFormat == NULL
        || escapedPackage == NULL || escapedProfile == NULL)
    {
        goto cleanup;
    }

    canonical = HostFormat(
        "{\n"
        "\"autoOrient\": {\n"
        "\"enabled\": true,\n"
        "\"maxHeightMm\": 9\n"
        "},\n"
        "\"background\": {\n"
        "\"value\": 255\n"
        "},\n"
        "\"input\": {\n"
        "\"format\": \"%s\",\n"
        "\"modelPath\": \"%s\"\n"
        "},\n"
        "\"materialProcessProfile\": {\n"
        "\"enabled\": true,\n"
        "\"name\": \"%s\",\n"
        "\"target\": \"host-reference\"\n"
        "},\n"
        "\"modelMaterial\": {\n"
        "\"applyMode\": \"solid_volume\",\n"
        "\"materialChannel\": \"%s\",\n"
        "\"rgb\": [\n"
        "%d,\n"
        "%d,\n"
        "%d\n"
        "],\n"
        "\"varnishValue\": %d,\n"
        "\"whiteValue\": %d\n"
        "},\n"
        "\"output\": {\n"
        "\"bitDepth\": 8,\n"
        "\"channelOrder\": [\n"
        "\"R\",\n"
        "\"G\",\n"
        "\"B\",\n"
        "\"W\",\n"
        "\"S\",\n"
        "\"V\"\n"
        "],\n"
        "\"dpiX\": %d,\n"
        "\"dpiY\": %d,\n"
        "\"layerThicknessMm\": %.15g,\n"
        "\"packageDir\": \"%s\",\n"
        "\"planarConfig\": \"contiguous\",\n"
        "\"rowsPerStrip\": 64,\n"
        "\"storageMode\": \"stripped\"\n"
        "},\n"
        "\"preview\": {\n"
        "\"enabled\": false\n"
        "},\n"
        "\"profileVersion\": \"1.0\",\n"
        "\"slicePipeline\": {\n"
        "\"mode\": \"legacy\"\n"
        "},\n"
        "\"slicingMode\": \"closed_mesh_scanline\"\n"
        "}",
        escapedFormat,
        escapedModel,
        escapedProfile,
        materialChannel,
        red,
        green,
        blue,
        varnishValue,
        whiteValue,
        settings->dpix,
        settings->dpiy,
        settings->layerthicknessmm,
        escapedPackage);
    if (canonical == NULL || !ComputeSha256(canonical, digest))
    {
        goto cleanup;
    }
    (void)snprintf(
        profileHash,
        profileHashCapacity,
        "sha256:%s",
        digest);
    profile = HostFormat(
        "{\"autoOrient\":{\"enabled\":true,\"maxHeightMm\":9},"
        "\"background\":{\"value\":255},"
        "\"input\":{\"format\":\"%s\",\"modelPath\":\"%s\"},"
        "\"materialProcessProfile\":{\"enabled\":true,"
        "\"name\":\"%s\",\"target\":\"host-reference\"},"
        "\"modelMaterial\":{\"applyMode\":\"solid_volume\","
        "\"materialChannel\":\"%s\",\"rgb\":[%d,%d,%d],"
        "\"varnishValue\":%d,\"whiteValue\":%d},"
        "\"output\":{\"bitDepth\":8,"
        "\"channelOrder\":[\"R\",\"G\",\"B\",\"W\",\"S\",\"V\"],"
        "\"dpiX\":%d,\"dpiY\":%d,\"layerThicknessMm\":%.15g,"
        "\"packageDir\":\"%s\",\"planarConfig\":\"contiguous\","
        "\"rowsPerStrip\":64,\"storageMode\":\"stripped\"},"
        "\"preview\":{\"enabled\":false},\"profileHash\":\"%s\","
        "\"profileVersion\":\"1.0\","
        "\"slicePipeline\":{\"mode\":\"legacy\"},"
        "\"slicingMode\":\"closed_mesh_scanline\"}",
        escapedFormat,
        escapedModel,
        escapedProfile,
        materialChannel,
        red,
        green,
        blue,
        varnishValue,
        whiteValue,
        settings->dpix,
        settings->dpiy,
        settings->layerthicknessmm,
        escapedPackage,
        profileHash);

cleanup:
    free(escapedModel);
    free(escapedFormat);
    free(escapedPackage);
    free(escapedProfile);
    free(canonical);
    return profile;
}
