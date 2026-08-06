#pragma once

class PackageWriteCancellation final : public std::runtime_error
{
public:
    explicit PackageWriteCancellation(const std::string& stage)
        : std::runtime_error(
            "RGBWSV package writing cancelled at " + stage)
    {
    }
};

void ThrowIfCancellationRequested(
    const api::ICancelToken* cancelToken,
    const std::string& stage)
{
    if (cancelToken != nullptr
        && cancelToken->IsCancelRequested())
    {
        throw PackageWriteCancellation(stage);
    }
}
