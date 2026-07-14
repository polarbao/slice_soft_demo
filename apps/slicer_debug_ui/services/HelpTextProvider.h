#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief Chinese help metadata for one slicing workbench setting.
 */
struct SettingHelpMetadata
{
    QString key;
    QString title;
    QString description;
    QStringList affects;
    QString defaultvalue;
    QString productionsafety;
    QString docpath;

    /**
     * @brief Determine whether all mandatory metadata fields are present.
     * @return true when the metadata can be shown in production UI.
     */
    bool IsComplete() const;

    /**
     * @brief Build the concise text used by control tooltips.
     * @return Chinese tooltip containing impact, default, safety, and document data.
     */
    QString ToolTipText() const;

    /**
     * @brief Build the full text used by the setting help panel.
     * @return Multi-line Chinese setting description.
     */
    QString DetailText() const;
};

/**
 * @brief Provides the single source of truth for workbench setting help text.
 */
class HelpTextProvider final
{
public:
    /**
     * @brief Return all registered setting metadata in UI display order.
     * @return Stable metadata collection owned by the provider.
     */
    static const QVector<SettingHelpMetadata>& All();

    /**
     * @brief Find setting metadata by stable key.
     * @param key Setting key such as modelFill.material.
     * @return Metadata pointer, or nullptr when the key is unknown.
     */
    static const SettingHelpMetadata* Find(const QString& key);

    /**
     * @brief Return a tooltip generated from registered metadata.
     * @param key Stable setting key.
     * @return Tooltip text, or an empty string when the key is unknown.
     */
    static QString ToolTip(const QString& key);
};
