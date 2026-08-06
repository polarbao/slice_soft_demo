#pragma once

#include "WorkerClient.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicesoft::module
{

/** @brief Stable outcomes from file_contract_v1 discovery and compatibility checks. */
enum class WorkerContractDecision
{
    Compatible,
    TransportFailure,
    InvalidDocument,
    MajorMismatch,
    MinorTooOld,
    MissingProductionContract,
    MissingCapability
};

/** @brief Validated worker identity returned by --contract-info. */
struct WorkerContractInfo
{
    std::uint32_t major{0};
    std::uint32_t minor{0};
    std::string engineVersion;
    std::vector<std::string> produces;
    std::vector<std::string> capabilities;
};

/** @brief Module requirements applied before a worker job may be launched. */
struct WorkerContractRequirement
{
    std::uint32_t major{1};
    std::uint32_t minor{0};
    std::vector<std::string> requiredProduces{"p0.rgbwsv.2"};
    std::vector<std::string> requiredCapabilities;
};

/** @brief Complete discovery transport and compatibility result. */
struct WorkerContractResult
{
    bool compatible{false};
    WorkerContractDecision decision{WorkerContractDecision::TransportFailure};
    std::string errorCode;
    std::string errorMessage;
    WorkerContractInfo info;
    WorkerRunResult transport;
};

/** @brief Executes and validates the private file_contract_v1 discovery handshake. */
class WorkerContractNegotiator final
{
public:
    /**
     * @brief Binds negotiation to an existing WorkerClient process owner.
     * @param client Client that preserves Stage 14D-02 timeout and process-tree semantics.
     */
    explicit WorkerContractNegotiator(WorkerClient& client) noexcept;

    /**
     * @brief Runs --contract-info and applies fail-closed compatibility rules.
     * @param workerExecutable Absolute path to slicer_worker.exe or a test worker.
     * @param requirement Required major/minor, production contracts, and capabilities.
     * @return Validated worker information or a stable rejection diagnostic.
     */
    [[nodiscard]] WorkerContractResult Negotiate(
        const std::filesystem::path& workerExecutable,
        const WorkerContractRequirement& requirement) const;

private:
    WorkerClient& m_client;
};

}  // namespace slicesoft::module
