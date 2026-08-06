#pragma once

#include "slicer_worker/runtime/WorkerResultEnvelope.h"

#include <stdexcept>
#include <string>

namespace slicesoft::worker
{

/** @brief Stable result publication failure with output-category semantics. */
class WorkerResultWriteError final : public std::runtime_error
{
public:
    /** @brief Creates a PM-SLICER-OUTPUT-0050 result publication failure. */
    explicit WorkerResultWriteError(const std::string& message);

    /** @brief Returns the stable public error code. */
    [[nodiscard]] const std::string& StableCode() const noexcept;

    /** @brief Returns the frozen output-category process exit code. */
    [[nodiscard]] int ProcessExitCode() const noexcept;

private:
    std::string m_stableCode{"PM-SLICER-OUTPUT-0050"};
};

/** @brief Publishes result.json through a same-directory atomic replacement. */
class WorkerResultWriter final
{
public:
    /**
     * @brief Writes and atomically replaces the identity-owned result.json.
     * @param result Valid identity-closed result envelope.
     * @throws WorkerResultWriteError When temporary write or atomic replace fails.
     */
    static void WriteAtomically(const WorkerResultEnvelope& result);
};

}  // namespace slicesoft::worker
