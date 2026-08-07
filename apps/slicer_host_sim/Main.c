#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "HostModuleApi.h"
#include "HostPath.h"
#include "HostRequestBuilder.h"
#include "JsonText.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int JsonSucceeded(const char* json)
{
    int ok = 0;
    return HostJsonReadBoolean(json, "ok", &ok) && ok != 0;
}

static void PrintLastError(const HostModuleApi* api, const char* context)
{
    char* error = NULL;
    if (HostModuleApiReadLastError(api, &error))
    {
        fprintf(stderr, "[14E-01] %s: %s\n", context, error);
    }
    else
    {
        fprintf(stderr, "[14E-01] %s: no module error available\n", context);
    }
    free(error);
}

static int RunJob(
    const HostModuleApi* api,
    pm_module_t* module,
    const char* label,
    const char* request,
    DWORD timeoutMs,
    char** result)
{
    pm_job_t* job;
    ULONGLONG deadline;
    char* terminalState = NULL;
    unsigned long long lastPercent = 101U;
    if (result == NULL)
    {
        return 0;
    }
    *result = NULL;
    job = api->m_submit(module, request);
    if (job == NULL)
    {
        PrintLastError(api, label);
        return 0;
    }
    deadline = GetTickCount64() + timeoutMs;
    for (;;)
    {
        char* progress = NULL;
        char* state = NULL;
        unsigned long long percent = 0U;
        if (!HostModuleApiReadPoll(api, job, &progress)
            || !HostJsonReadString(progress, "state", &state))
        {
            free(progress);
            api->m_release(job);
            return 0;
        }
        if (HostJsonReadUnsigned(progress, "percent", &percent)
            && percent != lastPercent)
        {
            printf("[14E-01] %s %llu%%\n", label, percent);
            lastPercent = percent;
        }
        free(progress);
        if (strcmp(state, "succeeded") == 0
            || strcmp(state, "failed") == 0
            || strcmp(state, "cancelled") == 0)
        {
            terminalState = state;
            break;
        }
        free(state);
        if (GetTickCount64() >= deadline)
        {
            (void)api->m_cancel(job);
            api->m_release(job);
            fprintf(stderr, "[14E-01] %s timed out\n", label);
            return 0;
        }
        Sleep(20U);
    }
    if (!HostModuleApiReadResult(api, job, result))
    {
        free(terminalState);
        api->m_release(job);
        return 0;
    }
    api->m_release(job);
    if (strcmp(terminalState, "succeeded") != 0 || !JsonSucceeded(*result))
    {
        fprintf(stderr, "[14E-01] %s failed: %s\n", label, *result);
        free(terminalState);
        return 0;
    }
    free(terminalState);
    return 1;
}

static int CheckModuleInfo(const HostModuleApi* api)
{
    static const char* capabilities[] = {
        "model.import", "model.get_metadata", "model.release",
        "scene.apply_operation", "scene.get_snapshot", "scene.get_viewdata",
        "geometry.preflight", "geometry.collision", "geometry.repair",
        "slice.rgbwsv", "package.verify", "package.get_summary",
        "package.get_layer_descriptor", "package.render_layer_preview",
        "package.read_report"};
    char* info = NULL;
    size_t index;
    if (api->m_spiVersion() != PM_SPI_VERSION
        || !HostModuleApiReadInfo(api, &info))
    {
        free(info);
        return 0;
    }
    for (index = 0U; index < sizeof(capabilities) / sizeof(capabilities[0]); ++index)
    {
        if (strstr(info, capabilities[index]) == NULL)
        {
            free(info);
            return 0;
        }
    }
    free(info);
    return 1;
}

static int CheckFailClosed(const HostModuleApi* api, pm_module_t* module)
{
    pm_job_t* job = api->m_submit(
        module,
        "{\"capability\":\"host.invalid_capability\"}");
    char* error = NULL;
    int passed;
    if (job != NULL)
    {
        api->m_release(job);
        return 0;
    }
    passed = HostModuleApiReadLastError(api, &error)
        && strstr(error, "PM-SLICER-") != NULL;
    if (passed)
    {
        printf("[14E-01] fail-closed PASS: %s\n", error);
    }
    free(error);
    return passed;
}

static int ReadImportedModel(
    const char* result,
    char** modelId,
    char** sourceDigest,
    HostBounds3* bounds)
{
    char* bbox = NULL;
    int success = HostJsonReadString(result, "modelId", modelId)
        && HostJsonReadString(result, "sourceDigest", sourceDigest)
        && HostJsonReadObject(result, "bboxMm", &bbox)
        && HostJsonReadNumber3(bbox, "min", bounds->min)
        && HostJsonReadNumber3(bbox, "max", bounds->max);
    free(bbox);
    return success;
}

