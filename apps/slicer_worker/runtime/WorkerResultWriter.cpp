#include "slicer_worker/runtime/WorkerResultWriter.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace slicesoft::worker
{
namespace
{

void RemoveTemporary(const std::filesystem::path& path) noexcept
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

std::string WindowsErrorMessage(const DWORD error)
{
    return "atomic result replacement failed with Windows error "
        + std::to_string(error);
}

}  // namespace

WorkerResultWriteError::WorkerResultWriteError(const std::string& message)
    : std::runtime_error(message)
{
}

const std::string& WorkerResultWriteError::StableCode() const noexcept
{
    return m_stableCode;
}

int WorkerResultWriteError::ProcessExitCode() const noexcept
{
    return 6;
}

void WorkerResultWriter::WriteAtomically(const WorkerResultEnvelope& result)
{
    const WorkerJobIdentity& identity = result.Identity();
    const std::filesystem::path& resultPath = identity.ResultPath();
    const std::filesystem::path& temporaryPath = identity.ResultTemporaryPath();
    if (resultPath.parent_path() != identity.JobDirectory()
        || temporaryPath.parent_path() != identity.JobDirectory())
    {
        throw WorkerResultWriteError("result paths are outside the trusted job directory");
    }

    const std::string payload = result.ToJson().dump(2) + '\n';
    {
        std::ofstream output{
            temporaryPath,
            std::ios::binary | std::ios::trunc};
        if (!output)
        {
            RemoveTemporary(temporaryPath);
            throw WorkerResultWriteError("result.json.tmp could not be opened");
        }
        output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        output.flush();
        if (!output)
        {
            output.close();
            RemoveTemporary(temporaryPath);
            throw WorkerResultWriteError("result.json.tmp could not be written completely");
        }
    }

    if (MoveFileExW(
            temporaryPath.c_str(),
            resultPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE)
    {
        const DWORD error = GetLastError();
        RemoveTemporary(temporaryPath);
        throw WorkerResultWriteError(WindowsErrorMessage(error));
    }
}

}  // namespace slicesoft::worker
