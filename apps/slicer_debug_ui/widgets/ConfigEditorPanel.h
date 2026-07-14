#pragma once

#include "../services/ConfigDocument.h"
#include "../services/EffectiveConfigGenerator.h"

#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QWidget>

class MaterialPolicyEditor;
class MaterialProcessProfileEditor;
class MaterialRoleMappingEditor;
class SupportEditor;
class ConfigDiffPanel;
class QuickConfigPanel;
class SettingHelpPanel;

class ConfigEditorPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ConfigEditorPanel(ConfigDocument* document, QWidget* parent = nullptr);
    bool loadConfig(const QString& path);
    QString configPath() const;

    /**
     * @brief Display the latest generated effective config summary and differences.
     * @param result Successful or failed effective config generation result.
     */
    void ShowEffectiveConfig(const EffectiveConfigResult& result);

    /**
     * @brief Return the text currently shown in the effective config view.
     * @return Summary, validation diagnostics, and Profile-to-effective differences.
     */
    QString EffectiveConfigText() const;

signals:
    void configPathChanged(const QString& path);
    void statusMessage(const QString& message);

private slots:
    void save();
    void saveAs();
    void revert();
    void validate();
    void updateDirty(bool dirty);
    void updateValidation(const QStringList& warnings, const QStringList& errors);
    void updateStorageMode(int index);

private:
    void refreshEditors();

    ConfigDocument* document_{nullptr};
    QLabel* path_label_{nullptr};
    QLabel* dirty_label_{nullptr};
    QComboBox* storage_mode_{nullptr};
    QPlainTextEdit* validation_view_{nullptr};
    QPlainTextEdit* m_effectiveConfigView{nullptr};
    SettingHelpPanel* m_settingHelpPanel{nullptr};
    MaterialProcessProfileEditor* profile_editor_{nullptr};
    QuickConfigPanel* quick_config_panel_{nullptr};
    MaterialPolicyEditor* policy_editor_{nullptr};
    MaterialRoleMappingEditor* role_mapping_editor_{nullptr};
    SupportEditor* support_editor_{nullptr};
    ConfigDiffPanel* diff_panel_{nullptr};
};
