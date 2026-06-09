#pragma once

#include "../services/ConfigDocument.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>

class MaterialProcessProfileEditor final : public QWidget {
    Q_OBJECT

public:
    explicit MaterialProcessProfileEditor(ConfigDocument* document, QWidget* parent = nullptr);
    void loadFromDocument();

private:
    void bind();
    void setString(const QStringList& path, QLineEdit* edit);
    void setBool(const QStringList& path, QCheckBox* check);
    void setInt(const QStringList& path, QSpinBox* spin);
    void setCombo(const QStringList& path, QComboBox* combo);

    ConfigDocument* document_{nullptr};
    bool loading_{false};

    QCheckBox* enabled_{nullptr};
    QLineEdit* name_{nullptr};
    QLineEdit* target_{nullptr};
    QCheckBox* rgb_enabled_{nullptr};
    QCheckBox* white_enabled_{nullptr};
    QLineEdit* white_coverage_{nullptr};
    QSpinBox* white_expand_{nullptr};
    QSpinBox* white_shrink_{nullptr};
    QCheckBox* varnish_enabled_{nullptr};
    QSpinBox* varnish_top_layers_{nullptr};
    QCheckBox* support_expected_{nullptr};
    QCheckBox* require_rgb_{nullptr};
    QCheckBox* require_white_{nullptr};
    QCheckBox* require_varnish_{nullptr};
    QCheckBox* require_support_{nullptr};
};
