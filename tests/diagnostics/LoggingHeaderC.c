#define PM_MODULE_STATIC
#include "contracts/slicer_logging.h"

static void PM_CALL Callback(void* context, int level, const char* text, int count)
{
    (void)context; (void)level; (void)text; (void)count;
}
int main(void)
{
    slicer_log_callback_v1 callback = Callback;
    callback(0, 2, "", 0);
    return SLICER_LOG_API_VERSION == 1 ? 0 : 1;
}
