#include "HostM1Intake.h"

#include "JsonText.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int JsonSucceeded(const char* json)
{
    int ok = 0;
    return HostJsonReadBoolean(json, "ok", &ok) && ok != 0;
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
        printf("[14F-02] fail-closed PASS: %s\n", error);
    }
    free(error);
    return passed;
}

int HostM1IntakeCheckModuleInfo(const HostModuleApi* api)
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

int HostM1IntakeRun(
    const HostModuleApi* api,
    pm_module_t* module,
    char** selfTest)
{
    return HostModuleApiReadSelfTest(api, module, selfTest)
        && JsonSucceeded(*selfTest)
        && CheckFailClosed(api, module);
}
