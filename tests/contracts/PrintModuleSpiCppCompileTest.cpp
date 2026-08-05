#define PM_MODULE_STATIC
#include "print_module_spi.h"

#include <type_traits>

static_assert(PM_SPI_VERSION == 1);
static_assert(std::is_same_v<decltype(&pm_spi_version), int (PM_CALL *)(void)>);
static_assert(std::is_same_v<decltype(&pm_module_info), int (PM_CALL *)(char*, int, int*)>);
static_assert(std::is_same_v<decltype(&pm_create), pm_module_t* (PM_CALL *)(const char*)>);
static_assert(std::is_same_v<decltype(&pm_destroy), void (PM_CALL *)(pm_module_t*)>);
static_assert(std::is_same_v<decltype(&pm_submit), pm_job_t* (PM_CALL *)(pm_module_t*, const char*)>);
static_assert(std::is_same_v<decltype(&pm_poll), int (PM_CALL *)(pm_job_t*, char*, int, int*)>);
static_assert(std::is_same_v<decltype(&pm_cancel), int (PM_CALL *)(pm_job_t*)>);
static_assert(std::is_same_v<decltype(&pm_result), int (PM_CALL *)(pm_job_t*, char*, int, int*)>);
static_assert(std::is_same_v<decltype(&pm_release), void (PM_CALL *)(pm_job_t*)>);
static_assert(std::is_same_v<decltype(&pm_self_test), int (PM_CALL *)(pm_module_t*, char*, int, int*)>);
static_assert(std::is_same_v<decltype(&pm_last_error), int (PM_CALL *)(char*, int, int*)>);
