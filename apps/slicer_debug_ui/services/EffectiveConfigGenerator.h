#pragma once

#include "ConfigDiffModel.h"
#include "ProductionModeCatalog.h"
#include "SliceSettingsModel.h"
#include "TextureWhitePreflightService.h"

#include <QJsonDocument>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

/**
 * @brief Production mode, Profile, and session audit requested by the Qt UI.
 */
struct ProductionEffectiveConfigSelection
{
    slicer_core::SlicePipelineMode requestedmode{
        slicer_core::SlicePipelineMode::Legacy};
    QString requestedprofileid;
    QString sourceprofileid;
    QString sessionid;
    QString generatedatutc;
};

/**
 * @brief Input required to create one session-scoped effective configuration.
 */
struct EffectiveConfigRequest
{
    QString profileid;
    QString templatepath;
    QString generatedconfigpath;
    QJsonDocument originaldocument;
    QJsonDocument overridedocument;
    SliceSettingsState settings;
    ProductionEffectiveConfigSelection production;
    QString sceneid;
    quint64 scenerevision{0U};
    QString scenecontenthash;
    QStringList profilecapabilities;
    std::optional<TextureWhitePreflightResult> texturewhitepreflight;
};

/**
 * @brief Generated configuration, validation diagnostics, and UI-facing audit data.
 */
struct EffectiveConfigResult
{
    QString generatedconfigpath;
    QJsonDocument document;
    QVector<ConfigDiffEntry> differences;
    QString summary;
    QStringList warnings;
    QStringList errors;

    /**
     * @brief Determine whether generation and validation completed successfully.
     * @return true when no blocking errors exist.
     */
    bool IsValid() const;
};

/**
 * @brief Maps Profile settings and in-memory UI overrides to a validated session config.
 */
class EffectiveConfigGenerator final
{
public:
    /**
     * @brief Generate and atomically write one effective configuration.
     * @param request Template documents, UI settings, and session output path.
     * @return Generated document, differences, summary, warnings, and errors.
     */
    EffectiveConfigResult Generate(const EffectiveConfigRequest& request) const;
};
