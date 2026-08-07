#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "HostFlowEndToEnd.h"

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
        fprintf(stderr, "[H-A-03] %s: %s\n", context, error);
    }
    else
    {
        fprintf(stderr, "[H-A-03] %s: no module error available\n", context);
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
            printf("[H-A-03] %s %llu%%\n", label, percent);
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
            fprintf(stderr, "[H-A-03] %s timed out\n", label);
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
        fprintf(stderr, "[H-A-03] %s failed: %s\n", label, *result);
        free(terminalState);
        return 0;
    }
    free(terminalState);
    return 1;
}

int HostFlowRunEndToEnd(
    const HostModuleApi* api,
    pm_module_t* module,
    const char* repository,
    const char* outputRoot)
{
    const unsigned long processId = GetCurrentProcessId();
    const unsigned long long nonce = GetTickCount64();
    char profileHash[72];
    char* fixture = HostFormat(
        "%s/samples/models/openvdb/surface_shell_cube_no_uv.obj",
        repository);
    char* packageDirectory = HostFormat(
        "%s/package_%lu_%llu",
        outputRoot,
        processId,
        nonce);
    char* escapedFixture = NULL;
    char* importRequest = NULL;
    char* importResult = NULL;
    char* modelId = NULL;
    char* addRequest = NULL;
    char* addResult = NULL;
    char* layoutRequest = NULL;
    char* layoutResult = NULL;
    char* transformRequest = NULL;
    char* transformResult = NULL;
    char* snapshotRequest = NULL;
    char* snapshotResult = NULL;
    char* committedScene = NULL;
    char* sceneHash = NULL;
    char* profile = NULL;
    char* escapedPackage = NULL;
    char* sliceRequest = NULL;
    char* sliceResult = NULL;
    char* verifyRequest = NULL;
    char* verifyResult = NULL;
    char* releaseRequest = NULL;
    char* releaseResult = NULL;
    unsigned long long sceneHandle = 0U;
    unsigned long long revision = 0U;
    unsigned long long layerCount = 0U;
    int valid = 0;
    int success = 0;

    if (fixture == NULL || packageDirectory == NULL)
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
        || !HostJsonReadString(importResult, "modelId", &modelId))
    {
        goto cleanup;
    }

    addRequest = HostFormat(
        "{\"capability\":\"scene.apply_operation\","
        "\"operationId\":\"hostflow-add-%lu-%llu\","
        "\"sceneContext\":{\"resolvedProfileId\":\"profile-stage14e01\","
        "\"buildVolume\":{\"source\":\"device_profile\",\"widthMm\":230," 
        "\"heightMm\":100,\"zLimitMm\":60,\"origin\":\"lower_left\","
        "\"xDirection\":\"positive\",\"yDirection\":\"positive\","
        "\"isFixture\":false}},\"currentSceneRevision\":0,"
        "\"expectedSceneRevision\":0,\"operations\":[{"
        "\"type\":\"addInstance\",\"modelId\":\"%s\","
        "\"assignInstanceId\":\"instance-hostflow\"}]}",
        processId,
        nonce,
        modelId);
    if (addRequest == NULL
        || !RunJob(api, module, "addInstance", addRequest, 30000U, &addResult)
        || !HostJsonReadUnsigned(addResult, "sceneHandle", &sceneHandle)
        || !HostJsonReadUnsigned(addResult, "newSceneRevision", &revision)
        || sceneHandle == 0U || revision != 1U)
    {
        goto cleanup;
    }

    layoutRequest = HostFormat(
        "{\"capability\":\"scene.apply_operation\","
        "\"operationId\":\"hostflow-layout-%lu-%llu\","
        "\"sceneHandle\":%llu,\"currentSceneRevision\":1,"
        "\"expectedSceneRevision\":1,\"operations\":[{"
        "\"type\":\"applyGridLayout\",\"layout\":{\"policy\":\"grid\","
        "\"maxColumns\":11,\"maxRows\":2,\"columnGapMm\":10,"
        "\"rowGapMm\":10,\"spacingMode\":\"edge_clearance\","
        "\"order\":\"row_major\"}}]}",
        processId,
        nonce,
        sceneHandle);
    if (layoutRequest == NULL
        || !RunJob(api, module, "applyGridLayout", layoutRequest, 30000U, &layoutResult)
        || !HostJsonReadUnsigned(layoutResult, "newSceneRevision", &revision)
        || revision != 2U)
    {
        goto cleanup;
    }

    transformRequest = HostFormat(
        "{\"capability\":\"scene.apply_operation\","
        "\"operationId\":\"hostflow-transform-%lu-%llu\","
        "\"sceneHandle\":%llu,\"currentSceneRevision\":2,"
        "\"expectedSceneRevision\":2,\"operations\":[{"
        "\"type\":\"translate\",\"instanceId\":\"instance-hostflow\","
        "\"deltaMm\":[1,2,0]}]}",
        processId,
        nonce,
        sceneHandle);
    if (transformRequest == NULL
        || !RunJob(api, module, "translate", transformRequest, 30000U, &transformResult)
        || !HostJsonReadUnsigned(transformResult, "newSceneRevision", &revision)
        || revision != 3U)
    {
        goto cleanup;
    }

    snapshotRequest = HostFormat(
        "{\"capability\":\"scene.get_snapshot\",\"sceneHandle\":%llu}",
        sceneHandle);
    if (snapshotRequest == NULL
        || !RunJob(api, module, "scene.get_snapshot", snapshotRequest, 30000U, &snapshotResult)
        || !HostJsonReadObject(snapshotResult, "scene", &committedScene)
        || !HostJsonReadString(snapshotResult, "sceneHash", &sceneHash)
        || !HostJsonReadUnsigned(snapshotResult, "sceneRevision", &revision)
        || revision != 3U)
    {
        goto cleanup;
    }

    profile = HostBuildProfile(
        fixture,
        packageDirectory,
        profileHash,
        sizeof(profileHash));
    escapedPackage = HostJsonEscape(packageDirectory);
    if (profile == NULL || escapedPackage == NULL)
    {
        goto cleanup;
    }
    sliceRequest = HostFormat(
        "{\"capability\":\"slice.rgbwsv\","
        "\"jobId\":\"job-hostflow-%lu-%llu\","
        "\"correlationId\":\"correlation-hostflow-%lu-%llu\","
        "\"sceneHash\":\"%s%s\",\"scene\":%s,\"profile\":%s,"
        "\"output\":{\"contract\":\"p0.rgbwsv.2\",\"packageDir\":\"%s\"},"
        "\"options\":{\"backend\":\"worker\"}}",
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
        || layerCount == 0U)
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
        "HOSTFLOW_HA03_PASS sceneHandle=%llu revision=%llu layers=%llu\n",
        sceneHandle,
        revision,
        layerCount);
    success = 1;

cleanup:
    free(fixture);
    free(packageDirectory);
    free(escapedFixture);
    free(importRequest);
    free(importResult);
    free(modelId);
    free(addRequest);
    free(addResult);
    free(layoutRequest);
    free(layoutResult);
    free(transformRequest);
    free(transformResult);
    free(snapshotRequest);
    free(snapshotResult);
    free(committedScene);
    free(sceneHash);
    free(profile);
    free(escapedPackage);
    free(sliceRequest);
    free(sliceResult);
    free(verifyRequest);
    free(verifyResult);
    free(releaseRequest);
    free(releaseResult);
    return success;
}
