#pragma once

#include "../services/ConfigDocument.h"

#include <QCheckBox>
#include <QComboBox>
#include <QTableWidget>
#include <QWidget>

class MaterialRoleMappingEditor final : public QWidget {
    Q_OBJECT

public:
    explicit MaterialRoleMappingEditor(ConfigDocument* document, QWidget* parent = nullptr);
    void loadFromDocument();

private slots:
    void addRule();
    void writeRules();

private:
    QComboBox* createRoleCombo(const QString& role);
    void addRuleRow(const QString& match, const QString& role);

    ConfigDocument* document_{nullptr};
    bool loading_{false};
    QCheckBox* enabled_{nullptr};
    QComboBox* default_role_{nullptr};
    QCheckBox* allow_input_support_{nullptr};
    QTableWidget* rules_{nullptr};
};
