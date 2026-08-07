#pragma once

#include "../camera/ViewModeSwitch.h"
#include "../render/IRenderBackend.h"

#include <QString>

/**
 * @brief Persists display-only view preferences in a session config file.
 */
class ViewPresentationSettings final
{
public:
    /**
     * @brief Creates settings bound to one session config path.
     * @param sessionConfigPath JSON file used for round-trip persistence.
     */
    explicit ViewPresentationSettings(const QString& sessionConfigPath);

    /** @brief Loads settings or contract defaults when the file is absent. */
    bool Load(QString* error);

    /** @brief Atomically saves the current display preferences. */
    bool Save(QString* error) const;

    /** @brief Returns the configured default presentation mode. */
    [[nodiscard]] HostViewMode DefaultViewMode() const;

    /** @brief Updates the configured default presentation mode. */
    void SetDefaultViewMode(HostViewMode mode);

    /** @brief Returns the configured three-dimensional projection. */
    [[nodiscard]] slicer::render::Projection ThreeDProjection() const;

    /** @brief Updates the three-dimensional projection preference. */
    void SetThreeDProjection(slicer::render::Projection projection);

    /** @brief Returns the bound session config path. */
    [[nodiscard]] QString SessionConfigPath() const;

private:
    QString m_sessionConfigPath;
    HostViewMode m_defaultViewMode{HostViewMode::Top};
    slicer::render::Projection m_threeDProjection{
        slicer::render::Projection::Orthographic};
};
