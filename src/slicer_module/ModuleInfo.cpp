#include "slicer_module/ModuleInfo.h"

#if !defined(_MSC_VER) || !defined(_M_X64)
#error SliceSoft module metadata requires the frozen MSVC x64 runtime.
#endif

#if !defined(_DLL)
#error SliceSoft module metadata requires the dynamic MSVC runtime (/MD or /MDd).
#endif

namespace slicesoft::module
{
namespace
{

constexpr std::string_view ModuleId{"slicer"};
constexpr std::string_view ModuleVersion{"0.1.0"};

#if defined(_DEBUG)
constexpr std::string_view ModuleRuntime{"MSVC-x64-MDd"};
constexpr std::string_view ModuleBuildConfig{"Debug"};
#define SLICESOFT_MODULE_RUNTIME_JSON "MSVC-x64-MDd"
#define SLICESOFT_MODULE_BUILD_CONFIG_JSON "Debug"
#else
constexpr std::string_view ModuleRuntime{"MSVC-x64-MD"};
constexpr std::string_view ModuleBuildConfig{"Release"};
#define SLICESOFT_MODULE_RUNTIME_JSON "MSVC-x64-MD"
#define SLICESOFT_MODULE_BUILD_CONFIG_JSON "Release"
#endif

constexpr std::string_view ModuleInfoJson{
    R"json({"schema":"slicesoft.module_info.1","id":"slicer","name":"SliceSoft Geometry Slicer","version":"0.1.0","spi":1,"runtime":")json"
    SLICESOFT_MODULE_RUNTIME_JSON
    R"json(","buildConfig":")json"
    SLICESOFT_MODULE_BUILD_CONFIG_JSON
    R"json(","provides":["model.import","model.get_metadata","model.release","scene.apply_operation","scene.get_snapshot","scene.get_viewdata","geometry.preflight","geometry.collision","geometry.repair","slice.rgbwsv","package.verify","package.get_summary","package.get_layer_descriptor","package.render_layer_preview","package.read_report"],"produces":[{"contract":"p0.rgbwsv.2","kind":"package"}],"capabilities":{"maxConcurrentJobs":1,"cancelLatencyMs":2000,"syncCapabilities":["model.import","model.get_metadata","model.release","scene.apply_operation","scene.get_snapshot","scene.get_viewdata","geometry.preflight","geometry.collision","package.verify","package.get_summary","package.get_layer_descriptor","package.render_layer_preview","package.read_report"],"workerCapabilities":["geometry.preflight","geometry.repair","slice.rgbwsv"]}})json"};

#undef SLICESOFT_MODULE_RUNTIME_JSON
#undef SLICESOFT_MODULE_BUILD_CONFIG_JSON

}  // namespace

std::string_view GetModuleId() noexcept
{
    return ModuleId;
}

std::string_view GetModuleVersion() noexcept
{
    return ModuleVersion;
}

std::string_view GetModuleRuntime() noexcept
{
    return ModuleRuntime;
}

std::string_view GetModuleBuildConfig() noexcept
{
    return ModuleBuildConfig;
}

std::string_view GetModuleInfoJson() noexcept
{
    return ModuleInfoJson;
}

}  // namespace slicesoft::module
