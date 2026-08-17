#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <chrono>
#include <cstdlib>
#include <thread>

int main()
{
    if (const char* value = std::getenv("RIPFLOW_FAKE_EXIT_CODE"))
    {
        const int code = std::atoi(value);
        if (code != 0)
        {
            return code;
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds{5});
    return 0;
}
