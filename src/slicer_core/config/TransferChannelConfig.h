#pragma once

#include "slicer_core/config.h"
#include "slicer_core/json_value.h"

namespace slicer_core
{

void LoadTransferChannelPolicy(
    const Json& root,
    TransferChannelPolicyConfig& policy);

void ValidateTransferChannelConfiguration(
    const OutputConfig& output,
    const TransferChannelPolicyConfig& policy);

}  // namespace slicer_core
