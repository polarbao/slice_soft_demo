#include "slicer_core/pipeline/GlobalSurfaceShellProductionPackage.h"

#include <stdexcept>
#include <utility>

namespace slicer_core
{
namespace
{

bool HasCurrentProtocol(const RgbwsvProtocol& protocol)
{
    const RgbwsvProtocol current = CurrentRgbwsvProtocol();
    return protocol.schema == current.schema
        && protocol.channel_order == current.channel_order
        && protocol.bit_depth == current.bit_depth
        && protocol.polarity == current.polarity
        && protocol.print_value == current.print_value
        && protocol.empty_value == current.empty_value;
}

}  // namespace

RgbwsvProductionPackageWriteResult
WriteGlobalSurfaceShellProductionPackage(
    RgbwsvProductionPackageWriteRequest request,
    GlobalSurfaceShellProductionLayerAdapterResult adapter)
{
    if (request.requestedPipelineMode != "global_surface_shell"
        || request.effectivePipelineMode != "global_surface_shell")
    {
        throw std::invalid_argument(
            "Global Surface Shell package requires matching global_surface_shell modes");
    }
    if (!request.layers.empty())
    {
        throw std::invalid_argument(
            "Global Surface Shell package layers must come only from the production adapter");
    }
    if (!adapter.available
        || !adapter.fullClosurePass
        || adapter.status != "ready_for_writer"
        || adapter.productionOutputWritten
        || !HasCurrentProtocol(adapter.protocol)
        || adapter.errorCode != SlicePipelineErrorCode::None)
    {
        throw std::runtime_error(
            "Global Surface Shell package is blocked: adapter is not writer-ready");
    }
    if (adapter.widthPx != request.grid.widthPx
        || adapter.heightPx != request.grid.heightPx
        || adapter.layerCount != request.grid.layerCount
        || adapter.layers.size()
            != static_cast<std::size_t>(request.grid.layerCount))
    {
        throw std::invalid_argument(
            "Global Surface Shell adapter dimensions do not match the package grid");
    }

    request.layers.reserve(adapter.layers.size());
    for (GlobalSurfaceShellProductionLayer& layer : adapter.layers)
    {
        request.layers.push_back(std::move(layer.output));
    }
    return WriteRgbwsvProductionPackage(request);
}

}  // namespace slicer_core
