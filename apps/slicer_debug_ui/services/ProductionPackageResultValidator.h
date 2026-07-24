#pragma once

#include "PackageLoader.h"
#include "ProductionModeCatalog.h"
#include "ProductionSliceRunSession.h"

#include <cstdint>
#include <optional>
#include <QStringList>

/**
 * @brief Inputs used to validate and present one completed production package.
 */
struct ProductionPackageResultRequest
{
    ProductionSliceRunRequest runrequest;
    PackageSummary package;
    std::optional<double> measuredtotalms;
    std::optional<std::uint64_t> measuredpeakworkingsetbytes;
};

/**
 * @brief Fail-closed package validation result for the production UI.
 */
struct ProductionPackageResult
{
    bool valid{false};
    ProductionModeUiDto presentation;
    QStringList errors;
};

/**
 * @brief Validate package identity, production mode, and preview/report provenance.
 */
class ProductionPackageResultValidator final
{
public:
    /**
     * @brief Validate one package against the exact process request that produced it.
     * @param request Completed process identity, loaded package, and measured resources.
     * @return Presentation data only when all production identity checks pass.
     */
    ProductionPackageResult Validate(
        const ProductionPackageResultRequest& request) const;
};