static int ReadBounds(
    const char* json,
    const char* key,
    HostBounds3* bounds)
{
    char* bbox = NULL;
    int success = HostJsonReadObject(json, key, &bbox)
        && HostJsonReadNumber3(bbox, "min", bounds->min)
        && HostJsonReadNumber3(bbox, "max", bounds->max);
    free(bbox);
    return success;
}

static int RunReferenceFlow(
    const HostModuleApi* api,
    pm_module_t* module,
    const char* repository,
    const char* outputRoot)
{
    char* fixture = HostFormat(
        "%s/samples/models/openvdb/surface_shell_cube_no_uv.obj",
        repository);
    char* resourceRoot = HostFormat(
        "%s/samples/models/openvdb",
        repository);
    char* packageDirectory = HostFormat("%s/stage14e01_package", outputRoot);
    char* escapedFixture = NULL;
    char* importRequest = NULL;
    char* importResult = NULL;
    char* modelId = NULL;
    char* sourceDigest = NULL;
    char resourceDigest[65];
    HostBounds3 bounds;
    HostBounds3 committedBounds;
    char* initialScene = NULL;
    char* transformRequest = NULL;
    char* transformResult = NULL;
    char* sceneHash = NULL;
    char* committedScene = NULL;
    char profileHash[72];
    char* profile = NULL;
    char* sliceRequest = NULL;
    char* sliceResult = NULL;
    char* escapedPackage = NULL;
    char* verifyRequest = NULL;
    char* verifyResult = NULL;
    char* releaseRequest = NULL;
    char* releaseResult = NULL;
    unsigned long long revision = 0U;
    unsigned long long layerCount = 0U;
    int valid = 0;
    int success = 0;
    const unsigned long processId = GetCurrentProcessId();
    const unsigned long long nonce = GetTickCount64();
    if (fixture == NULL || resourceRoot == NULL || packageDirectory == NULL)
    {
        goto cleanup;
    }
    escapedFixture = HostJsonEscape(fixture);
    importRequest = HostFormat(
        "{\"capability\":\"model.import\",\"modelPath\":\"%s\","
        "\"options\":{\"computeBBox\":true,\"extractMaterials\":false}}",
        escapedFixture);
    if (importRequest == NULL
        || !RunJob(api, module, "model.import", importRequest, 30000U, &importResult)
        || !ReadImportedModel(
               importResult,
               &modelId,
               &sourceDigest,
               &bounds))
    {
        goto cleanup;
    }
    if (!HostComputeUntexturedObjResourceDigest(
            fixture,
            resourceDigest,
            sizeof(resourceDigest)))
    {
        goto cleanup;
    }

    initialScene = HostBuildScene(
        fixture,
        resourceRoot,
        sourceDigest,
        resourceDigest,
        &bounds,
        &bounds,
        0U,
        0.0,
        0.0,
        0U);
    if (initialScene == NULL)
    {
        goto cleanup;
    }
    transformRequest = HostFormat(
        "{\"capability\":\"scene.apply_operation\","
        "\"operationId\":\"operation-stage14e01-%lu-%llu\","
        "\"scene\":%s,\"currentSceneRevision\":0,"
        "\"expectedSceneRevision\":0,\"operations\":[{"
        "\"type\":\"translate\",\"instanceId\":\"instance-stage14e01\","
        "\"deltaMm\":[1,2,0]}]}",
        processId,
        nonce,
        initialScene);
    if (transformRequest == NULL
        || !RunJob(
               api,
               module,
               "scene.apply_operation",
               transformRequest,
               30000U,
               &transformResult)
        || !HostJsonReadUnsigned(transformResult, "newSceneRevision", &revision)
        || revision != 1U
        || !HostJsonReadString(transformResult, "sceneHash", &sceneHash)
        || !ReadBounds(transformResult, "effectiveBBoxMm", &committedBounds))
    {
        goto cleanup;
    }
    printf(
        "[14E-01] committed bounds: [%.17g, %.17g, %.17g] "
        "to [%.17g, %.17g, %.17g]\n",
        committedBounds.min[0],
        committedBounds.min[1],
        committedBounds.min[2],
        committedBounds.max[0],
        committedBounds.max[1],
        committedBounds.max[2]);
    committedScene = HostBuildScene(
        fixture,
        resourceRoot,
        sourceDigest,
        resourceDigest,
        &bounds,
        &committedBounds,
        revision,
        1.0,
        2.0,
        1U);
    profile = HostBuildProfile(
        fixture,
        packageDirectory,
        profileHash,
        sizeof(profileHash));
    escapedPackage = HostJsonEscape(packageDirectory);
    if (committedScene == NULL || profile == NULL || escapedPackage == NULL)
    {
        goto cleanup;
    }
    sliceRequest = HostFormat(
        "{\"capability\":\"slice.rgbwsv\","
        "\"jobId\":\"job-stage14e01-%lu-%llu\","
        "\"correlationId\":\"correlation-stage14e01-%lu-%llu\","
        "\"sceneHash\":\"%s%s\",\"scene\":%s,\"profile\":%s,"
        "\"output\":{\"contract\":\"p0.rgbwsv.2\","
        "\"packageDir\":\"%s\"},\"options\":{\"backend\":\"worker\"}}",
        processId,
        nonce,
        processId,
        nonce,
        strncmp(sceneHash, "sha256:", 7U) == 0 ? "" : "sha256:",
        sceneHash,
        committedScene,
        profile,
        escapedPackage);
    if (sliceRequest == NULL
        || !RunJob(api, module, "slice.rgbwsv", sliceRequest, 90000U, &sliceResult)
        || !HostJsonReadUnsigned(sliceResult, "layerCount", &layerCount)
        || layerCount == 0U
        || strstr(sliceResult, "manifestPath") == NULL)
    {
        goto cleanup;
    }

    verifyRequest = HostFormat(
        "{\"capability\":\"package.verify\",\"packageDir\":\"%s\"}",
        escapedPackage);
    if (verifyRequest == NULL
        || !RunJob(api, module, "package.verify", verifyRequest, 30000U, &verifyResult)
        || !HostJsonReadBoolean(verifyResult, "valid", &valid)
        || valid == 0)
    {
        goto cleanup;
    }
    releaseRequest = HostFormat(
        "{\"capability\":\"model.release\",\"modelId\":\"%s\"}",
        modelId);
    if (releaseRequest == NULL
        || !RunJob(api, module, "model.release", releaseRequest, 30000U, &releaseResult))
    {
        goto cleanup;
    }
    printf(
        "[14E-01] M-MVP PASS: import -> transform -> slice (%llu layers) "
        "-> package -> verify\n",
        layerCount);
    success = 1;

cleanup:
    free(fixture);
    free(resourceRoot);
    free(packageDirectory);
    free(escapedFixture);
    free(importRequest);
    free(importResult);
    free(modelId);
    free(sourceDigest);
    free(initialScene);
    free(transformRequest);
    free(transformResult);
    free(sceneHash);
    free(committedScene);
    free(profile);
    free(sliceRequest);
    free(sliceResult);
    free(escapedPackage);
    free(verifyRequest);
    free(verifyResult);
    free(releaseRequest);
    free(releaseResult);
    return success;
}

