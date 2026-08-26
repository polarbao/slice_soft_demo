#pragma once

#include <stdexcept>
#include <string>

namespace slicer_core
{

enum class TransferChannelErrorCode
{
    ConfigInvalid,
    RegionMissing,
    MatchAmbiguous,
    TopologyInvalid,
    MaskOutsideModel,
    ProtocolInvalid,
};

[[nodiscard]] std::string TransferChannelErrorCodeName(TransferChannelErrorCode code);

class TransferChannelError final : public std::runtime_error
{
public:
    TransferChannelError(TransferChannelErrorCode code, const std::string& detail);

    [[nodiscard]] TransferChannelErrorCode Code() const noexcept { return code_; }

private:
    TransferChannelErrorCode code_;
};

}  // namespace slicer_core
