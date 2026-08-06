#include "slicer_module/BufferApi.h"

#include "contracts/print_module_spi.h"

#include <cstring>
#include <limits>

namespace slicesoft::module
{

int WriteOut(
    const std::string_view content,
    char* const output,
    const int capacity,
    int* const outRequired) noexcept
{
    if (capacity < 0
        || content.size() > static_cast<std::size_t>(
            std::numeric_limits<int>::max()))
    {
        return PM_ERR_INVALID_ARG;
    }

    const int required = static_cast<int>(content.size());
    if (outRequired != nullptr)
    {
        *outRequired = required;
    }

    if (output == nullptr || capacity == 0)
    {
        return PM_ERR_BUFFER_SMALL;
    }

    if (capacity <= required)
    {
        return PM_ERR_BUFFER_SMALL;
    }

    if (required > 0)
    {
        std::memmove(
            output,
            content.data(),
            static_cast<std::size_t>(required));
    }
    output[required] = '\0';
    return required;
}

}  // namespace slicesoft::module