int wmain(int argumentCount, wchar_t* arguments[])
{
    HostModuleApi api;
    char loadError[256];
    pm_module_t* module = NULL;
    char* repository = NULL;
    char* outputRoot = NULL;
    char* selfTest = NULL;
    int result = 1;
    memset(&api, 0, sizeof(api));
    loadError[0] = '\0';
    if (argumentCount != 4)
    {
        fwprintf(
            stderr,
            L"usage: slicer_host_sim <slicer_module.dll> <repository> <output-root>\n");
        return 2;
    }
    if (!HostEnsureDirectoryTree(arguments[3]))
    {
        fprintf(stderr, "[14E-01] failed to create output root\n");
        goto cleanup;
    }
    repository = HostUtf8FromWide(arguments[2]);
    outputRoot = HostUtf8FromWide(arguments[3]);
    if (repository == NULL || outputRoot == NULL)
    {
        fprintf(stderr, "[14E-01] UTF-8 path conversion failed\n");
        goto cleanup;
    }
    if (!HostModuleApiLoad(
               &api,
               arguments[1],
               loadError,
               sizeof(loadError)))
    {
        fprintf(stderr, "[14E-01] load failed: %s\n", loadError);
        goto cleanup;
    }
    if (!CheckModuleInfo(&api))
    {
        fprintf(stderr, "[14E-01] module metadata contract rejected\n");
        goto cleanup;
    }
    module = api.m_create(NULL);
    if (module == NULL)
    {
        PrintLastError(&api, "pm_create");
        goto cleanup;
    }
    if (!HostModuleApiReadSelfTest(&api, module, &selfTest)
        || !JsonSucceeded(selfTest)
        || !CheckFailClosed(&api, module)
        || !RunReferenceFlow(&api, module, repository, outputRoot))
    {
        goto cleanup;
    }
    result = 0;

cleanup:
    free(selfTest);
    if (module != NULL)
    {
        api.m_destroy(module);
    }
    HostModuleApiUnload(&api);
    free(repository);
    free(outputRoot);
    return result;
}
