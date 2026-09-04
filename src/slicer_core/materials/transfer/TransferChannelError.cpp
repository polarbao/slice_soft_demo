#include "slicer_core/materials/transfer/TransferChannelError.h"

namespace slicer_core
{

std::string TransferChannelErrorCodeName(const TransferChannelErrorCode code)
{
    switch (code)
    {
        case TransferChannelErrorCode::ConfigInvalid:
            return "E_MATVOL_T_CONFIG_INVALID";
        case TransferChannelErrorCode::RegionMissing:
            return "E_MATVOL_T_REGION_MISSING";
        case TransferChannelErrorCode::MatchAmbiguous:
            return "E_MATVOL_T_MATCH_AMBIGUOUS";
        case TransferChannelErrorCode::TopologyInvalid:
            return "E_MATVOL_T_TOPOLOGY_INVALID";
        case TransferChannelErrorCode::MaskOutsideModel:
            return "E_MATVOL_T_MASK_OUTSIDE_MODEL";
        case TransferChannelErrorCode::ProtocolInvalid:
            return "E_MATVOL_T_PROTOCOL_INVALID";
    }
    return "E_MATVOL_T_UNKNOWN";
}

TransferChannelError::TransferChannelError(
    const TransferChannelErrorCode code,
    const std::string& detail)
    : std::runtime_error(TransferChannelErrorCodeName(code) + ": " + detail),
      code_(code)
{
}

}  // namespace slicer_core
