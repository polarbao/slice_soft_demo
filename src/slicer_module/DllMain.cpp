#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

BOOL APIENTRY DllMain(
    HMODULE module,
    DWORD reason,
    LPVOID reserved)
{
    (void)module;
    (void)reason;
    (void)reserved;
    return TRUE;
}

#endif
