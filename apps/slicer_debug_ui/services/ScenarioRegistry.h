#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief Metadata and execution paths for one UI slicing Profile or test scenario.
 */
struct ScenarioEntry
{
    QString id;
    QString name;
    QString displayname;
    QString category;
    QString audience;
    QString visibility;
    QStringList inputformats;
    QStringList materialcapabilities;
    QString productionsafety;
    QString docpath;
    QString configpath;
    QString packagedir;
    QString description;
    bool enabled{true};
    bool experimental{false};
    bool requiresopenvdb{false};
    bool ripsummary{true};
};

class ScenarioRegistry final
{
public:
    /**
     * @brief Load the slicer scenario registry from the repository.
     * @param repoRoot Absolute repository root used to resolve the registry file.
     * @param relativePath Registry path relative to repoRoot.
     * @return true when the registry JSON was parsed successfully.
     */
    bool Load(const QString& repoRoot, const QString& relativePath = QStringLiteral("samples/scenarios/slicer_scenarios.json"));

    /**
     * @brief Return all scenarios declared by the registry.
     * @return Scenario entries in file order.
     */
    const QVector<ScenarioEntry>& Entries() const;

    /**
     * @brief Return the default scenario id declared by the registry.
     * @return Scenario id, or an empty string when the file has no default.
     */
    QString DefaultScenarioId() const;

    /**
     * @brief Find a scenario by id.
     * @param id Scenario id from the registry.
     * @return Pointer to the matching scenario, or nullptr when absent.
     */
    const ScenarioEntry* FindById(const QString& id) const;

    /**
     * @brief Return parse and validation warnings from the last load.
     * @return Warning messages suitable for UI display.
     */
    QStringList Warnings() const;

private:
    QVector<ScenarioEntry> m_entries;
    QString m_defaultScenarioId;
    QStringList m_warnings;
};
