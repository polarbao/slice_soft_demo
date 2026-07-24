#pragma once

#include "slicer_core/config/SlicePipelineConfig.h"

#include <QJsonDocument>
#include <QString>
#include <QStringList>

/**
 * @brief Inputs used to resolve the read-only source Profile for one production run.
 */
struct ProductionProfileSourceRequest
{
    QString reporoot;
    slicer_core::SlicePipelineMode mode{
        slicer_core::SlicePipelineMode::Legacy};
    QString requestedprofileid;
    QString legacytemplatepath;
    QJsonDocument legacyoriginaldocument;
    QJsonDocument legacyoverridedocument;
};

/**
 * @brief Read-only Profile source and allowed UI override projection.
 */
struct ProductionProfileSourceResult
{
    QString profileid;
    QString templatepath;
    QJsonDocument originaldocument;
    QJsonDocument overridedocument;
    QStringList errors;

    /**
     * @brief Return whether a valid source Profile was resolved.
     * @return true when documents and source identity are valid.
     */
    bool IsValid() const;
};

/**
 * @brief Resolve Legacy pass-through or catalog-locked Global source Profiles.
 */
class ProductionProfileSourceResolver final
{
public:
    /**
     * @brief Resolve a production source without modifying repository fixtures.
     * @param request Current Legacy document and explicit product selection.
     * @return Read-only source and session override projection.
     */
    ProductionProfileSourceResult Resolve(
        const ProductionProfileSourceRequest& request) const;
};
