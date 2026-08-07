#ifndef SLICESOFT_APPS_SLICER_HOST_SIM_HOST_M1_INTAKE_H
#define SLICESOFT_APPS_SLICER_HOST_SIM_HOST_M1_INTAKE_H

#include "HostModuleApi.h"

/**
 * @brief Validates SPI v1 and the frozen fifteen-capability module list.
 * @param api Loaded host-side module function table.
 * @return Non-zero when the module information satisfies the M1 contract.
 */
int HostM1IntakeCheckModuleInfo(const HostModuleApi* api);

/**
 * @brief Runs the M1 self-test and unknown-capability fail-closed probe.
 * @param api Loaded host-side module function table.
 * @param module Live module instance created through pm_create.
 * @param selfTest Receives the allocated UTF-8 self-test response.
 * @return Non-zero when both M1 checks pass.
 */
int HostM1IntakeRun(
    const HostModuleApi* api,
    pm_module_t* module,
    char** selfTest);

#endif
