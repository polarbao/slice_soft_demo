#pragma once

#include "contracts/print_module_spi.h"

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace slicesoft::tests
{

/**
 * @brief Runtime-loaded view of the frozen print-module C ABI.
 *
 * The conformance executable deliberately owns no import-library dependency on
 * slicer_module. Every operation resolves and calls one of the 11 public C ABI
 * exports from the supplied DLL.
 */
class SpiModuleApi final
{
public:
    /**
     * @brief Loads a module DLL and resolves the exact SPI v1 export set.
     * @param libraryPath Absolute or relative path to slicer_module.dll.
     */
    explicit SpiModuleApi(const std::filesystem::path& libraryPath);

    /**
     * @brief Unloads the runtime-loaded module DLL.
     */
    ~SpiModuleApi();

    SpiModuleApi(const SpiModuleApi&) = delete;
    SpiModuleApi& operator=(const SpiModuleApi&) = delete;
    SpiModuleApi(SpiModuleApi&&) = delete;
    SpiModuleApi& operator=(SpiModuleApi&&) = delete;

    /** @brief Calls pm_spi_version. @return Module SPI version. */
    [[nodiscard]] int SpiVersion() const;

    /** @brief Calls pm_module_info. @return SPI return value. */
    [[nodiscard]] int ModuleInfo(char* output, int capacity, int* required) const;

    /** @brief Calls pm_create. @return Opaque module handle or nullptr. */
    [[nodiscard]] pm_module_t* Create(const char* optionsJson) const;

    /** @brief Calls pm_destroy. @param module Opaque module handle. */
    void Destroy(pm_module_t* module) const;

    /** @brief Calls pm_submit. @return Opaque job handle or nullptr. */
    [[nodiscard]] pm_job_t* Submit(
        pm_module_t* module,
        const char* requestJson) const;

    /** @brief Calls pm_poll. @return SPI return value. */
    [[nodiscard]] int Poll(
        pm_job_t* job,
        char* output,
        int capacity,
        int* required) const;

    /** @brief Calls pm_cancel. @return SPI return value. */
    [[nodiscard]] int Cancel(pm_job_t* job) const;

    /** @brief Calls pm_result. @return SPI return value. */
    [[nodiscard]] int Result(
        pm_job_t* job,
        char* output,
        int capacity,
        int* required) const;

    /** @brief Calls pm_release. @param job Opaque job handle. */
    void Release(pm_job_t* job) const;

    /** @brief Calls pm_self_test. @return SPI return value. */
    [[nodiscard]] int SelfTest(
        pm_module_t* module,
        char* output,
        int capacity,
        int* required) const;

    /** @brief Calls pm_last_error. @return SPI return value. */
    [[nodiscard]] int LastError(char* output, int capacity, int* required) const;

    /**
     * @brief Enumerates names from the loaded PE export directory.
     * @return Sorted export names.
     */
    [[nodiscard]] std::vector<std::string> ExportNames() const;

    /**
     * @brief Enumerates DLL names from the loaded PE import directory.
     * @return Sorted imported DLL names.
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
