#pragma once

#include "../services/ConfigDocument.h"

#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QWidget>

class SupportEditor final : public QWidget {
    Q_OBJECT

public:
    explicit SupportEditor(ConfigDocument* document, QWidget* parent = nullptr);
    void loadFromDocument();

private:
    ConfigDocument* document_{nullptr};
    bool loading_{false};
    QCheckBox* enabled_{nullptr};
    QComboBox* mode_{nullptr};
    QSpinBox* min_island_area_{nullptr};
    QSpinBox* xy_dilation_{nullptr};
    QSpinBox* connectivity_{nullptr};
};
