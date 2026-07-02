#include "ScenarioRegistry.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{

bool ReadBool(const QJsonObject& object, const QString& key, const bool fallback)
{
    const QJsonValue value = object.value(key);
    return value.isBool() ? value.toBool() : fallback;
}

QString ReadString(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : QString{};
}

}  // namespace

bool ScenarioRegistry::Load(const QString& repoRoot, const QString& relativePath)
{
    m_entries.clear();
    m_defaultScenarioId.clear();
    m_warnings.clear();

    const QString registryPath = QDir(repoRoot).filePath(relativePath);
    QFile file(registryPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_warnings.push_back(QStringLiteral("场景索引不存在或无法读取：") + registryPath);
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        m_warnings.push_back(QStringLiteral("场景索引 JSON 解析失败：") + parseError.errorString());
        return false;
    }

    const QJsonObject root = document.object();
    const QString schema = ReadString(root, QStringLiteral("schema"));
    if (schema != QStringLiteral("slice_soft.scenarios.1"))
    {
        m_warnings.push_back(QStringLiteral("场景索引 schema 非预期：") + schema);
    }

    m_defaultScenarioId = ReadString(root, QStringLiteral("defaultScenarioId"));

    const QJsonArray scenarios = root.value(QStringLiteral("scenarios")).toArray();
    for (const QJsonValue& item : scenarios)
    {
        if (!item.isObject())
        {
            m_warnings.push_back(QStringLiteral("跳过非 object 场景条目。"));
            continue;
        }

        const QJsonObject object = item.toObject();
        ScenarioEntry entry;
        entry.id = ReadString(object, QStringLiteral("id"));
        entry.name = ReadString(object, QStringLiteral("name"));
        entry.category = ReadString(object, QStringLiteral("category"));
        entry.configpath = ReadString(object, QStringLiteral("configPath"));
        entry.packagedir = ReadString(object, QStringLiteral("packageDir"));
        entry.description = ReadString(object, QStringLiteral("description"));
        entry.enabled = ReadBool(object, QStringLiteral("enabled"), true);
        entry.experimental = ReadBool(object, QStringLiteral("experimental"), false);
        entry.requiresopenvdb = ReadBool(object, QStringLiteral("requiresOpenVdb"), false);
        entry.ripsummary = ReadBool(object, QStringLiteral("ripSummary"), true);

        if (entry.id.isEmpty() || entry.name.isEmpty() || entry.configpath.isEmpty())
        {
            m_warnings.push_back(QStringLiteral("跳过缺少 id/name/configPath 的场景。"));
            continue;
        }

        m_entries.push_back(entry);
    }

    if (m_entries.isEmpty())
    {
        m_warnings.push_back(QStringLiteral("场景索引未提供任何可用场景。"));
    }

    return true;
}

const QVector<ScenarioEntry>& ScenarioRegistry::Entries() const
{
    return m_entries;
}

QString ScenarioRegistry::DefaultScenarioId() const
{
    return m_defaultScenarioId;
}

const ScenarioEntry* ScenarioRegistry::FindById(const QString& id) const
{
    for (const ScenarioEntry& entry : m_entries)
    {
        if (entry.id == id)
        {
            return &entry;
        }
    }
    return nullptr;
}

QStringList ScenarioRegistry::Warnings() const
{
    return m_warnings;
}
