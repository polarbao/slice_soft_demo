#ifndef SLICER_LOGGING_H
#define SLICER_LOGGING_H

#include "print_module_spi.h"

#define SLICER_LOG_API_VERSION 1
#define SLICER_LOG_OK 0
#define SLICER_LOG_INVALID_ARGUMENT (-1)
#define SLICER_LOG_INVALID_STATE (-2)
#define SLICER_LOG_TIMEOUT (-3)
#define SLICER_LOG_RESOURCE_ERROR (-4)
#define SLICER_LOG_MAX_EVENT_BYTES 16384

#ifdef __cplusplus
extern "C" {
#endif

/* Each module owns one callback slot. Data is borrowed until callback returns.
 * Copy/enqueue only: no blocking IO, GUI work, or calls back into the module.
 * Levels: off=-1 (configuration only), trace=0 .. critical=5.
 * The context is never dereferenced or freed by the DLL. */
typedef void (PM_CALL *slicer_log_callback_v1)(
    void* user_context, int level, const char* event_json_utf8, int byte_count);

PM_API int PM_CALL slicer_log_api_version(void);
PM_API int PM_CALL slicer_set_log_callback_v1(
    pm_module_t* module, slicer_log_callback_v1 callback,
    void* user_context, int minimum_level);

/* Success is a callback-quiescence barrier. On timeout the caller must keep
 * context, module and DLL alive and retry. Pending events may be discarded.
 * Clear before destroying the instance or unloading its DLL. Never clear
 * from inside the callback. Errors do not change pm_last_error. */
PM_API int PM_CALL slicer_clear_log_callback_v1(pm_module_t* module, int timeout_ms);

#ifdef __cplusplus
}
#endif
#endif
