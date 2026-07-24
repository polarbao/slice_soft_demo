#pragma once

#include "slicer_core/config/SlicePipelineConfig.h"

#include <optional>
#include <QString>
#include <QStringList>

/**
 * @brief Immutable identity of one requested production process.
 */
struct ProductionSliceRunRequest
{
    slicer_core::SlicePipelineMode mode{
        slicer_core::SlicePipelineMode::Legacy};
    QString profileid;
    QString sessionid;
    QString configpath;
    QString packagedir;
};

/**
 * @brief Fail-closed process result used to decide whether a package may be loaded.
 */
struct ProductionSliceRunCompletion
{
    bool success{false};
    bool fallbackapplied{false};
    QString packagedirtoload;
    std::optional<ProductionSliceRunRequest> request;
};

/**
 * @brief Track one production process and prevent stale package reuse or fallback.
 */
class ProductionSliceRunSession final
{
public:
    /**
     * @brief Begin tracking one validated production process request.
     * @param request Exact session identity and requested product mode.
     * @return Empty list on success; blocking validation errors otherwise.
     */
    QStringList Begin(const ProductionSliceRunRequest& request);

    /**
     * @brief Complete the active process and consume its identity.
     * @param exitCode Process exit code.
     * @return Package path only for a successful current process.
     */
    ProductionSliceRunCompletion Complete(int exitCode);

    /**
     * @brief Invalidate any pending process identity after block, stale input, or launch failure.
     */
    void Invalidate();

    /**
     * @brief Return whether a current production process identity is active.
     * @return true between successful Begin and Complete/Invalidate.
     */
    bool IsActive() const;

private:
    std::optional<ProductionSliceRunRequest> m_activeRequest;
};
