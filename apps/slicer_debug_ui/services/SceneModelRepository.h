#pragma once

#include "slicer_core/scene/SceneModel.h"
#include "slicer_core/texture_image.h"

#include <QString>

#include <map>
#include <memory>
#include <mutex>
#include <optional>

/**
 * @brief Immutable source model and identities retained for scene reprojection.
 */
struct SceneModelRepositoryEntry
{
    QString cachekey;
    QString modelpath;
    QString sourcetransformidentity;
    QString sourcehash;
    QString resourcehash;
    slicer_core::TextureSampleOptions textureoptions;
    std::shared_ptr<const slicer_core::SceneModel> model;
};

/**
 * @brief Thread-safe repository for immutable imported scene models.
 */
class SceneModelRepository final
{
public:
    /**
     * @brief Store or replace an immutable source entry.
     * @param entry Source identities and shared immutable model.
     * @return True when the entry is complete and was stored.
     */
    bool Store(const SceneModelRepositoryEntry& entry);

    /**
     * @brief Find one immutable source entry.
     * @param cacheKey Stable source cache identity.
     * @return Entry when present.
     */
    std::optional<SceneModelRepositoryEntry> Find(
        const QString& cacheKey) const;

    /**
     * @brief Remove every cached source.
     */
    void Clear();

    /**
     * @brief Return the number of immutable source entries.
     * @return Repository entry count.
     */
    std::size_t Size() const;

private:
    mutable std::mutex m_mutex;
    std::map<QString, SceneModelRepositoryEntry> m_entries;
};
