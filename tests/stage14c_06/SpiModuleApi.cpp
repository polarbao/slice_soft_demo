#include "SpiModuleApi.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace slicesoft::tests
{
namespace
{

struct DelayImportDescriptor
{
    DWORD attributes;
    DWORD name;
    DWORD modulehandle;
    DWORD importaddresstable;
    DWORD importnametable;
    DWORD boundimportaddresstable;
    DWORD unloadinformationtable;
    DWORD timestamp;
};

template <typename Value>
const Value* ResolveRva(const HMODULE library, const DWORD relativeAddress)
{
    const auto* const base = reinterpret_cast<const std::byte*>(library);
    return reinterpret_cast<const Value*>(base + relativeAddress);
}

const IMAGE_NT_HEADERS64& GetNtHeaders(const HMODULE library)
{
    if (library == nullptr)
    {
        throw std::runtime_error("PE image is not loaded");
    }
    const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(library);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        throw std::runtime_error("invalid PE DOS signature");
    }
    const auto* const nt = ResolveRva<IMAGE_NT_HEADERS64>(
        library,
        static_cast<DWORD>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE
        || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        throw std::runtime_error("invalid x64 PE header");
    }
    return *nt;
}

}  // namespace

SpiModuleApi::SpiModuleApi(const std::filesystem::path& libraryPath)
{
    m_library = LoadLibraryW(libraryPath.c_str());
    if (m_library == nullptr)
    {
        throw std::runtime_error("could not load module DLL");
    }
    try
    {
        m_spiVersion = LoadFunction<decltype(m_spiVersion)>("pm_spi_version");
        m_moduleInfo = LoadFunction<decltype(m_moduleInfo)>("pm_module_info");
        m_create = LoadFunction<decltype(m_create)>("pm_create");
        m_destroy = LoadFunction<decltype(m_destroy)>("pm_destroy");
        m_submit = LoadFunction<decltype(m_submit)>("pm_submit");
        m_poll = LoadFunction<decltype(m_poll)>("pm_poll");
        m_cancel = LoadFunction<decltype(m_cancel)>("pm_cancel");
        m_result = LoadFunction<decltype(m_result)>("pm_result");
        m_release = LoadFunction<decltype(m_release)>("pm_release");
        m_selfTest = LoadFunction<decltype(m_selfTest)>("pm_self_test");
        m_lastError = LoadFunction<decltype(m_lastError)>("pm_last_error");
    }
    catch (...)
    {
        FreeLibrary(m_library);
        m_library = nullptr;
        throw;
    }
}

SpiModuleApi::~SpiModuleApi()
{
    if (m_library != nullptr)
    {
        FreeLibrary(m_library);
    }
}

int SpiModuleApi::SpiVersion() const
{
    return m_spiVersion();
}

int SpiModuleApi::ModuleInfo(
    char* const output,
    const int capacity,
    int* const required) const
{
    return m_moduleInfo(output, capacity, required);
}

pm_module_t* SpiModuleApi::Create(const char* const optionsJson) const
{
    return m_create(optionsJson);
}

void SpiModuleApi::Destroy(pm_module_t* const module) const
{
    m_destroy(module);
}

pm_job_t* SpiModuleApi::Submit(
    pm_module_t* const module,
    const char* const requestJson) const
{
    return m_submit(module, requestJson);
}

int SpiModuleApi::Poll(
    pm_job_t* const job,
    char* const output,
    const int capacity,
    int* const required) const
{
    return m_poll(job, output, capacity, required);
}

int SpiModuleApi::Cancel(pm_job_t* const job) const
{
    return m_cancel(job);
}

int SpiModuleApi::Result(
    pm_job_t* const job,
    char* const output,
    const int capacity,
    int* const required) const
{
    return m_result(job, output, capacity, required);
}

void SpiModuleApi::Release(pm_job_t* const job) const
{
    m_release(job);
}

int SpiModuleApi::SelfTest(
    pm_module_t* const module,
    char* const output,
    const int capacity,
    int* const required) const
{
    return m_selfTest(module, output, capacity, required);
}

int SpiModuleApi::LastError(
    char* const output,
    const int capacity,
    int* const required) const
{
    return m_lastError(output, capacity, required);
}

std::vector<std::string> SpiModuleApi::ExportNames() const
{
    const auto& nt = GetNtHeaders(m_library);
    const auto& directory = nt.OptionalHeader.DataDirectory[
        IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (directory.VirtualAddress == 0U)
    {
        return {};
    }
    const auto* const exports = ResolveRva<IMAGE_EXPORT_DIRECTORY>(
        m_library,
        directory.VirtualAddress);
    if (exports->NumberOfFunctions != exports->NumberOfNames)
    {
        throw std::runtime_error("PE contains unnamed ordinal exports");
    }
    const auto* const names = ResolveRva<DWORD>(m_library, exports->AddressOfNames);
    std::vector<std::string> result;
    result.reserve(exports->NumberOfNames);
    for (DWORD index = 0U; index < exports->NumberOfNames; ++index)
    {
        result.emplace_back(ResolveRva<char>(m_library, names[index]));
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> SpiModuleApi::ImportedDllNames() const
{
    const auto& nt = GetNtHeaders(m_library);
    const auto& directory = nt.OptionalHeader.DataDirectory[
        IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (directory.VirtualAddress == 0U)
    {
        return {};
    }
    const auto* descriptor = ResolveRva<IMAGE_IMPORT_DESCRIPTOR>(
        m_library,
        directory.VirtualAddress);
    std::vector<std::string> result;
    while (descriptor->Name != 0U)
    {
        result.emplace_back(ResolveRva<char>(m_library, descriptor->Name));
        ++descriptor;
    }
    const auto& delayDirectory = nt.OptionalHeader.DataDirectory[
        IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
    if (delayDirectory.VirtualAddress != 0U)
    {
        const auto* delay = ResolveRva<DelayImportDescriptor>(
            m_library,
            delayDirectory.VirtualAddress);
        while (delay->name != 0U)
        {
            result.emplace_back(ResolveRva<char>(m_library, delay->name));
            ++delay;
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

template <typename Function>
Function SpiModuleApi::LoadFunction(const char* const name) const
{
    const FARPROC address = GetProcAddress(m_library, name);
    if (address == nullptr)
    {
        throw std::runtime_error(std::string{"missing SPI export: "} + name);
    }
    return reinterpret_cast<Function>(address);
}

}  // namespace slicesoft::tests
