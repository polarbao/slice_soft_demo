#pragma once

class ProductionCancellation final : public std::runtime_error
{
public:
    explicit ProductionCancellation(const std::string& stage)
        : std::runtime_error(stage)
    {
    }
};

bool IsCancellationRequested(
    const MultiModelProductionRequest& request) noexcept
{
    return request.canceltoken != nullptr
        && request.canceltoken->IsCancelRequested();
}

void ThrowIfCancellationRequested(
    const MultiModelProductionRequest& request,
    const std::string& stage)
{
    if (IsCancellationRequested(request))
    {
        throw ProductionCancellation(stage);
    }
}

double ElapsedMilliseconds(
    const ProductionClock::time_point& start)
{
    return std::chrono::duration<double, std::milli>(
               ProductionClock::now() - start)
        .count();
}

void ReportProgress(
    const MultiModelProductionRequest& request,
    const ProductionClock::time_point& runStart,
    const std::string& phase,
    const int current,
    const int total,
    const int percent)
{
    ThrowIfCancellationRequested(request, phase);
    if (!request.progresscallback)
    {
        return;
    }
    request.progresscallback(
        SliceRunProgress{
            phase,
            current,
            total,
            std::clamp(percent, 0, 100),
            ElapsedMilliseconds(runStart)});
}

MultiModelProductionResult Block(
    const MultiModelProductionRequest& request,
    const MultiModelProductionErrorCode code,
    const std::string& field,
    const std::string& message,
    const std::string& sceneId = {},
    const std::string& modelId = {},
    const std::string& instanceId = {})
{
    MultiModelProductionResult result;
    MultiModelProductionError error;
    error.code = code;
    error.sceneid = sceneId;
    error.modelid = modelId;
    error.instanceid = instanceId;
    error.field = field.empty()
        ? request.effectiveconfigpath.generic_string()
        : field;
    error.message = message;
    result.sceneid = sceneId;
    result.error = std::move(error);
    return result;
}
