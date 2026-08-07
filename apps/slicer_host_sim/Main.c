#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "HostFlowEndToEnd.h"
#include "HostM1Intake.h"
#include "HostModuleApi.h"
#include "HostPath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

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

int wmain(int argumentCount, wchar_t* arguments[])
{
    HostModuleApi api;
    char loadError[256];
    pm_module_t* module = NULL;
    char* repository = NULL;
    char* outputRoot = NULL;
    char* selfTest = NULL;
    const wchar_t* modulePath = NULL;
    int m1IntakeOnly = 0;
    int result = 1;
    memset(&api, 0, sizeof(api));
    loadError[0] = '\0';
    if (argumentCount == 3
        && wcscmp(arguments[1], L"--m1-self-test") == 0)
    {
        m1IntakeOnly = 1;
        modulePath = arguments[2];
    }
    else if (argumentCount == 4)
    {
        modulePath = arguments[1];
    }
    else
    {
        fwprintf(
            stderr,
            L"usage:\n"
            L"  slicer_host_sim --m1-self-test <slicer_module.dll>\n"
            L"  slicer_host_sim <slicer_module.dll> <repository> <output-root>\n");
        return 2;
    }
    if (!m1IntakeOnly && !HostEnsureDirectoryTree(arguments[3]))
    {
        fprintf(stderr, "[14E-01] failed to create output root\n");
        goto cleanup;
    }
    if (!m1IntakeOnly)
    {
        repository = HostUtf8FromWide(arguments[2]);
        outputRoot = HostUtf8FromWide(arguments[3]);
        if (repository == NULL || outputRoot == NULL)
        {
            fprintf(stderr, "[14E-01] UTF-8 path conversion failed\n");
            goto cleanup;
        }
    }
    if (!HostModuleApiLoad(
               &api,
               modulePath,
               loadError,
               sizeof(loadError)))
    {
        fprintf(stderr, "[14E-01] load failed: %s\n", loadError);
        goto cleanup;
    }
    if (!HostM1IntakeCheckModuleInfo(&api))
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
    if (!HostM1IntakeRun(&api, module, &selfTest))
    {
        goto cleanup;
    }
    if (m1IntakeOnly)
    {
        printf(
            "STAGE14F02_M1_INTAKE_PASS spi=%d capabilities=15\n",
            api.m_spiVersion());
    }
    if (!m1IntakeOnly
        && !HostFlowRunEndToEnd(&api, module, repository, outputRoot))
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
