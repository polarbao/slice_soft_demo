#define PM_MODULE_STATIC
#include "print_module_spi.h"

#if PM_SPI_VERSION != 1
#error "Unexpected PM SPI version"
#endif

static int (PM_CALL * const spiVersionFunction)(void) = &pm_spi_version;
static int (PM_CALL * const moduleInfoFunction)(char*, int, int*) = &pm_module_info;
static pm_module_t* (PM_CALL * const createFunction)(const char*) = &pm_create;
static void (PM_CALL * const destroyFunction)(pm_module_t*) = &pm_destroy;
static pm_job_t* (PM_CALL * const submitFunction)(pm_module_t*, const char*) = &pm_submit;
static int (PM_CALL * const pollFunction)(pm_job_t*, char*, int, int*) = &pm_poll;
static int (PM_CALL * const cancelFunction)(pm_job_t*) = &pm_cancel;
static int (PM_CALL * const resultFunction)(pm_job_t*, char*, int, int*) = &pm_result;
static void (PM_CALL * const releaseFunction)(pm_job_t*) = &pm_release;
static int (PM_CALL * const selfTestFunction)(pm_module_t*, char*, int, int*) = &pm_self_test;
static int (PM_CALL * const lastErrorFunction)(char*, int, int*) = &pm_last_error;

void VerifyPrintModuleSpiCContract(void)
{
    (void)spiVersionFunction;
    (void)moduleInfoFunction;
    (void)createFunction;
    (void)destroyFunction;
    (void)submitFunction;
    (void)pollFunction;
    (void)cancelFunction;
    (void)resultFunction;
    (void)releaseFunction;
    (void)selfTestFunction;
    (void)lastErrorFunction;
}
