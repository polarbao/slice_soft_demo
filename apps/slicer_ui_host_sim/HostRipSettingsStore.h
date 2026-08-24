#pragma once

#include <QString>

class QSettings;

/** @brief Host-owned settings for the isolated external RIP adapter. */
struct hostripsettings
{
    bool autoafterslice{false};
    int renderintent{0};
    QString transparentmode{QStringLiteral("follow_manifest")};
    int colormode{0};
    QString inputicc{QStringLiteral("CmykFiles/CIERGB.icc")};
    QString outputicc{QStringLiteral("CmykFiles/CMYK.icc")};
    bool continueonerror{false};
    int devicegraybits{2};
    int timeoutseconds{3600};
    QString outputvalidationmode{QStringLiteral("strict_s2")};
    QString outputdirectoryname{QStringLiteral("rip")};
    QString existingoutputpolicy{QStringLiteral("fail_closed")};
};
/** @brief Versioned persistence isolated from the slicing workspace schema. */
class HostRipSettingsStore final
{
public:
    [[nodiscard]] static hostripsettings Defaults();
    [[nodiscard]] static bool Validate(
        const hostripsettings& settings,
        QString* error = nullptr);
    [[nodiscard]] static bool Load(
        QSettings& settings,
        hostripsettings* value,
        QString* error = nullptr);
    [[nodiscard]] static bool Save(
        QSettings& settings,
        const hostripsettings& value,
        QString* error = nullptr);
    [[nodiscard]] static bool IsDiagnosticMode(
        const hostripsettings& settings);
    [[nodiscard]] static QString EffectiveOutputDirectoryName(
        const hostripsettings& settings);
};
