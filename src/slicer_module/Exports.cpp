#include "contracts/print_module_spi.h"

extern "C" PM_API int PM_CALL pm_spi_version(void)
{
    try
    {
        return PM_SPI_VERSION;
    }
    catch (...)
    {
        return PM_ERR_FAILED;
    }
}

extern "C" PM_API int PM_CALL pm_module_info(char* jsonOut, int cap, int* outRequired)
{
    try
    {
        (void)jsonOut;
        (void)cap;
        (void)outRequired;
        return PM_ERR_FAILED;
    }
    catch (...)
    {
        return PM_ERR_FAILED;
    }
}

extern "C" PM_API pm_module_t* PM_CALL pm_create(const char* optionsJson)
{
    try
    {
        (void)optionsJson;
        return nullptr;
    }
    catch (...)
    {
        return nullptr;
    }
}

extern "C" PM_API void PM_CALL pm_destroy(pm_module_t* module)
{
    try
    {
        (void)module;
    }
    catch (...)
    {
    }
}

extern "C" PM_API pm_job_t* PM_CALL pm_submit(pm_module_t* module, const char* requestJson)
{
    try
    {
        (void)module;
        (void)requestJson;
        return nullptr;
    }
    catch (...)
    {
        return nullptr;
    }
}

extern "C" PM_API int PM_CALL pm_poll(
    pm_job_t* job,
    char* progressJson,
    int cap,
    int* outRequired)
{
    try
    {
        (void)job;
        (void)progressJson;
        (void)cap;
        (void)outRequired;
        return PM_ERR_FAILED;
    }
    catch (...)
    {
        return PM_ERR_FAILED;
    }
}

extern "C" PM_API int PM_CALL pm_cancel(pm_job_t* job)
{
    try
    {
        (void)job;
        return PM_ERR_FAILED;
    }
    catch (...)
    {
        return PM_ERR_FAILED;
    }
}

extern "C" PM_API int PM_CALL pm_result(
    pm_job_t* job,
    char* resultJson,
    int cap,
    int* outRequired)
{
    try
    {
        (void)job;
        (void)resultJson;
        (void)cap;
        (void)outRequired;
        return PM_ERR_FAILED;
    }
    catch (...)
    {
        return PM_ERR_FAILED;
    }
}

extern "C" PM_API void PM_CALL pm_release(pm_job_t* job)
{
    try
    {
        (void)job;
    }
    catch (...)
    {
    }
}

extern "C" PM_API int PM_CALL pm_self_test(
    pm_module_t* module,
    char* reportJson,
    int cap,
    int* outRequired)
{
    try
    {
        (void)module;
        (void)reportJson;
        (void)cap;
        (void)outRequired;
        return PM_ERR_FAILED;
    }
    catch (...)
    {
        return PM_ERR_FAILED;
    }
}

extern "C" PM_API int PM_CALL pm_last_error(char* jsonOut, int cap, int* outRequired)
{
    try
    {
        (void)jsonOut;
        (void)cap;
        (void)outRequired;
        return PM_ERR_FAILED;
    }
    catch (...)
    {
        return PM_ERR_FAILED;
    }
}
