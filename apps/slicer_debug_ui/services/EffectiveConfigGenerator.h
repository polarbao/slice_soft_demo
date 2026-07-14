#pragma once

#include "ConfigDiffModel.h"
#include "SliceSettingsModel.h"

#include <QJsonDocument>
#include <QString>
#include <QStringList>
#include <QVector>

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
