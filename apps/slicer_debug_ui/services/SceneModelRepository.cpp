#include "SceneModelRepository.h"

bool SceneModelRepository::Store(
    const SceneModelRepositoryEntry& entry)
{
    if (entry.cachekey.trimmed().isEmpty()
        || entry.modelpath.trimmed().isEmpty()
        || entry.sourcetransformidentity.trimmed().isEmpty()
        || entry.sourcehash.trimmed().isEmpty()
        || entry.resourcehash.trimmed().isEmpty()
        || entry.model == nullptr)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries[entry.cachekey] = entry;
    return true;
}

std::optional<SceneModelRepositoryEntry> SceneModelRepository::Find(
    const QString& cacheKey) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto found = m_entries.find(cacheKey);
    if (found == m_entries.end())
    {
        return std::nullopt;
    }
    return found->second;
}

void SceneModelRepository::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

std::size_t SceneModelRepository::Size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}
