#pragma once

#include "../services/ConfigDocument.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>

class MaterialPolicyEditor final : public QWidget {
    Q_OBJECT

public:
    explicit MaterialPolicyEditor(ConfigDocument* document, QWidget* parent = nullptr);
    void loadFromDocument();

private:
    void bind();
    void setString(const QStringList& path, QLineEdit* edit);
    void setBool(const QStringList& path, QCheckBox* check);
    void setInt(const QStringList& path, QSpinBox* spin);
    void setCombo(const QStringList& path, QComboBox* combo, const QString& fallback = QString());

    ConfigDocument* document_{nullptr};
    bool loading_{false};

    QCheckBox* enabled_{nullptr};
    QCheckBox* rgb_enabled_{nullptr};
    QComboBox* rgb_source_{nullptr};
    QCheckBox* white_enabled_{nullptr};
    QComboBox* white_mode_{nullptr};
    QLineEdit* white_layers_{nullptr};
    QSpinBox* white_value_{nullptr};
    QCheckBox* varnish_enabled_{nullptr};
    QComboBox* varnish_mode_{nullptr};
    QSpinBox* varnish_top_layers_{nullptr};
    QSpinBox* varnish_value_{nullptr};
    QComboBox* conflict_policy_{nullptr};
};
