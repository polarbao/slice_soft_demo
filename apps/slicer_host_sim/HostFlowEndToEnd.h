#pragma once

#include "HostModuleApi.h"

/**
 * @brief Runs the HOSTFLOW H-A-03 empty-scene production closure.
 * @param api Loaded public SPI function table.
 * @param module Live module instance.
 * @param repository UTF-8 repository root containing the reference model.
 * @param outputRoot UTF-8 host-owned evidence directory.
 * @return Non-zero when import, scene edits, slice, and verification succeed.
 */
int HostFlowRunEndToEnd(
    const HostModuleApi* api,
    pm_module_t* module,
    const char* repository,
    const char* outputRoot);
