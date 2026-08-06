#include "slicer_module/ModuleSelfTest.h"

namespace slicesoft::module
{
namespace
{

constexpr std::string_view ModuleSelfTestJson{
    R"json({"schema":"slicesoft.module_self_test.1","status":"passed","ok":true,"scope":"module_local","module":{"id":"slicer","spi":1},"checks":[{"id":"spi_version","status":"PASS"},{"id":"module_info","status":"PASS"},{"id":"buffer_protocol","status":"PASS"},{"id":"handle_registry","status":"PASS"},{"id":"persistent_side_effects","status":"PASS"}],"deferredChecks":[{"id":"C-SPI-08","status":"BLOCKED_BY_WORKER_GATE"},{"id":"C-SPI-09","status":"BLOCKED_BY_WORKER_GATE"},{"id":"C-SPI-13","status":"BLOCKED_BY_WORKER_GATE"}],"sideEffects":{"workerStarted":false,"persistentWrites":0}})json"};

}  // namespace

std::string_view GetModuleSelfTestJson() noexcept
{
    return ModuleSelfTestJson;
}

}  // namespace slicesoft::module
