#pragma once

#include "../services/ConfigDocument.h"

#include <QLabel>
#include <QPlainTextEdit>
#include <QWidget>

class MaterialPolicyEditor;
class MaterialProcessProfileEditor;
class MaterialRoleMappingEditor;
class SupportEditor;

class ConfigEditorPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ConfigEditorPanel(ConfigDocument* document, QWidget* parent = nullptr);
    bool loadConfig(const QString& path);
    QString configPath() const;

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

private:
    void refreshEditors();

    ConfigDocument* document_{nullptr};
    QLabel* path_label_{nullptr};
    QLabel* dirty_label_{nullptr};
    QPlainTextEdit* validation_view_{nullptr};
    MaterialProcessProfileEditor* profile_editor_{nullptr};
    MaterialPolicyEditor* policy_editor_{nullptr};
    MaterialRoleMappingEditor* role_mapping_editor_{nullptr};
    SupportEditor* support_editor_{nullptr};
};
