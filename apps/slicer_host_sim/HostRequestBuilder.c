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

static char* BuildTransform(double translateXMm, double translateYMm)
{
    return HostFormat(
        "{\"translateXMm\":%.17g,\"translateYMm\":%.17g,"
        "\"rotateZDeg\":0,\"uniformScale\":1,"
        "\"mirrorX\":false,\"mirrorY\":false}",
        translateXMm,
        translateYMm);
}

static char* BuildBounds(const HostBounds3* bounds, double x, double y)
{
    return HostFormat(
        "{\"min\":{\"x\":%.17g,\"y\":%.17g,\"z\":%.17g},"
        "\"max\":{\"x\":%.17g,\"y\":%.17g,\"z\":%.17g}}",
        bounds->min[0] + x,
        bounds->min[1] + y,
        bounds->min[2],
        bounds->max[0] + x,
        bounds->max[1] + y,
        bounds->max[2]);
}

int HostComputeUntexturedObjResourceDigest(
    const char* modelPath,
    char* resourceDigest,
    unsigned long resourceDigestCapacity)
{
    char* payload;
    int success;
    if (modelPath == NULL || resourceDigest == NULL
        || resourceDigestCapacity < 65U)
    {
        return 0;
    }
    payload = HostFormat("%s|obj|0|0,0", modelPath);
    if (payload == NULL)
    {
        return 0;
    }
    success = ComputeSha256(payload, resourceDigest);
    free(payload);
    return success;
}

char* HostBuildScene(
    const char* modelPath,
    const char* resourceRoot,
    const char* sourceDigest,
    const char* resourceDigest,
    const HostBounds3* sourceBoundsInput,
    const HostBounds3* effectiveBoundsInput,
    unsigned long long sceneRevision,
    double translateXMm,
    double translateYMm,
    unsigned long long transformRevision)
{
    char* escapedModel = NULL;
    char* escapedRoot = NULL;
    char* escapedSourceDigest = NULL;
    char* escapedResourceDigest = NULL;
    char* requested = NULL;
    char* derived = NULL;
    char* effective = NULL;
    char* sourceBoundsJson = NULL;
    char* effectiveBoundsJson = NULL;
    char* scene = NULL;
    if (modelPath == NULL || resourceRoot == NULL
        || sourceDigest == NULL || resourceDigest == NULL
        || sourceBoundsInput == NULL || effectiveBoundsInput == NULL)
    {
        return NULL;
    }
    escapedModel = HostJsonEscape(modelPath);
    escapedRoot = HostJsonEscape(resourceRoot);
    escapedSourceDigest = HostJsonEscape(sourceDigest);
    escapedResourceDigest = HostJsonEscape(resourceDigest);
    requested = BuildTransform(translateXMm, translateYMm);
    derived = BuildTransform(0.0, 0.0);
    effective = BuildTransform(translateXMm, translateYMm);
    sourceBoundsJson = BuildBounds(sourceBoundsInput, 0.0, 0.0);
    effectiveBoundsJson = BuildBounds(effectiveBoundsInput, 0.0, 0.0);
    if (escapedModel == NULL || escapedRoot == NULL
        || escapedSourceDigest == NULL || escapedResourceDigest == NULL
        || requested == NULL || derived == NULL || effective == NULL
        || sourceBoundsJson == NULL || effectiveBoundsJson == NULL)
    {
        goto cleanup;
    }
    scene = HostFormat(
        "{\"schema\":\"slicesoft.multimodel_scene.13b.1\","
        "\"subjectType\":\"scene\",\"sceneId\":\"scene-stage14e01\","
        "\"sceneRevision\":%llu,"
        "\"buildVolume\":{\"source\":\"device_profile\","
        "\"widthMm\":230,\"heightMm\":100,\"zLimitMm\":60,"
        "\"origin\":\"center\",\"xDirection\":\"positive\","
        "\"yDirection\":\"positive\",\"isFixture\":false},"
        "\"layout\":{\"policy\":\"grid\",\"maxColumns\":11,"
        "\"maxRows\":2,\"columnGapMm\":10,\"rowGapMm\":10,"
        "\"spacingMode\":\"edge_clearance\",\"order\":\"row_major\"},"
        "\"materialBindingMode\":\"scene_profile_only\","
        "\"resolvedProfileId\":\"profile-stage14e01\","
        "\"resourceScopes\":[{\"resourceScopeId\":\"scope-stage14e01\","
        "\"kind\":\"obj_directory\",\"rootPath\":\"%s\","
        "\"packagePath\":\"\",\"partIdentity\":\"\"}],"
        "\"models\":[{\"modelId\":\"model-stage14e01\","
        "\"sourcePath\":\"%s\",\"format\":\"obj\","
        "\"resourceScopeId\":\"scope-stage14e01\","
        "\"sourceHash\":\"%s\",\"resourceHash\":\"%s\","
        "\"displayName\":\"Stage 14E-01 cube\"}],"
        "\"instances\":[{\"instanceId\":\"instance-stage14e01\","
        "\"modelId\":\"model-stage14e01\","
        "\"sourceTransformIdentity\":\"%s\","
        "\"requestedTransform\":%s,\"derivedLayoutTransform\":%s,"
        "\"effectiveTransform\":%s,\"visible\":true,\"locked\":false,"
        "\"transformRevision\":%llu,\"sourceBboxMm\":%s,"
        "\"effectiveBboxMm\":%s,\"admissionStatus\":\"admitted\","
        "\"resolvedProfileId\":\"profile-stage14e01\"}]}",
        sceneRevision,
        escapedRoot,
        escapedModel,
        escapedSourceDigest,
        escapedResourceDigest,
        escapedModel,
        requested,
        derived,
        effective,
        transformRevision,
        sourceBoundsJson,
        effectiveBoundsJson);

cleanup:
    free(escapedModel);
    free(escapedRoot);
    free(escapedSourceDigest);
    free(escapedResourceDigest);
    free(requested);
    free(derived);
    free(effective);
    free(sourceBoundsJson);
    free(effectiveBoundsJson);
    return scene;
}

char* HostBuildProfile(
    const char* modelPath,
    const char* packageDirectory,
    char* profileHash,
    unsigned long profileHashCapacity)
{
    char* escapedModel = NULL;
    char* escapedPackage = NULL;
    char* canonical = NULL;
    char* profile = NULL;
    char digest[65];
    if (modelPath == NULL || packageDirectory == NULL
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
        "\"layerThicknessMm\": 0.2,\n"
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
        "\"dpiX\":127,\"dpiY\":127,\"layerThicknessMm\":0.2,"
        "\"packageDir\":\"%s\",\"planarConfig\":\"contiguous\","
        "\"rowsPerStrip\":64,\"storageMode\":\"stripped\"},"
        "\"preview\":{\"enabled\":false},\"profileHash\":\"%s\","
        "\"profileVersion\":\"1.0\","
        "\"slicePipeline\":{\"mode\":\"legacy\"},"
        "\"slicingMode\":\"closed_mesh_scanline\"}",
        escapedModel,
        escapedPackage,
        profileHash);

cleanup:
    free(escapedModel);
    free(escapedPackage);
    free(canonical);
    return profile;
}
