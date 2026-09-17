#include "contracts/slicer_logging.h"
#include "ModuleLogRegistry.h"

extern "C" PM_API int PM_CALL slicer_log_api_version(void)
{
    return SLICER_LOG_API_VERSION;
}

extern "C" PM_API int PM_CALL slicer_set_log_callback_v1(
    pm_module_t* module, slicer_log_callback_v1 callback,
    void* userContext, int minimumLevel)
{
    return slicesoft::module::logging::SetCallback(module, callback, userContext, minimumLevel);
}

extern "C" PM_API int PM_CALL slicer_clear_log_callback_v1(
    pm_module_t* module, int timeoutMs)
{
    return slicesoft::module::logging::ClearCallback(module, timeoutMs);
}
