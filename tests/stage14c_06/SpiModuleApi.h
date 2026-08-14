#pragma once

#include "contracts/print_module_spi.h"

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace slicesoft::tests
{

/**
 * @brief 冻结打印模块 C ABI 的运行时加载视图。
 *
 * 一致性测试程序有意不链接 slicer_module 导入库；所有操作都从指定 DLL
 * 动态解析并调用 11 个公开 C ABI 导出符号，以验证真实部署边界。
 */
class SpiModuleApi final
{
public:
    /**
     * @brief 加载模块 DLL 并解析完整的 SPI v1 导出集合。
     * @param libraryPath slicer_module.dll 的绝对或相对路径。
     */
    explicit SpiModuleApi(const std::filesystem::path& libraryPath);

    /**
     * @brief 卸载运行时加载的模块 DLL。
     */
    ~SpiModuleApi();

    SpiModuleApi(const SpiModuleApi&) = delete;
    SpiModuleApi& operator=(const SpiModuleApi&) = delete;
    SpiModuleApi(SpiModuleApi&&) = delete;
    SpiModuleApi& operator=(SpiModuleApi&&) = delete;

    /** @brief 调用 pm_spi_version。 @return 模块 SPI 版本。 */
    [[nodiscard]] int SpiVersion() const;

    /** @brief 调用 pm_module_info。 @return SPI 返回值。 */
    [[nodiscard]] int ModuleInfo(char* output, int capacity, int* required) const;

    /** @brief 调用 pm_create。 @return 不透明模块句柄，失败时为 nullptr。 */
    [[nodiscard]] pm_module_t* Create(const char* optionsJson) const;

    /** @brief 调用 pm_destroy。 @param module 不透明模块句柄。 */
    void Destroy(pm_module_t* module) const;

    /** @brief 调用 pm_submit。 @return 不透明任务句柄，失败时为 nullptr。 */
    [[nodiscard]] pm_job_t* Submit(
        pm_module_t* module,
        const char* requestJson) const;

    /** @brief 调用 pm_poll。 @return SPI 返回值。 */
    [[nodiscard]] int Poll(
        pm_job_t* job,
        char* output,
        int capacity,
        int* required) const;

    /** @brief 调用 pm_cancel。 @return SPI 返回值。 */
    [[nodiscard]] int Cancel(pm_job_t* job) const;

    /** @brief 调用 pm_result。 @return SPI 返回值。 */
    [[nodiscard]] int Result(
        pm_job_t* job,
        char* output,
        int capacity,
        int* required) const;

    /** @brief 调用 pm_release。 @param job 不透明任务句柄。 */
    void Release(pm_job_t* job) const;

    /** @brief 调用 pm_self_test。 @return SPI 返回值。 */
    [[nodiscard]] int SelfTest(
        pm_module_t* module,
        char* output,
        int capacity,
        int* required) const;

    /** @brief 调用 pm_last_error。 @return SPI 返回值。 */
    [[nodiscard]] int LastError(char* output, int capacity, int* required) const;

    /**
     * @brief 枚举已加载 PE 的导出目录名称。
     * @return 排序后的导出名称。
     */
    [[nodiscard]] std::vector<std::string> ExportNames() const;

    /**
     * @brief 枚举已加载 PE 的导入目录 DLL 名称。
     * @return 排序后的导入 DLL 名称。
     */
    [[nodiscard]] std::vector<std::string> ImportedDllNames() const;

private:
    template <typename Function>
    [[nodiscard]] Function LoadFunction(const char* name) const;

    HMODULE m_library{nullptr};
    decltype(&pm_spi_version) m_spiVersion{nullptr};
    decltype(&pm_module_info) m_moduleInfo{nullptr};
    decltype(&pm_create) m_create{nullptr};
    decltype(&pm_destroy) m_destroy{nullptr};
    decltype(&pm_submit) m_submit{nullptr};
    decltype(&pm_poll) m_poll{nullptr};
    decltype(&pm_cancel) m_cancel{nullptr};
    decltype(&pm_result) m_result{nullptr};
    decltype(&pm_release) m_release{nullptr};
    decltype(&pm_self_test) m_selfTest{nullptr};
    decltype(&pm_last_error) m_lastError{nullptr};
};

}  // namespace slicesoft::tests
