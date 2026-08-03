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
class ProductionModePanel;
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

    /**
     * @brief Return the product mode selected on the ordinary configuration page.
     * @return Legacy by default or explicitly selected Global Surface Shell.
     */
    slicer_core::SlicePipelineMode SelectedProductionMode() const;

    /**
     * @brief Return the explicitly selected Global Production Profile.
     * @return Empty for Legacy.
     */
    QString SelectedProductionProfileId() const;

    /**
     * @brief Show a fail-closed production admission state.
     * @param state Current state.
     * @param detail Chinese state or blocking detail.
     */
    void ShowProductionAdmissionState(
        ProductionAdmissionState state,
        const QString& detail);

    /**
     * @brief Show the validated production package result in the mode panel.
     * @param result Current session production result.
     */
    void ShowProductionResult(const ProductionModeUiDto& result);

    /**
     * @brief Clear the previous production result before starting a new run.
     */
    void ClearProductionResult();

signals:
    void configPathChanged(const QString& path);
    void statusMessage(const QString& message);

    /**
     * @brief Emitted after product mode or Global Profile selection changes.
     */
    void SigProductionSelectionChanged();

private slots:
    void save();
    void saveAs();
    void revert();
    void validate();
    void updateDirty(bool dirty);
    void updateValidation(const QStringList& warnings, const QStringList& errors);
    void updateStorageMode(int index);
    void OnTiffCompressionChanged(int index);

private:
    void refreshEditors();

    ConfigDocument* document_{nullptr};
    QLabel* path_label_{nullptr};
    QLabel* dirty_label_{nullptr};
    QComboBox* storage_mode_{nullptr};
    QComboBox* m_tiffCompression{nullptr};
    QPlainTextEdit* validation_view_{nullptr};
    QPlainTextEdit* m_effectiveConfigView{nullptr};
    SettingHelpPanel* m_settingHelpPanel{nullptr};
    MaterialProcessProfileEditor* profile_editor_{nullptr};
    QuickConfigPanel* quick_config_panel_{nullptr};
    ProductionModePanel* m_productionModePanel{nullptr};
    MaterialPolicyEditor* policy_editor_{nullptr};
    MaterialRoleMappingEditor* role_mapping_editor_{nullptr};
    SupportEditor* support_editor_{nullptr};
    ConfigDiffPanel* diff_panel_{nullptr};
};
