#pragma once

#include "slicer_worker/runtime/WorkerResultEnvelope.h"

#include <stdexcept>
#include <string>

namespace slicesoft::worker
{

/** @brief 具有输出类别语义的稳定结果发布失败。 */
class WorkerResultWriteError final : public std::runtime_error
{
public:
    /** @brief 创建 PM-SLICER-OUTPUT-0050 结果发布失败。 */
    explicit WorkerResultWriteError(const std::string& message);

    /** @brief 返回稳定公共错误码。 */
    [[nodiscard]] const std::string& StableCode() const noexcept;

    /** @brief 返回冻结的输出类别进程退出码。 */
    [[nodiscard]] int ProcessExitCode() const noexcept;

private:
    std::string m_stableCode{"PM-SLICER-OUTPUT-0050"};
};

/** @brief 通过同目录原子替换发布 result.json。 */
class WorkerResultWriter final
{
public:
    /**
     * @brief 写入并原子替换由标识持有的 result.json。
     * @param result 有效且标识闭合的结果信封。
     * @throws WorkerResultWriteError 临时写入或原子替换失败时抛出。
     */
    static void WriteAtomically(const WorkerResultEnvelope& result);
};

}  // namespace slicesoft::worker
